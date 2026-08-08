#include <cmath>
#include <cstring>

#include "math/array.hpp"
#include "math/catalog.hpp"
#include "math/engine.hpp"
#include "math/list_ops.hpp"
#include "math/lists.hpp"
#include "math/named_lists.hpp"
#include "math/scratch.hpp"
#include "math/stats.hpp"
#include "math/unified_eval.hpp"

// Stack machine for the unified evaluator (Phase 5.2, task 5.2.4).
//
// Executes the flat RPN program the shunting-yard compiler emits. There is no
// evaluation recursion: depth is the operand stack, a sized array in bss, so
// over-deep input is a reported error rather than the hard fault matexpr's
// depth-3 cap exists to prevent (D48).
//
// Scope as of 5.2.6: real and complex scalars — arithmetic, unary negation,
// powers, variables, catalog constants, calls to catalog/builtin functions —
// and lists of either. Matrix operands are recognised and rejected here; that
// tier is 5.2.7.
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


// ---- Builtins (5.2.5) ----------------------------------------------------
//
// Everything callable from the home screen that catalog.cpp does not carry.
// Two distinct origins, unified here because a Value-based evaluator has no
// reason to keep them apart:
//
//   * tinyexpr's own builtin table — sqrt, abs, exp, the hyperbolics,
//     ceil/floor/log10/atan2/pow. Reachable today only because every
//     unrecognised scalar span escaped to eval_field.
//   * complex_expr's complex-only kFns — conj, real, imag, arg. These have no
//     real counterpart at all; a real argument is simply the degenerate case.
//
// `cx` is the complex implementation, used when the argument is complex.
// nullptr there means a complex argument is an error rather than a silent
// truncation to the real part.
Complex c_absz(const Complex& z) {
    return {z.modulus(), 0.0};
}
Complex c_argz(const Complex& z) {
    return {z.argument(), 0.0};
}
Complex c_realz(const Complex& z) {
    return {z.re, 0.0};
}
Complex c_imagz(const Complex& z) {
    return {z.im, 0.0};
}

double r_abs(double x) {
    return std::fabs(x);
}
double r_sqrt(double x) {
    return std::sqrt(x);
}
double r_exp(double x) {
    return std::exp(x);
}
double r_sinh(double x) {
    return std::sinh(x);
}
double r_cosh(double x) {
    return std::cosh(x);
}
double r_tanh(double x) {
    return std::tanh(x);
}
double r_ceil(double x) {
    return std::ceil(x);
}
double r_floor(double x) {
    return std::floor(x);
}
double r_log10(double x) {
    return std::log10(x);
}
double r_conj(double x) {
    return x;
}
double r_real(double x) {
    return x;
}
double r_imag(double x) {
    (void)x;  // imag() of a real is always zero
    return 0.0;
}
double r_arg(double x) {
    return x >= 0 ? 0.0 : 3.14159265358979323846;
}
double r_atan2(double y, double x) {
    return std::atan2(y, x);
}
double r_pow(double a, double b) {
    return std::pow(a, b);
}

struct Builtin {
    const char* name;
    int arity;
    double (*r1)(double);
    double (*r2)(double, double);
    Complex (*cx)(const Complex&);
};

const Builtin kBuiltins[] = {
    {"sqrt", 1, r_sqrt, nullptr, c_sqrt},
    {"abs", 1, r_abs, nullptr, c_absz},
    {"exp", 1, r_exp, nullptr, c_exp},
    {"sinh", 1, r_sinh, nullptr, nullptr},
    {"cosh", 1, r_cosh, nullptr, nullptr},
    {"tanh", 1, r_tanh, nullptr, nullptr},
    {"ceil", 1, r_ceil, nullptr, nullptr},
    {"floor", 1, r_floor, nullptr, nullptr},
    {"log10", 1, r_log10, nullptr, nullptr},
    // Complex-only in origin; a real argument is the degenerate case.
    {"conj", 1, r_conj, nullptr, c_conj},
    {"real", 1, r_real, nullptr, c_realz},
    {"imag", 1, r_imag, nullptr, c_imagz},
    {"arg", 1, r_arg, nullptr, c_argz},
    {"atan2", 2, nullptr, r_atan2, nullptr},
    {"pow", 2, nullptr, r_pow, nullptr},
};
constexpr int kBuiltinCount = static_cast<int>(sizeof(kBuiltins) / sizeof(kBuiltins[0]));

