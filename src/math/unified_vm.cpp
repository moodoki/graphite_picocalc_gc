#include <cmath>
#include <cstring>

#include "math/catalog.hpp"
#include "math/engine.hpp"
#include "math/unified_eval.hpp"

// Stack machine for the unified evaluator (Phase 5.2, task 5.2.4).
//
// Executes the flat RPN program the shunting-yard compiler emits. There is no
// evaluation recursion: depth is the operand stack, a sized array in bss, so
// over-deep input is a reported error rather than the hard fault matexpr's
// depth-3 cap exists to prevent (D48).
//
// Scope as of 5.2.4: real and complex scalars — arithmetic, unary negation,
// powers, variables, catalog constants, and calls to catalog functions. Matrix
// and list operands are recognised and rejected here; their tiers are 5.2.6 and
// 5.2.7.
namespace math::unified {

namespace {

// The operand stack, in bss for the same reason the compiler's operator stack
// is (unified_compile.cpp): 64 x 24 = 1,536 B is not something to put on a
// 4 KB core-0 stack. Non-reentrant, like the rest of this evaluator.
Value g_stack[kMaxStack];

// Complex counterparts, by catalog name. Where one exists the call is
// dispatched on the argument's Kind; elsewhere a non-real argument is an error
// rather than a silent truncation.
//
// The trig entries MUST be the angle-mode-scaling wrappers, not complex.cpp's
// pure c_sin/c_cos/... That is D46: without the scaling the complex evaluator
// answered every trig call in radians, so DEGREE mode was silently ignored
// whenever Number mode was not REAL, and sin(30) disagreed between the two
// evaluators. Scaling the whole complex argument (TI-89's behaviour) reduces
// exactly to the real path for a real input, which is the property that
// matters.
constexpr calc_t kDegPerRad = 180.0;
constexpr calc_t kPiConst = 3.14159265358979323846;

Complex to_radians(const Complex& z) {
    return angle_mode() == AngleMode::kDegrees ? z * Complex(kPiConst / kDegPerRad) : z;
}
Complex from_radians(const Complex& z) {
    return angle_mode() == AngleMode::kDegrees ? z * Complex(kDegPerRad / kPiConst) : z;
}

Complex m_sin(const Complex& z) {
    return c_sin(to_radians(z));
}
Complex m_cos(const Complex& z) {
    return c_cos(to_radians(z));
}
Complex m_tan(const Complex& z) {
    return c_tan(to_radians(z));
}
Complex m_asin(const Complex& z) {
    return from_radians(c_asin(z));
}
Complex m_acos(const Complex& z) {
    return from_radians(c_acos(z));
}
Complex m_atan(const Complex& z) {
    return from_radians(c_atan(z));
}

struct CFn {
    const char* name;
    Complex (*fn)(const Complex&);
};

const CFn kComplexFns[] = {
    {"exp", c_exp}, {"ln", c_ln},     {"sin", m_sin},   {"cos", m_cos},
    {"tan", m_tan}, {"asin", m_asin}, {"acos", m_acos}, {"atan", m_atan},
};

Complex (*complex_variant(const char* name))(const Complex&) {
    for (const auto& e : kComplexFns) {
        if (std::strcmp(e.name, name) == 0) {
            return e.fn;
        }
    }
    return nullptr;
}

struct Machine {
    const Program* p = nullptr;
    Value* st = g_stack;
    int n = 0;
    const char* err = nullptr;

    bool fail(const char* msg) {
        if (err == nullptr) {
            err = msg;
        }
        return false;
    }

    bool push(const Value& v) {
        if (n >= kMaxStack) {
            return fail("Expression too complex");
        }
        st[n++] = v;
        return true;
    }

    bool pop(Value* out) {
        if (n <= 0) {
            return fail("Syntax error");
        }
        *out = st[--n];
        return true;
    }

    // Arithmetic promotes to complex only when an operand already is. Keeping
    // the all-real case on plain doubles matters for more than speed: c_pow
    // routes real bases through std::pow precisely so the two evaluators agree
    // about ordinary real arithmetic (D46, D49).
    bool binary(Op op) {
        Value b;
        Value a;
        if (!pop(&b) || !pop(&a)) {
            return false;
        }
        if (a.is_array() || b.is_array()) {
            return fail("Matrix not allowed here");  // tiers 5.2.6/5.2.7
        }
        if (a.kind == Kind::kReal && b.kind == Kind::kReal) {
            calc_t v = 0;
            switch (op) {
                case Op::kAdd:
                    v = a.r + b.r;
                    break;
                case Op::kSub:
                    v = a.r - b.r;
                    break;
                case Op::kMul:
                    v = a.r * b.r;
                    break;
                case Op::kDiv:
                    v = a.r / b.r;
                    break;
                case Op::kPow: {
                    // Through c_pow so the real fast path and the D49 integer
                    // rule are applied in exactly one place.
                    const Complex z = c_pow(Complex(a.r), Complex(b.r));
                    return push(z.im == 0.0 ? Value::real(z.re) : Value::complex(z));
                }
                default:
                    return fail("Syntax error");
            }
            return push(Value::real(v));
        }
        const Complex x = a.as_complex();
        const Complex y = b.as_complex();
        Complex v;
        switch (op) {
            case Op::kAdd:
                v = x + y;
                break;
            case Op::kSub:
                v = x - y;
                break;
            case Op::kMul:
                v = x * y;
                break;
            case Op::kDiv:
                v = x / y;
                break;
            case Op::kPow:
                v = c_pow(x, y);
                break;
            default:
                return fail("Syntax error");
        }
        // A complex operation that lands exactly on the real axis reports as
        // real, so `i^2` is -1 rather than -1+0i. Exactness is the test, not a
        // tolerance — D49 made the values exact so this can be.
        return push(v.im == 0.0 ? Value::real(v.re) : Value::complex(v));
    }