// ---- List tier (5.2.6) ---------------------------------------------------
//
// A list operand broadcasts at the instruction that consumes it: the operands
// stream through 256-element chunks and one temporary Array is materialised.
// Every node is therefore evaluated exactly once per element — including
// reductions, which a whole-program-per-element lift would recompute N times
// (see unified_eval.hpp for why the 5.2.1 sketch was revised here).
//
// Note what is NOT here: no textual reduction substitution, no separate
// complex-list grammar, no operand extraction pass. listexpr needs all three
// because it rewrites source text before handing it to tinyexpr; a tagged
// Value needs none of them.

constexpr int kChunk = 256;
constexpr int kMaxBcastArgs = 4;  // catalog's widest arity
constexpr int kOutSlot = kMaxBcastArgs;

// Staging: one 256-element chunk per argument plus one for the result. Sized
// at Complex width so a complex operand needs no second buffer; a real operand
// uses the low half of the same bytes and pays no conversion. Overlays the
// shared compute arena (scratch.hpp), whose invariant this respects — nothing
// reached from inside a chunk loop is a kCompute owner. Checked, not assumed:
// the elementwise body reaches only functions.cpp/dist.cpp (neither touches the
// arena), listops owns the disjoint kListops region, and stats — which does own
// kCompute — is reached only from kCallList, its own instruction, never from
// inside someone else's loop. **5.2.7 must re-check this**: matrix.cpp is a
// kCompute owner too, so a matrix operation called from within a chunk loop
// would break the invariant where a scalar one does not.
Complex (&g_slot)[kMaxBcastArgs + 1][kChunk] =
    *reinterpret_cast<Complex (*)[kMaxBcastArgs + 1][kChunk]>(scratch::compute_region());
static_assert(sizeof(Complex) * (kMaxBcastArgs + 1) * kChunk <= scratch::kComputeBytes,
              "unified list staging exceeds the shared compute region");

calc_t* real_view(int k) {
    return reinterpret_cast<calc_t*>(&g_slot[k][0]);
}

// Result temporaries. Arrays, not buffers: storage comes from the existing
// ArrayStore, so the tier costs bss only for these handles — the same trade
// listexpr's g_op/g_temp/g_result make today, and they are deleted in 5.2.11.
constexpr int kMaxTemps = 6;
Array g_temps[kMaxTemps];
bool g_temp_used[kMaxTemps] = {};

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

// ---- List functions ------------------------------------------------------
//
// The catalogue's help-only rows, given real bindings (spec §2). Order matches
// ListFn. `std` is listexpr's alias for `stdev` and has no catalogue row at
// all, which is one reason this table is consulted before the catalogue rather
// than layered on top of it.
enum class ListFn : uint8_t {
    kSum,
    kProd,
    kLength,
    kMean,
    kMedian,
    kStdev,
    kStd,
    kCumsum,
    kDeltaList,
    kSortAsc,
    kSortDesc,
    kSeq,
    kRange,
};

struct ListFnDesc {
    const char* name;
    uint8_t argc_min;
    uint8_t argc_max;
};

const ListFnDesc kListFnTable[] = {
    {"sum", 1, 1},       {"prod", 1, 1}, {"length", 1, 1}, {"mean", 1, 1},       {"median", 1, 1},
    {"stdev", 1, 1},     {"std", 1, 1},  {"cumsum", 1, 1}, {"delta_list", 1, 1}, {"sort_asc", 1, 1},
    {"sort_desc", 1, 1}, {"seq", 5, 5},  {"range", 2, 3},
};
constexpr int kListFnCount = static_cast<int>(sizeof(kListFnTable) / sizeof(kListFnTable[0]));
static_assert(kListFnCount == static_cast<int>(ListFn::kRange) + 1,
              "kListFnTable and ListFn must stay in step");

// The element count seq/range produce, with listexpr's rules verbatim
// (list_ops.cpp:270) — including the half-step tolerance that makes
// seq(x, x, 0, 1, 0.1) land on 1.0 despite binary fractions.
bool seq_count(calc_t lo, calc_t hi, calc_t step, int* count, const char** err) {
    if (step == 0 || std::isnan(step) || std::isnan(lo) || std::isnan(hi)) {
        *err = "Bad seq step";
        return false;
    }
    const calc_t span = (hi - lo) / step;
    if (span < 0) {
        *err = "Bad seq range";
        return false;
    }
    const int n = static_cast<int>(std::floor(span + 0.5 * 1e-9 + 1e-9)) + 1;
    if (n > Array::kMaxElements) {
        *err = "List too long (max 10000)";
        return false;
    }
    *count = n;
    return true;
}

// How deeply a quoted body may nest — `seq(seq(...))`. The only re-entry in
// the machine, and small on purpose: this is the one place a C++ frame is paid
// per level, which is exactly what the phase exists to stop doing (D48).
constexpr int kMaxBodyDepth = 2;

struct Machine {
    const Program* p = nullptr;
    Value* st = g_stack;
    int n = 0;
    int body_depth = 0;
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

    // ---- temporaries -----------------------------------------------------

    // Claim a result array. Temporaries are reclaimed between instructions
    // (release_unreferenced), never inside one, so an operation's popped
    // inputs stay valid for its whole duration.
    Array* alloc_temp() {
        for (int t = 0; t < kMaxTemps; ++t) {
            if (!g_temp_used[t]) {
                g_temp_used[t] = true;
                g_temps[t].clear();
                return &g_temps[t];
            }
        }
        fail("Too many list terms");
        return nullptr;
    }

    // Free every temporary the operand stack no longer names. Scanning is what
    // makes this leak-proof without reference counts: the stack is the only
    // thing that holds a Value between instructions.
    // const: the pool is file-scope state, not the machine's — this only reads
    // the stack to decide what is still named.
    void release_unreferenced() const {
        for (int t = 0; t < kMaxTemps; ++t) {
            if (!g_temp_used[t]) {
                continue;
            }
            bool live = false;
            for (int i = 0; i < n && !live; ++i) {
                live = st[i].is_array() && st[i].a == &g_temps[t];
            }
            if (!live) {
                g_temps[t].clear();
                g_temp_used[t] = false;
            }
        }
    }