    bool call(const Instr& in) {
        int n_cat = 0;
        const FnDescriptor* cat = catalog(&n_cat);
        if (in.b >= static_cast<uint16_t>(n_cat)) {
            return fail("Syntax error");
        }
        const FnDescriptor& d = cat[in.b];
        const int argc = in.a;

        Value args[4];
        if (argc > 4) {
            return fail("Expression too complex");
        }
        for (int i = argc - 1; i >= 0; --i) {
            if (!pop(&args[i])) {
                return false;
            }
        }
        for (int i = 0; i < argc; ++i) {
            if (args[i].is_array()) {
                return fail("Matrix not allowed here");  // 5.2.6/5.2.7
            }
        }

        // Complex argument: use the complex counterpart when one exists.
        if (argc == 1 && args[0].kind == Kind::kComplex) {
            auto* cf = complex_variant(d.name);
            if (cf == nullptr) {
                return fail("Non-real result");
            }
            const Complex v = cf(args[0].as_complex());
            return push(v.im == 0.0 ? Value::real(v.re) : Value::complex(v));
        }

        // Help-only rows (fn == nullptr) are the list and matrix functions,
        // which land with their tiers.
        if (d.fn == nullptr) {
            return fail("Syntax error");
        }
        if (d.arity != argc) {
            return fail("Syntax error");
        }

        // The catalog stores plain double-taking function pointers behind a
        // const void*, exactly as engine.cpp:194-200 hands them to tinyexpr.
        // Absorbing the catalogue is this dispatch, not sixty ports.
        const double a0 = argc > 0 ? args[0].r : 0.0;
        const double a1 = argc > 1 ? args[1].r : 0.0;
        const double a2 = argc > 2 ? args[2].r : 0.0;
        const double a3 = argc > 3 ? args[3].r : 0.0;
        double v = 0.0;
        switch (argc) {
            case 0:
                v = reinterpret_cast<double (*)()>(d.fn)();
                break;
            case 1:
                v = reinterpret_cast<double (*)(double)>(d.fn)(a0);
                break;
            case 2:
                v = reinterpret_cast<double (*)(double, double)>(d.fn)(a0, a1);
                break;
            case 3:
                v = reinterpret_cast<double (*)(double, double, double)>(d.fn)(a0, a1, a2);
                break;
            default:
                v = reinterpret_cast<double (*)(double, double, double, double)>(d.fn)(a0, a1, a2,
                                                                                       a3);
                break;
        }
        return push(Value::real(v));
    }

    bool step(const Instr& in) {
        switch (in.op) {
            case Op::kPushConst: {
                if (in.b >= static_cast<uint16_t>(p->n_consts)) {
                    return fail("Syntax error");
                }
                const Complex& k = p->consts[in.b];
                return push(k.im == 0.0 ? Value::real(k.re) : Value::complex(k));
            }
            case Op::kPushVar: {
                auto& vars = engine().vars();
                const int idx = in.a;
                if (idx < 0 || idx >= Variables::kCount) {
                    return fail("Syntax error");
                }
                // is_complex() is why kPushVar uses Variables' own slot
                // indices: the complex-variable read that refs_complex_var
                // works around today is just a flag here.
                if (vars.is_complex(idx)) {
                    return push(Value::complex(Complex(vars.vars[idx], vars.imag[idx])));
                }
                return push(Value::real(vars.vars[idx]));
            }
            case Op::kPushSysc: {
                int n_c = 0;
                const ConstDescriptor* cs = constants(&n_c);
                if (in.b >= static_cast<uint16_t>(n_c)) {
                    return fail("Syntax error");
                }
                return push(Value::real(cs[in.b].value));
            }
            case Op::kAdd:
            case Op::kSub:
            case Op::kMul:
            case Op::kDiv:
            case Op::kPow:
                return binary(in.op);
            case Op::kNeg: {
                Value a;
                if (!pop(&a)) {
                    return false;
                }
                if (a.is_array()) {
                    return fail("Matrix not allowed here");  // 5.2.7
                }
                if (a.kind == Kind::kReal) {
                    return push(Value::real(-a.r));
                }
                return push(Value::complex(-a.as_complex()));
            }
            case Op::kCall:
                return call(in);
            default:
                // kPushMat / kPushList / kPushElem / kIndex / kTranspose /
                // kStore arrive with their tiers.
                return fail("Syntax error");
        }
    }
};

}  // namespace

bool run(const Program& p, Value* out, const char** err) {
    if (err != nullptr) {
        *err = nullptr;
    }
    Machine m;
    m.p = &p;
    for (int i = 0; i < p.n_code; ++i) {
        if (!m.step(p.code[i])) {
            if (err != nullptr) {
                *err = m.err != nullptr ? m.err : "Syntax error";
            }
            return false;
        }
    }
    // Exactly one value must remain: fewer means the program was empty, more
    // means the compiler emitted something unbalanced, which is a bug here
    // rather than bad user input.
    if (m.n != 1) {
        if (err != nullptr) {
            *err = "Syntax error";
        }
        return false;
    }
    if (out != nullptr) {
        *out = m.st[0];
    }
    return true;
}

}  // namespace math::unified