    // Arithmetic promotes to complex only when an operand already is. Keeping
    // the all-real case on plain doubles matters for more than speed: c_pow
    // routes real bases through std::pow precisely so the two evaluators agree
    // about ordinary real arithmetic (D46, D49).
    bool scalar_binop(Op op, const Value& a, const Value& b, Value* out) {
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
                    *out = z.im == 0.0 ? Value::real(z.re) : Value::complex(z);
                    return true;
                }
                default:
                    return fail("Syntax error");
            }
            *out = Value::real(v);
            return true;
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
        *out = v.im == 0.0 ? Value::real(v.re) : Value::complex(v);
        return true;
    }

    bool scalar_neg(const Value& a, Value* out) {
        *out = a.kind == Kind::kReal ? Value::real(-a.r) : Value::complex(-a.as_complex());
        return true;
    }

    bool scalar_builtin(const Builtin& b, const Value* args, int argc, Value* out) {
        if (argc != b.arity) {
            return fail("Syntax error");
        }
        if (argc == 1 && args[0].kind == Kind::kComplex) {
            if (b.cx == nullptr) {
                return fail("Non-real result");
            }
            const Complex v = b.cx(args[0].as_complex());
            *out = v.im == 0.0 ? Value::real(v.re) : Value::complex(v);
            return true;
        }
        // sqrt of a negative real is the one place a real argument must still
        // produce a complex result — matching complexexpr, where sqrt(-4) is
        // 2i rather than NaN.
        if (argc == 1 && b.cx == c_sqrt && args[0].r < 0) {
            *out = Value::complex(c_sqrt(Complex(args[0].r)));
            return true;
        }
        *out = argc == 1 ? Value::real(b.r1(args[0].r)) : Value::real(b.r2(args[0].r, args[1].r));
        return true;
    }

    bool scalar_catalog(const FnDescriptor& d, const Value* args, int argc, Value* out) {
        // Complex argument: use the complex counterpart when one exists.
        if (argc == 1 && args[0].kind == Kind::kComplex) {
            auto* cf = complex_variant(d.name);
            if (cf == nullptr) {
                return fail("Non-real result");
            }
            const Complex v = cf(args[0].as_complex());
            *out = v.im == 0.0 ? Value::real(v.re) : Value::complex(v);
            return true;
        }

        // Help-only rows (fn == nullptr) are the matrix functions, which land
        // with 5.2.7 — the list ones are dispatched from kCallList instead.
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
        *out = Value::real(v);
        return true;
    }

    // ---- elementwise dispatch --------------------------------------------

    // One description of "what to do to a scalar", so the scalar path and the
    // elementwise path cannot drift apart. That is the phase's whole premise
    // applied one level down: D46 was two evaluators disagreeing, and a
    // separate elementwise implementation would be the same mistake in
    // miniature.
    struct Applier {
        enum class Kind : uint8_t { kBinop, kNeg, kCatalog, kBuiltin };
        Kind kind = Kind::kBinop;
        Op op = Op::kAdd;
        const FnDescriptor* d = nullptr;
        const Builtin* b = nullptr;
    };

    bool apply_scalar(const Applier& ap, const Value* args, int argc, Value* out) {
        switch (ap.kind) {
            case Applier::Kind::kBinop:
                return scalar_binop(ap.op, args[0], args[1], out);
            case Applier::Kind::kNeg:
                return scalar_neg(args[0], out);
            case Applier::Kind::kCatalog:
                return scalar_catalog(*ap.d, args, argc, out);
            default:
                return scalar_builtin(*ap.b, args, argc, out);
        }
    }

    // The output chunk was staged real and an element came back complex —
    // `sqrt({4,-1})`, say. Widen what has been written so far (in place, back
    // to front, so the two views of the slot never overlap destructively) and
    // migrate the array itself.
    bool widen_output(Array& out, int written) {
        const calc_t* r = real_view(kOutSlot);
        for (int i = written - 1; i >= 0; --i) {
            g_slot[kOutSlot][i] = Complex(r[i]);
        }
        if (!listops::make_complex(out)) {
            return fail("Out of list memory");
        }
        return true;
    }

    // Apply `ap` elementwise. Any argument may be a list; the rest broadcast.
    bool broadcast(const Applier& ap, const Value* args, int argc) {
        int len = -1;
        bool any_complex = false;
        for (int i = 0; i < argc; ++i) {
            if (args[i].kind == Kind::kMatrix) {
                return fail("Matrix not allowed here");  // 5.2.7
            }
            if (args[i].kind == Kind::kList) {
                const int sz = args[i].a->size();
                if (len >= 0 && sz != len) {
                    return fail("List length mismatch");
                }
                len = sz;
                any_complex = any_complex || args[i].a->dtype() == Dtype::kComplex;
            } else {
                any_complex = any_complex || args[i].kind == Kind::kComplex;
            }
        }
        if (len < 0) {
            return fail("Expected a list");
        }

        Array* out = alloc_temp();
        if (out == nullptr) {
            return false;
        }
        out->set_dtype(any_complex ? Dtype::kComplex : Dtype::kDouble);
        if (!out->resize(len)) {
            return fail("Out of list memory");
        }
        bool out_complex = any_complex;

        for (int at = 0; at < len; at += kChunk) {
            const int m = len - at < kChunk ? len - at : kChunk;
            for (int i = 0; i < argc; ++i) {
                if (args[i].kind != Kind::kList) {
                    continue;
                }
                const Array& src = *args[i].a;
                if (src.dtype() == Dtype::kComplex) {
                    src.read_range_c(at, m, g_slot[i]);
                } else {
                    src.read_range(at, m, real_view(i));
                }
            }
            for (int j = 0; j < m; ++j) {
                Value ev[kMaxBcastArgs];
                for (int i = 0; i < argc; ++i) {
                    if (args[i].kind != Kind::kList) {
                        ev[i] = args[i];
                    } else if (args[i].a->dtype() == Dtype::kComplex) {
                        ev[i] = Value::complex(g_slot[i][j]);
                    } else {
                        ev[i] = Value::real(real_view(i)[j]);
                    }
                }
                Value r;
                if (!apply_scalar(ap, ev, argc, &r)) {
                    return false;
                }
                if (r.is_array()) {
                    return fail("Syntax error");  // no lists of lists
                }
                if (!out_complex && r.kind == Kind::kComplex) {
                    if (!widen_output(*out, j)) {
                        return false;
                    }
                    out_complex = true;
                }
                if (out_complex) {
                    g_slot[kOutSlot][j] = r.as_complex();
                } else {
                    real_view(kOutSlot)[j] = r.r;
                }
            }
            if (out_complex) {
                out->write_range_c(at, m, g_slot[kOutSlot]);
            } else {
                out->write_range(at, m, real_view(kOutSlot));
            }
        }
        return push(Value::list(out));
    }

    static bool any_list(const Value* args, int argc) {
        for (int i = 0; i < argc; ++i) {
            if (args[i].kind == Kind::kList) {
                return true;
            }
        }
        return false;
    }

    bool binary(Op op) {
        Value args[2];
        if (!pop(&args[1]) || !pop(&args[0])) {
            return false;
        }
        if (any_list(args, 2)) {
            Applier ap;
            ap.kind = Applier::Kind::kBinop;
            ap.op = op;
            return broadcast(ap, args, 2);
        }
        if (args[0].is_array() || args[1].is_array()) {
            return fail("Matrix not allowed here");  // 5.2.7
        }
        Value out;
        return scalar_binop(op, args[0], args[1], &out) && push(out);
    }

    bool call_builtin(const Instr& in) {
        if (in.b >= static_cast<uint16_t>(kBuiltinCount)) {
            return fail("Syntax error");
        }
        const Builtin& b = kBuiltins[in.b];
        const int argc = in.a;
        if (argc != b.arity) {
            return fail("Syntax error");
        }
        Value args[2];
        for (int i = argc - 1; i >= 0; --i) {
            if (!pop(&args[i])) {
                return false;
            }
        }
        if (any_list(args, argc)) {
            Applier ap;
            ap.kind = Applier::Kind::kBuiltin;
            ap.b = &b;
            return broadcast(ap, args, argc);
        }
        for (int i = 0; i < argc; ++i) {
            if (args[i].is_array()) {
                return fail("Matrix not allowed here");  // 5.2.7
            }
        }
        Value out;
        return scalar_builtin(b, args, argc, &out) && push(out);
    }

    bool call(const Instr& in) {
        int n_cat = 0;
        const FnDescriptor* cat = catalog(&n_cat);
        if (in.b >= static_cast<uint16_t>(n_cat)) {
            return fail("Syntax error");
        }
        const FnDescriptor& d = cat[in.b];
        const int argc = in.a;

        Value args[kMaxBcastArgs];
        if (argc > kMaxBcastArgs) {
            return fail("Expression too complex");
        }
        for (int i = argc - 1; i >= 0; --i) {
            if (!pop(&args[i])) {
                return false;
            }
        }
        if (any_list(args, argc)) {
            if (d.fn == nullptr) {
                return fail("Syntax error");
            }
            Applier ap;
            ap.kind = Applier::Kind::kCatalog;
            ap.d = &d;
            return broadcast(ap, args, argc);
        }
        for (int i = 0; i < argc; ++i) {
            if (args[i].is_array()) {
                return fail("Matrix not allowed here");  // 5.2.7
            }
        }
        Value out;
        return scalar_catalog(d, args, argc, &out) && push(out);
    }

    // ---- list construction and list functions ----------------------------

    // `{a, b, c}`: the elements are already contiguous on the operand stack,
    // which is why there is no literal parser here — the compiler emitted them
    // as ordinary expressions and this just types and packs them.
    bool make_list(int count) {
        if (count > n) {
            return fail("Syntax error");
        }
        if (count > kChunk) {
            return fail("List literal too long");
        }
        const int base = n - count;
        bool complex = false;
        for (int i = 0; i < count; ++i) {
            if (st[base + i].is_array()) {
                return fail("Bad list element");  // no lists of lists
            }
            complex = complex || st[base + i].kind == Kind::kComplex;
        }
        Array* out = alloc_temp();
        if (out == nullptr) {
            return false;
        }
        out->set_dtype(complex ? Dtype::kComplex : Dtype::kDouble);
        if (!out->resize(count)) {
            return fail("Out of list memory");
        }
        for (int i = 0; i < count; ++i) {
            if (complex) {
                g_slot[kOutSlot][i] = st[base + i].as_complex();
            } else {
                real_view(kOutSlot)[i] = st[base + i].r;
            }
        }
        if (complex) {
            out->write_range_c(0, count, g_slot[kOutSlot]);
        } else {
            out->write_range(0, count, real_view(kOutSlot));
        }
        n = base;
        return push(Value::list(out));
    }

    // seq(body, var, lo, hi, step). The body is a quoted code range and the
    // variable a slot index; both arrive as kPushInt operands (see the
    // compiler). The loop variable is saved and restored, as listops::seq
    // does — a seq must not leave its counter behind in `x`.
    bool eval_seq(const Value* args) {
        for (int i = 0; i < 5; ++i) {
            if (args[i].kind != Kind::kReal) {
                return fail("Bad seq argument");
            }
        }
        const int body = static_cast<int>(args[0].r);
        const int slot = static_cast<int>(args[1].r);
        if (body <= 0 || body >= p->n_code || slot < 0 || slot >= Variables::kCount) {
            return fail("Syntax error");
        }
        const calc_t lo = args[2].r;
        const calc_t step = args[4].r;
        int count = 0;
        const char* e = nullptr;
        if (!seq_count(lo, args[3].r, step, &count, &e)) {
            return fail(e);
        }
        Array* out = alloc_temp();
        if (out == nullptr) {
            return false;
        }
        out->set_dtype(Dtype::kDouble);  // as listexpr: seq output is real
        if (!out->resize(count)) {
            return fail("Out of list memory");
        }
        // Pushed BEFORE the loop on purpose: the body runs whole instructions,
        // so release_unreferenced() fires while this array is live, and the
        // operand stack is what marks a temporary as still in use.
        if (!push(Value::list(out))) {
            return false;
        }

        auto& vars = engine().vars();
        const calc_t saved_re = vars.vars[slot];
        const calc_t saved_im = vars.imag[slot];
        bool ok = true;
        for (int i = 0; i < count; ++i) {
            vars.set_real(slot, lo + static_cast<calc_t>(i) * step);
            Value v;
            if (!run_body(body, &v)) {
                ok = false;
                break;
            }
            if (v.kind != Kind::kReal) {
                fail("Non-real result");
                ok = false;
                break;
            }
            // Element at a time, not staged in chunks: the body is arbitrary
            // code and may call a reduction, which owns the same scratch
            // region (scratch.hpp's invariant — a kCompute buffer must not be
            // held live across a call to another kCompute owner).
            out->set(i, v.r);
        }
        vars.set_complex(slot, saved_re, saved_im);
        return ok;
    }

    // range(lo, hi[, step]) — inclusive, default step ±1 toward hi (D24).
    bool eval_range(const Value* args, int argc) {
        for (int i = 0; i < argc; ++i) {
            if (args[i].kind != Kind::kReal) {
                return fail("Bad range argument");
            }
        }
        const calc_t lo = args[0].r;
        const calc_t hi = args[1].r;
        const calc_t step = argc == 3 ? args[2].r : (hi >= lo ? 1 : -1);
        int count = 0;
        const char* e = nullptr;
        if (!seq_count(lo, hi, step, &count, &e)) {
            return fail(e);
        }
        Array* out = alloc_temp();
        if (out == nullptr) {
            return false;
        }
        out->set_dtype(Dtype::kDouble);
        if (!out->resize(count)) {
            return fail("Out of list memory");
        }
        for (int at = 0; at < count; at += kChunk) {
            const int m = count - at < kChunk ? count - at : kChunk;
            for (int j = 0; j < m; ++j) {
                real_view(kOutSlot)[j] = lo + static_cast<calc_t>(at + j) * step;
            }
            out->write_range(at, m, real_view(kOutSlot));
        }
        return push(Value::list(out));
    }

    // A list-producing wrapper over one real list: cumsum/delta_list/sorts.
    bool list_wrapper(ListFn fn, const Array& src) {
        Array* out = alloc_temp();
        if (out == nullptr) {
            return false;
        }
        out->set_dtype(Dtype::kDouble);
        bool ok = false;
        switch (fn) {
            case ListFn::kCumsum:
                ok = listops::cumsum(src, *out);
                break;
            case ListFn::kDeltaList:
                ok = listops::delta_list(src, *out);
                break;
            default:
                // Value semantics, always. The bare-argument in-place form
                // (`sort_asc(l4)` mutating l4) is a commit decision, not an
                // expression one, and belongs to the store grammar in 5.2.8.
                ok = listops::copy(src, *out) &&
                     (fn == ListFn::kSortAsc ? listops::sort_asc(*out) : listops::sort_desc(*out));
                break;
        }
        if (!ok) {
            return fail("Out of list memory");
        }
        return push(Value::list(out));
    }

    bool call_list(const Instr& in) {
        if (in.b >= static_cast<uint16_t>(kListFnCount)) {
            return fail("Syntax error");
        }
        const int idx = in.b;
        const ListFnDesc& d = kListFnTable[idx];
        const auto fn = static_cast<ListFn>(idx);
        const int argc = in.a;
        if (argc < d.argc_min || argc > d.argc_max) {
            return fail(fn == ListFn::kSeq     ? "seq needs (expr, var, lo, hi, step)"
                        : fn == ListFn::kRange ? "range needs (lo, hi[, step])"
                                               : "Syntax error");
        }

        Value args[5];
        for (int i = argc - 1; i >= 0; --i) {
            if (!pop(&args[i])) {
                return false;
            }
        }
        if (fn == ListFn::kSeq) {
            return eval_seq(args);
        }
        if (fn == ListFn::kRange) {
            return eval_range(args, argc);
        }

        // Everything else consumes exactly one list.
        if (args[0].kind != Kind::kList) {
            return fail("Expected a list");
        }
        const Array& src = *args[0].a;
        const bool cplx = src.dtype() == Dtype::kComplex;
        if (fn == ListFn::kLength) {
            return push(Value::real(src.size()));
        }
        if (cplx) {
            // Componentwise sum and mean are well defined on a complex list;
            // ordering and the moment statistics are not (D37: never silently
            // truncate). Unlike listexpr these results are ordinary complex
            // scalars — `sum(l1)+1` composes instead of erroring, which is a
            // widened behaviour for 5.2.10 to sign off.
            if (fn != ListFn::kSum && fn != ListFn::kMean) {
                return fail("Non-real list");
            }
            Complex s = listops::csum(src);
            if (fn == ListFn::kMean) {
                if (src.size() == 0) {
                    return fail("Undefined result");
                }
                s = s / Complex(static_cast<calc_t>(src.size()));
            }
            return push(s.im == 0.0 ? Value::real(s.re) : Value::complex(s));
        }
        switch (fn) {
            case ListFn::kSum:
                return push(Value::real(listops::sum(src)));
            case ListFn::kProd:
                return push(Value::real(listops::prod(src)));
            case ListFn::kCumsum:
            case ListFn::kDeltaList:
            case ListFn::kSortAsc:
            case ListFn::kSortDesc:
                return list_wrapper(fn, src);
            default:
                break;
        }
        const auto s = stats::one_var(src);
        if (!s.ok) {
            return fail(s.error);
        }
        const calc_t v = fn == ListFn::kMean     ? s.mean
                         : fn == ListFn::kMedian ? s.median
                                                 : s.sample_stddev;
        if (std::isnan(v)) {
            return fail("Undefined result");  // e.g. stdev of a 1-element list
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
                if (a.kind == Kind::kList) {
                    Applier ap;
                    ap.kind = Applier::Kind::kNeg;
                    return broadcast(ap, &a, 1);
                }
                if (a.is_array()) {
                    return fail("Matrix not allowed here");  // 5.2.7
                }
                Value out;
                return scalar_neg(a, &out) && push(out);
            }
            case Op::kPushInt:
                return push(Value::real(in.b));
            case Op::kPushList: {
                const int ref = in.b;
                if (ref < 0 ||
                    (ref >= ListStore::kCount && !named_lists().used(ref - kNamedRefBase))) {
                    return fail("Syntax error");
                }
                return push(Value::list(&list_by_ref(ref)));
            }
            case Op::kMakeList:
                return make_list(in.a);
            case Op::kCall:
                return call(in);
            case Op::kCallBi:
                return call_builtin(in);
            case Op::kCallList:
                return call_list(in);
            default:
                // kPushMat / kIndex / kTranspose / kStore arrive with their
                // tiers; kJump / kRet are handled by the execution loop.
                return fail("Syntax error");
        }
    }

    // ---- execution -------------------------------------------------------

    // Run from `start` until the program ends or a quoted body returns. Flat:
    // kJump moves the counter, it does not nest. The only nesting is a seq
    // body, which re-enters through run_body below and is depth-capped.
    bool run_from(int start) {
        int pc = start;
        while (pc >= 0 && pc < p->n_code) {
            const Instr& in = p->code[pc];
            if (in.op == Op::kRet) {
                return true;
            }
            if (in.op == Op::kJump) {
                if (in.b <= pc || in.b > p->n_code) {
                    return fail("Syntax error");  // only ever a forward skip
                }
                pc = in.b;
                continue;
            }
            if (!step(in)) {
                return false;
            }
            release_unreferenced();
            ++pc;
        }
        return true;
    }

    // Evaluate a quoted body and take its single value.
    bool run_body(int start, Value* out) {
        if (body_depth >= kMaxBodyDepth) {
            return fail("Too deeply nested");
        }
        const int base = n;
        ++body_depth;
        const bool ok = run_from(start);
        --body_depth;
        if (!ok) {
            return false;
        }
        if (n != base + 1) {
            return fail("Syntax error");
        }
        *out = st[--n];
        return true;
    }
};

}  // namespace

int builtin_index(const char* name, size_t len) {
    for (int i = 0; i < kBuiltinCount; ++i) {
        if (std::strlen(kBuiltins[i].name) == len &&
            std::strncmp(kBuiltins[i].name, name, len) == 0) {
            return i;
        }
    }
    return -1;
}

int builtin_arity(int idx) {
    return (idx >= 0 && idx < kBuiltinCount) ? kBuiltins[idx].arity : -1;
}

int list_fn_index(const char* name, size_t len) {
    for (int i = 0; i < kListFnCount; ++i) {
        if (std::strlen(kListFnTable[i].name) == len &&
            std::strncmp(kListFnTable[i].name, name, len) == 0) {
            return i;
        }
    }
    return -1;
}

bool list_fn_is_seq(int idx) {
    return idx == static_cast<int>(ListFn::kSeq);
}

bool run(const Program& p, Value* out, const char** err) {
    if (err != nullptr) {
        *err = nullptr;
    }
    // Every temporary from the previous run dies here — which is also what
    // makes the returned list Value valid only until the next call.
    for (int t = 0; t < kMaxTemps; ++t) {
        g_temps[t].clear();
        g_temp_used[t] = false;
    }
    Machine m;
    m.p = &p;
    if (!m.run_from(0)) {
        if (err != nullptr) {
            *err = m.err != nullptr ? m.err : "Syntax error";
        }
        return false;
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
