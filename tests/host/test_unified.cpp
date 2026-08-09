// Host-side tests for the Phase 5.2 unified evaluator.
//
// Task 5.2.2 covers the value type, instruction encoding and sizing only; the
// compiler (5.2.3) and stack machine (5.2.4) follow. These checks exist because
// the sizing is a *budget commitment* — the phase's premise is that it frees
// bss rather than spending it, and that only holds if these types stay the size
// they were measured at.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/array.hpp"
#include "math/engine.hpp"
#include "math/lists.hpp"
#include "math/mat_expr.hpp"
#include "math/matrix.hpp"
#include "math/named_lists.hpp"
#include "math/types.hpp"
#include "math/unified_eval.hpp"

namespace {

using namespace math::unified;

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

void check_eq(long got, long expected, const char* what) {
    ++g_checks;
    if (got != expected) {
        std::printf("FAIL: %s -> %ld (expected %ld)\n", what, got, expected);
        ++g_failures;
    }
}

// The measured figures from 5.2.1. On the host these are checked against the
// same expectations as the target: pointers are wider here (8 vs 4 bytes), but
// the union is dominated by Complex (16 B) on both, so sizeof(Value) agrees.
// If that ever stops being true the static_asserts in the header fire first.
void test_sizes() {
    check_eq(static_cast<long>(sizeof(Value)), 24, "sizeof(Value)");
    check_eq(static_cast<long>(sizeof(Instr)), 4, "sizeof(Instr)");

    // The bss budget claimed in phase5.2-spec.md §5 and the header. If these
    // move, the "5.2 frees several KB" claim needs re-deriving, not patching.
    check_eq(static_cast<long>(sizeof(Value)) * kMaxStack, 1536, "operand stack bytes");
    check_eq(static_cast<long>(sizeof(Instr)) * kMaxCode, 1024, "program code bytes");
    check_eq(static_cast<long>(sizeof(math::Complex)) * kMaxConsts, 1024, "constant pool bytes");

    // Program is measured, not derived: the struct pads around its bookkeeping
    // fields, so code+consts understates it by 16 B. The budget in the header
    // quotes the measured figure. It has moved twice — 2,064 before 5.2.6
    // dropped n_elem_slots with the element-slot lift it belonged to, and back
    // to 2,064 when 5.2.8 added the pending `-> name` store target.
    check_eq(static_cast<long>(sizeof(Program)), 2064, "sizeof(Program)");

    const long total = static_cast<long>(sizeof(Value)) * kMaxStack + sizeof(Program);
    check_eq(total, 3600, "total evaluator bss");
    check(total < 10053, "budget stays under the ~10 KB freed by retiring the three evaluators");
}

// The depth this phase buys. matexpr was capped at 3 (D48, 84 B of margin),
// complexexpr at 7/4, tinyexpr at 7 — all against call frames. Operand depth
// is a different and far more generous resource.
void test_depth_budget() {
    check(kMaxStack > 3, "beats matexpr's depth cap of 3");
    check(kMaxStack > 7, "beats complexexpr's and tinyexpr's cap of 7");
    check(kMaxCode >= kMaxStack, "a program can at least fill the operand stack");
}

void test_value_construction() {
    const Value r = Value::real(2.5);
    check(r.kind == Kind::kReal, "real kind");
    check(r.r == 2.5, "real payload");
    check(r.is_scalar() && !r.is_array(), "real is scalar");

    const Value c = Value::complex(math::Complex(3, 4));
    check(c.kind == Kind::kComplex, "complex kind");
    check(c.c.re == 3 && c.c.im == 4, "complex payload");
    check(c.is_scalar() && !c.is_array(), "complex is scalar");

    // Matrix and list are distinguishable *kinds* over the same pointer field —
    // that distinction is what lets norm()/dim() dispatch by argument type
    // instead of by which evaluator happened to run first.
    const auto* fake = reinterpret_cast<const math::Array*>(0x1000);
    const Value m = Value::matrix(fake);
    const Value l = Value::list(fake);
    check(m.kind == Kind::kMatrix && l.kind == Kind::kList, "matrix and list are distinct kinds");
    check(m.a == l.a, "both hold the same pointer field");
    check(m.is_array() && l.is_array(), "both are arrays");
    check(!m.is_scalar() && !l.is_scalar(), "neither is scalar");

    // Default is a zero real, so an unwritten stack slot is inert rather than a
    // dangling pointer.
    const Value d;
    check(d.kind == Kind::kReal && d.r == 0.0, "default Value is zero real");
}

void test_scalar_promotion() {
    check(Value::real(5).as_complex().re == 5, "real promotes, re");
    check(Value::real(5).as_complex().im == 0, "real promotes, im");
    check(Value::complex(math::Complex(1, 2)).as_complex().im == 2, "complex passes through");
}

void test_program_defaults() {
    const Program p;
    check(p.n_code == 0 && p.n_consts == 0, "empty program");
}

// ---- 5.2.3: the shunting-yard compiler -----------------------------------

// Render a program as a compact RPN string so tests can assert on shape
// rather than poking at instruction indices.
void render(const Program& p, char* buf, size_t cap) {
    size_t n = 0;
    const auto put = [&](const char* s) {
        while (*s != 0 && n + 1 < cap) {
            buf[n++] = *s++;
        }
    };
    for (int i = 0; i < p.n_code; ++i) {
        if (i != 0 && n + 1 < cap) {
            buf[n++] = ' ';
        }
        const Instr& in = p.code[i];
        char tmp[32];
        switch (in.op) {
            case Op::kPushConst: {
                const math::Complex& c = p.consts[in.b];
                if (c.im != 0) {
                    std::snprintf(tmp, sizeof(tmp), "%gi", c.im);
                } else {
                    std::snprintf(tmp, sizeof(tmp), "%g", c.re);
                }
                put(tmp);
                break;
            }
            case Op::kPushVar:
                std::snprintf(tmp, sizeof(tmp), "v%u", in.a);
                put(tmp);
                break;
            case Op::kPushSysc:
                std::snprintf(tmp, sizeof(tmp), "k%u", in.b);
                put(tmp);
                break;
            case Op::kCall:
                std::snprintf(tmp, sizeof(tmp), "call%u/%u", in.b, in.a);
                put(tmp);
                break;
            case Op::kAdd: put("+"); break;
            case Op::kSub: put("-"); break;
            case Op::kMul: put("*"); break;
            case Op::kDiv: put("/"); break;
            case Op::kPow: put("^"); break;
            case Op::kNeg: put("neg"); break;
            case Op::kCallBi:
                std::snprintf(tmp, sizeof(tmp), "bi%u/%u", in.b, in.a);
                put(tmp);
                break;
            case Op::kCallList:
                std::snprintf(tmp, sizeof(tmp), "lf%u/%u", in.b, in.a);
                put(tmp);
                break;
            case Op::kPushList:
                std::snprintf(tmp, sizeof(tmp), "L%u", in.b);
                put(tmp);
                break;
            case Op::kPushInt:
                std::snprintf(tmp, sizeof(tmp), "#%u", in.b);
                put(tmp);
                break;
            case Op::kMakeList:
                std::snprintf(tmp, sizeof(tmp), "mklist%u", in.a);
                put(tmp);
                break;
            case Op::kPushMat:
                std::snprintf(tmp, sizeof(tmp), "M%u", in.a);
                put(tmp);
                break;
            case Op::kMakeMat:
                std::snprintf(tmp, sizeof(tmp), "mkmat%ux%u", in.a, in.b);
                put(tmp);
                break;
            case Op::kIndex:
                std::snprintf(tmp, sizeof(tmp), "idx%u", in.a);
                put(tmp);
                break;
            case Op::kCallMat:
                std::snprintf(tmp, sizeof(tmp), "mf%u/%u", in.b, in.a);
                put(tmp);
                break;
            case Op::kTranspose: put("^T"); break;
            case Op::kStore: {
                const auto kind = static_cast<StoreKind>(in.a);
                const char tag = kind == StoreKind::kVar       ? 'v'
                                 : kind == StoreKind::kList    ? 'l'
                                 : kind == StoreKind::kNewList ? 'n'
                                                               : 'm';
                std::snprintf(tmp, sizeof(tmp), ">%c%u", tag, in.b);
                put(tmp);
                break;
            }
            case Op::kJump:
                std::snprintf(tmp, sizeof(tmp), "jmp%u", in.b);
                put(tmp);
                break;
            case Op::kRet: put("ret"); break;
            default: put("?"); break;
        }
    }
    buf[n < cap ? n : cap - 1] = 0;
}

void check_rpn(const char* src, const char* expected, const char* what) {
    ++g_checks;
    Program p;
    const char* err = nullptr;
    if (!compile(src, p, &err)) {
        std::printf("FAIL: %s: '%s' did not compile (%s)\n", what, src,
                    err != nullptr ? err : "?");
        ++g_failures;
        return;
    }
    char buf[256];
    render(p, buf, sizeof(buf));
    if (std::strcmp(buf, expected) != 0) {
        std::printf("FAIL: %s: '%s' -> \"%s\" (expected \"%s\")\n", what, src, buf, expected);
        ++g_failures;
    }
}

void check_compile_error(const char* src, const char* expected, const char* what) {
    ++g_checks;
    Program p;
    const char* err = nullptr;
    if (compile(src, p, &err)) {
        std::printf("FAIL: %s: '%s' compiled but should not have\n", what, src);
        ++g_failures;
        return;
    }
    if (err == nullptr || std::strcmp(err, expected) != 0) {
        std::printf("FAIL: %s: '%s' -> '%s' (expected '%s')\n", what, src,
                    err != nullptr ? err : "(null)", expected);
        ++g_failures;
    }
}

void test_compile_basics() {
    check_rpn("2+3", "2 3 +", "addition");
    check_rpn("2+3*4", "2 3 4 * +", "precedence: * over +");
    check_rpn("(2+3)*4", "2 3 + 4 *", "parens override precedence");
    check_rpn("2.5", "2.5", "decimal literal");
    check_rpn(".5", "0.5", "leading-dot literal");
    check_rpn("1e3", "1000", "exponent literal");
}

// Associativity is the part most likely to change meaning silently, and the
// existing evaluators pin both directions: `^` is right-associative (tinyexpr
// is built with TE_POW_FROM_RIGHT), `/` and `-` are left.
void test_compile_associativity() {
    check_rpn("2^3^2", "2 3 2 ^ ^", "^ is right-associative");
    check_rpn("8/4/2", "8 4 / 2 /", "/ is left-associative");
    check_rpn("8-4-2", "8 4 - 2 -", "- is left-associative");
    check_rpn("2^3*4", "2 3 ^ 4 *", "^ binds tighter than *");
}

void test_compile_unary() {
    check_rpn("-3", "3 neg", "unary minus");
    check_rpn("--3", "3 neg neg", "double negation folds by nesting");
    check_rpn("-3+4", "3 neg 4 +", "unary binds tighter than +");
    check_rpn("-3*4", "3 neg 4 *", "unary binds tighter than *");
    // Unary binds looser than ^, so -3^2 is -(3^2), matching the existing
    // evaluators and ordinary mathematical convention.
    check_rpn("-3^2", "3 2 ^ neg", "unary is looser than ^");
    check_rpn("+3", "3", "unary plus is a no-op");
}

void test_compile_identifiers() {
    check_rpn("a", "v0", "variable a is slot 0");
    check_rpn("z", "v25", "variable z is slot 25");
    check_rpn("theta", "v26", "theta is Variables::kTheta");
    check_rpn("ans", "v27", "ans is Variables::kAns");
    check_rpn("i", "1i", "i is the imaginary unit, not a variable");
    check_rpn("2i", "2i", "imaginary shorthand");
    check_rpn("3+2i", "3 2i +", "complex literal expression");
    check_rpn("a+b*c", "v0 v1 v2 * +", "variables obey precedence");
}

void test_compile_calls() {
    // sin is catalog index 0 in display order; assert the shape, not the
    // index, by checking arity and that a call is emitted at all.
    Program p;
    const char* err = nullptr;
    check(compile("sin(1)", p, &err), "sin(1) compiles");
    check(p.n_code == 2, "sin(1) is push + call");
    check(p.code[1].op == Op::kCall && p.code[1].a == 1, "sin(1) has arity 1");

    check(compile("ncr(5,2)", p, &err), "ncr(5,2) compiles");
    check(p.code[p.n_code - 1].op == Op::kCall && p.code[p.n_code - 1].a == 2,
          "ncr(5,2) has arity 2");

    check(compile("sin(1)+cos(2)", p, &err), "nested calls compile");
    check(p.code[p.n_code - 1].op == Op::kAdd, "call results feed the operator");

    check(compile("sin(cos(1))", p, &err), "call inside a call compiles");
    check(p.code[p.n_code - 1].op == Op::kCall, "outer call emitted last");
}

void test_compile_errors() {
    check_compile_error("", "Syntax error", "empty input");
    check_compile_error("2+", "Syntax error", "trailing operator");
    check_compile_error("(2+3", "Syntax error", "unclosed paren");
    check_compile_error("2+3)", "Syntax error", "unbalanced close");
    check_compile_error("nosuchfn(1)", "Syntax error", "unknown function");
    check_compile_error("qq", "Syntax error", "unknown multi-char identifier");
    check_compile_error("2 3", "Syntax error", "two operands, no operator");
    check_compile_error(",", "Syntax error", "bare comma");
}

// The depth this phase buys, exercised rather than asserted. matexpr capped
// at 3 and complexexpr at 7; both would reject these outright.
void test_compile_depth() {
    check_rpn("((((((((1))))))))", "1", "8 nested parens (matexpr capped at 3)");

    // Deep enough to exhaust the operator stack, which must be a clean error
    // rather than a fault — the entire point of moving depth off the call
    // stack.
    char deep[256];
    size_t n = 0;
    for (int i = 0; i < 80 && n + 2 < sizeof(deep); ++i) {
        deep[n++] = '(';
    }
    deep[n++] = '1';
    deep[n] = 0;
    check_compile_error(deep, "Too deeply nested", "operator stack overflow is a clean error");
}

// ---- 5.2.4: the stack machine --------------------------------------------

// Everything here evaluates in kProbe mode unless a check asks otherwise: a
// probe computes the value and writes nothing, so hundreds of checks run in any
// order without Ans, a store or a sorted list leaking from one into the next.
// The store tests (5.2.8) opt into kCommit and assert on what was written —
// which is also the contract 5.2.9's differential harness needs.
bool eval(const char* src, Value* out, const char** err, Mode mode = Mode::kProbe,
          Commit* commit = nullptr) {
    Program p;
    if (!compile(src, p, err)) {
        return false;
    }
    return run(p, out, err, mode, commit);
}

void check_real(const char* src, double expected, const char* what, double tol = 1e-9) {
    ++g_checks;
    Value v;
    const char* err = nullptr;
    if (!eval(src, &v, &err)) {
        std::printf("FAIL: %s: '%s' -> error %s\n", what, src, err != nullptr ? err : "?");
        ++g_failures;
        return;
    }
    if (v.kind != Kind::kReal) {
        std::printf("FAIL: %s: '%s' -> kind %d (expected real)\n", what, src,
                    static_cast<int>(v.kind));
        ++g_failures;
        return;
    }
    if (std::fabs(v.r - expected) > tol) {
        std::printf("FAIL: %s: '%s' -> %.12g (expected %.12g)\n", what, src, v.r, expected);
        ++g_failures;
    }
}

void check_cplx(const char* src, double re, double im, const char* what, double tol = 1e-9) {
    ++g_checks;
    Value v;
    const char* err = nullptr;
    if (!eval(src, &v, &err)) {
        std::printf("FAIL: %s: '%s' -> error %s\n", what, src, err != nullptr ? err : "?");
        ++g_failures;
        return;
    }
    const math::Complex z = v.as_complex();
    if (std::fabs(z.re - re) > tol || std::fabs(z.im - im) > tol) {
        std::printf("FAIL: %s: '%s' -> (%.12g,%.12g) (expected (%.12g,%.12g))\n", what, src, z.re,
                    z.im, re, im);
        ++g_failures;
    }
}

void check_eval_error(const char* src, const char* expected, const char* what) {
    ++g_checks;
    Value v;
    const char* err = nullptr;
    if (eval(src, &v, &err)) {
        std::printf("FAIL: %s: '%s' evaluated but should not have\n", what, src);
        ++g_failures;
        return;
    }
    if (err == nullptr || std::strcmp(err, expected) != 0) {
        std::printf("FAIL: %s: '%s' -> '%s' (expected '%s')\n", what, src,
                    err != nullptr ? err : "(null)", expected);
        ++g_failures;
    }
}

// A kCommit run: the value comes back in *v and what was written in *c.
bool commit_eval(const char* src, Value* v, Commit* c, const char* what) {
    ++g_checks;
    const char* err = nullptr;
    if (!eval(src, v, &err, Mode::kCommit, c)) {
        std::printf("FAIL: %s: '%s' -> error %s\n", what, src, err != nullptr ? err : "?");
        ++g_failures;
        return false;
    }
    return true;
}

void check_commit_error(const char* src, const char* expected, const char* what) {
    ++g_checks;
    Value v;
    Commit c;
    const char* err = nullptr;
    if (eval(src, &v, &err, Mode::kCommit, &c)) {
        std::printf("FAIL: %s: '%s' committed but should not have\n", what, src);
        ++g_failures;
        return;
    }
    if (err == nullptr || std::strcmp(err, expected) != 0) {
        std::printf("FAIL: %s: '%s' -> '%s' (expected '%s')\n", what, src,
                    err != nullptr ? err : "(null)", expected);
        ++g_failures;
    }
}

void test_vm_arithmetic() {
    check_real("2+3", 5, "add");
    check_real("2+3*4", 14, "precedence holds through evaluation");
    check_real("(2+3)*4", 20, "parens hold through evaluation");
    check_real("8/4/2", 1, "/ left-associative evaluates to 1, not 4");
    check_real("2^3^2", 512, "^ right-associative evaluates to 512, not 64");
    check_real("-3^2", -9, "unary looser than ^");
    check_real("--3", 3, "double negation");
    check_real("10-4-3", 3, "- left-associative");
}

// The unified evaluator must agree with the real path on ordinary arithmetic.
// That is not a nicety: D46 was two evaluators disagreeing about sin(30), and
// removing that class of bug is half this phase's justification.
void test_vm_matches_real_path() {
    math::set_angle_mode(math::AngleMode::kRadians);
    check_real("sin(0)", 0, "sin(0)");
    check_real("cos(0)", 1, "cos(0)");
    check_real("ln(1)", 0, "ln(1)");
    check_real("round(3.456,1)", 3.5, "round with 2 args");
    check_real("ncr(5,2)", 10, "ncr");
    check_real("min(3,7)", 3, "min");
    check_real("fac(5)", 120, "fac");

    // Angle mode reaches the catalog's own angle-aware entries.
    math::set_angle_mode(math::AngleMode::kDegrees);
    check_real("sin(30)", 0.5, "sin(30) in DEGREE mode");
    check_real("cos(60)", 0.5, "cos(60) in DEGREE mode");
    math::set_angle_mode(math::AngleMode::kRadians);
}

void test_vm_complex() {
    check_cplx("3+2i", 3, 2, "complex literal sum");
    check_cplx("2i*2i", -4, 0, "i squared scales");
    check_real("i^2", -1, "i^2 collapses to real -1");
    check_real("i^4", 1, "i^4 collapses to real 1");
    check_cplx("(1+i)*(1+i)", 0, 2, "complex product");

    // D49: integer powers of a complex base are exact, so the real axis is hit
    // exactly and the result reports as real rather than 0+2i with an epsilon.
    check_cplx("(1+i)^2", 0, 2, "(1+i)^2 exact");
    check_real("(1+i)^4", -4, "(1+i)^4 collapses to real -4");
    check_cplx("(3+4i)^2", -7, 24, "(3+4i)^2 exact");

    // The angle-mode wrappers must survive into the complex tier — this is
    // D46 itself. sin of a real-valued complex must equal the real path.
    math::set_angle_mode(math::AngleMode::kDegrees);
    check_cplx("sin(30+0i)", 0.5, 0, "complex sin honours DEGREE mode (D46)");
    math::set_angle_mode(math::AngleMode::kRadians);
}

void test_vm_variables() {
    auto& vars = math::engine().vars();
    vars.set_real(0, 6);                        // a = 6
    vars.set_complex(1, 1, 2);                  // b = 1+2i
    vars.set_real(math::Variables::kAns, 100);  // ans
    check_real("a", 6, "variable read");
    check_real("a*2", 12, "variable in an expression");
    check_cplx("b", 1, 2, "complex variable read via is_complex");
    check_cplx("b+1", 2, 2, "complex variable arithmetic");
    check_real("ans", 100, "ans");
    vars.set_real(0, 0);
    vars.set_real(1, 0);
}

void test_vm_constants() {
    check_real("pi", 3.14159265358979, "pi", 1e-12);
    check_real("e", 2.71828182845905, "e", 1e-12);
    check_real("clight", 299792458.0, "catalog constant clight", 1e-3);
}

void test_vm_errors() {
    // A catalog function with no complex counterpart refuses a complex
    // argument rather than silently truncating to the real part.
    check_eval_error("fac(1+2i)", "Non-real result", "no complex counterpart");

    // Help-only catalog rows (fn == nullptr) are the list and matrix
    // functions. The list ones gained real bindings in 5.2.6 — `sum` now
    // reports what is actually wrong with `sum(1)` — while the matrix ones
    // stay uncallable until 5.2.7.
    check_eval_error("sum(1)", "Expected a list", "a bound list function reports its own error");
    check_eval_error("det(1)", "Expected a matrix", "and so does a bound matrix function");

    check_compile_error("nosuchfn(1)", "Syntax error", "unknown name is still an error");
}

// 5.2.5 closed the gap 5.2.4 found: the callable surface is three tables, not
// one — catalog.cpp's 82 rows, tinyexpr's builtins, and complex_expr's
// complex-only set. These pin all three reaching the evaluator.
void test_builtins() {
    check_real("sqrt(4)", 2, "sqrt (tinyexpr builtin)");
    check_real("abs(-2)", 2, "abs (tinyexpr builtin)");
    check_real("exp(0)", 1, "exp (tinyexpr builtin)");
    check_real("floor(2.7)", 2, "floor");
    check_real("ceil(2.1)", 3, "ceil");
    check_real("log10(100)", 2, "log10");
    check_real("atan2(0,1)", 0, "atan2, arity 2");
    check_real("pow(2,10)", 1024, "pow, arity 2");
    check_real("sinh(0)", 0, "sinh");

    // Complex-only functions, with a real argument as the degenerate case.
    check_real("real(3+4i)", 3, "real() of a complex");
    check_real("imag(3+4i)", 4, "imag() of a complex");
    check_real("abs(3+4i)", 5, "abs() of a complex is the modulus");
    check_cplx("conj(3+4i)", 3, -4, "conj()");
    check_real("real(7)", 7, "real() of a real");
    check_real("imag(7)", 0, "imag() of a real is zero");

    // sqrt of a negative real must produce a complex result, as complexexpr
    // does — sqrt(-4) is 2i, not NaN. This is the one place a real argument
    // legitimately leaves the real tier.
    check_cplx("sqrt(-4)", 0, 2, "sqrt(-4) is 2i, not NaN");
    check_cplx("sqrt(-1)", 0, 1, "sqrt(-1) is i");

    // The catalogue must win over the builtin table for shared names: its
    // sin/cos/tan are angle-mode aware, tinyexpr's are raw radians. If the
    // builtin shadowed it, DEGREE mode would silently break — D46 again.
    math::set_angle_mode(math::AngleMode::kDegrees);
    check_real("sin(30)", 0.5, "catalog's angle-aware sin wins over the builtin");
    math::set_angle_mode(math::AngleMode::kRadians);

    // Euler's identity, end to end through both tables.
    check_real("exp(0)*cos(0)", 1, "builtin and catalog compose");
}

// ---- 5.2.6: the list tier -------------------------------------------------
//
// The behaviours mirrored here are test_lists' expression-layer checks, which
// is the acceptance this task is written against. Where the unified evaluator
// deliberately differs from listexpr the check says so and names 5.2.10, the
// task that signs those widenings off — a silent difference would be exactly
// the kind of drift this phase exists to remove.

void seed_list(int slot, const double* v, int n) {
    math::Array& a = math::lists().list(slot);
    a.clear();
    a.set_dtype(math::Dtype::kDouble);
    a.resize(n);
    if (n > 0) {
        a.write_range(0, n, v);
    }
}

void seed_clist(int slot, const math::Complex* v, int n) {
    math::Array& a = math::lists().list(slot);
    a.clear();
    a.set_dtype(math::Dtype::kComplex);
    a.resize(n);
    a.write_range_c(0, n, v);
}

const math::Array* eval_list(const char* src, const char* what) {
    Value v;
    const char* err = nullptr;
    if (!eval(src, &v, &err)) {
        std::printf("FAIL: %s: '%s' -> error %s\n", what, src, err != nullptr ? err : "?");
        ++g_failures;
        return nullptr;
    }
    if (v.kind != Kind::kList) {
        std::printf("FAIL: %s: '%s' -> kind %d (expected list)\n", what, src,
                    static_cast<int>(v.kind));
        ++g_failures;
        return nullptr;
    }
    return v.a;
}

void check_list(const char* src, const double* expected, int n, const char* what) {
    ++g_checks;
    const math::Array* a = eval_list(src, what);
    if (a == nullptr) {
        return;
    }
    if (a->size() != n) {
        std::printf("FAIL: %s: '%s' -> size %d (expected %d)\n", what, src, a->size(), n);
        ++g_failures;
        return;
    }
    for (int i = 0; i < n; ++i) {
        if (std::fabs(a->get(i) - expected[i]) > 1e-9) {
            std::printf("FAIL: %s: '%s'[%d] -> %.12g (expected %.12g)\n", what, src, i, a->get(i),
                        expected[i]);
            ++g_failures;
            return;
        }
    }
}

void check_clist(const char* src, const math::Complex* expected, int n, const char* what) {
    ++g_checks;
    const math::Array* a = eval_list(src, what);
    if (a == nullptr) {
        return;
    }
    if (a->size() != n) {
        std::printf("FAIL: %s: '%s' -> size %d (expected %d)\n", what, src, a->size(), n);
        ++g_failures;
        return;
    }
    for (int i = 0; i < n; ++i) {
        const math::Complex z = a->cget(i);
        if (std::fabs(z.re - expected[i].re) > 1e-9 || std::fabs(z.im - expected[i].im) > 1e-9) {
            std::printf("FAIL: %s: '%s'[%d] -> (%.12g,%.12g) (expected (%.12g,%.12g))\n", what, src,
                        i, z.re, z.im, expected[i].re, expected[i].im);
            ++g_failures;
            return;
        }
    }
}

void test_list_literals() {
    const double one_two_three[] = {1, 2, 3};
    check_list("{1,2,3}", one_two_three, 3, "brace literal");
    check_list("{1, 2, 3}", one_two_three, 3, "spaces between elements");

    // Elements are full expressions — the compiler emits them inline and
    // kMakeList packs the operand stack, so this needs no literal parser.
    const double pis[] = {3.141592653589793, 6.283185307179586};
    check_list("{pi, 2*pi}", pis, 2, "literal elements are expressions");
    const double computed[] = {3, 0, 8};
    check_list("{1+2, sin(0), 2^3}", computed, 3, "calls and powers as elements");

    ++g_checks;
    const math::Array* empty = eval_list("{}", "empty literal");
    if (empty != nullptr && empty->size() != 0) {
        std::printf("FAIL: '{}' -> size %d (expected 0)\n", empty->size());
        ++g_failures;
    }

    check_rpn("{1,2}", "1 2 mklist2", "literal compiles to a packing instruction");
    check_compile_error("{1,2", "Syntax error", "unclosed literal");
    check_compile_error("{1,foo}", "Syntax error", "unknown identifier in a literal");
    check_eval_error("{l1, 2}", "Bad list element", "no lists of lists");
}

void test_list_refs() {
    const double base[] = {1, 2, 3};
    seed_list(0, base, 3);
    seed_list(1, base, 3);

    check_rpn("l1", "L0", "l1 is a list reference");
    check_list("l1", base, 3, "bare reference");

    const double doubled[] = {2, 4, 6};
    check_list("l1*2", doubled, 3, "list times scalar");
    check_list("2*l1", doubled, 3, "scalar times list");
    check_list("l1+l1", doubled, 3, "list plus list");
    check_list("l1+l2", doubled, 3, "two distinct lists");

    const double plus_one[] = {2, 3, 4};
    check_list("l1+{1,1,1}", plus_one, 3, "reference plus literal");
    check_list("{1,2,3}+1", plus_one, 3, "literal plus scalar");
    const double from_ten[] = {11, 12, 13};
    check_list("10+{1,2,3}", from_ten, 3, "scalar on the left");
    const double neg[] = {-1, -2, -3};
    check_list("-l1", neg, 3, "unary minus lifts");

    // Functions map over a list — one dispatch, whether the name comes from
    // the catalogue or tinyexpr's builtin table.
    const double squares[] = {1, 4, 9};
    check_list("l1^2", squares, 3, "power lifts");
    const double roots[] = {1, 1.414213562373095, 1.732050807568877};
    check_list("sqrt(l1)", roots, 3, "builtin maps over a list");
    const double sines[] = {0, 0, 0};
    check_list("sin(0*l1)", sines, 3, "catalog function maps over a list");
    const double rounded[] = {1.5, 2.5, 3.5};
    const double halves[] = {1.46, 2.54, 3.5};
    seed_list(2, halves, 3);
    check_list("round(l3,1)", rounded, 3, "arity-2 catalog call with one list argument");

    // Reductions inside an elementwise expression. listexpr substitutes these
    // textually before lifting; here the RPN order does it — the reduction is
    // simply an earlier instruction, evaluated once.
    const double normed[] = {1.0 / 6, 2.0 / 6, 3.0 / 6};
    check_list("l1/sum(l1)", normed, 3, "reduction inside an elementwise expression");

    const double two[] = {1, 2};
    seed_list(5, two, 2);
    check_eval_error("l1+l6", "List length mismatch", "operand lengths must agree");
    check_eval_error("{1,2}+{1,2,3}", "List length mismatch", "literal lengths must agree");

    seed_list(4, nullptr, 0);
    ++g_checks;
    const math::Array* e = eval_list("l5*2", "empty list lifts to empty");
    if (e != nullptr && e->size() != 0) {
        std::printf("FAIL: 'l5*2' -> size %d (expected 0)\n", e->size());
        ++g_failures;
    }

    // Named lists resolve exactly like l1-l6 (4D.13's one numbering).
    const int slot = math::named_lists().create("pric");
    check(slot >= 0, "named list created for the reference test");
    if (slot >= 0) {
        math::Array& a = math::named_lists().list(slot);
        a.clear();
        a.set_dtype(math::Dtype::kDouble);
        a.resize(3);
        a.write_range(0, 3, base);
        check_list("pric*2", doubled, 3, "named list reference");
        math::named_lists().remove(slot);
    }
}

void test_list_reductions() {
    const double base[] = {1, 2, 3};
    seed_list(0, base, 3);

    check_real("sum(l1)", 6, "sum");
    check_real("prod(l1)", 6, "prod");
    check_real("length(l1)", 3, "length");
    check_real("mean(l1)", 2, "mean");
    check_real("median(l1)", 2, "median");
    check_real("stdev(l1)", 1, "stdev");
    check_real("std(l1)", 1, "std alias (no catalogue row of its own)");
    check_real("1+sum(l1)*2", 13, "reduction embeds in a scalar expression");
    check_real("mean(l1)+sum(l1)", 8, "two reductions");

    // General arguments: the reduction takes whatever the previous
    // instructions left on the stack, so a computed list needs no special case.
    check_real("sum(l1*2)", 12, "reduction over a computed list");
    check_real("sum(range(1,100))", 5050, "reduction over a generator");
    check_real("mean({1,2,3,4})", 2.5, "reduction over a literal");
    check_real("sum(cumsum(l1))", 10, "reduction over a wrapper result");

    check_eval_error("sum(2)", "Expected a list", "a reduction needs a list");
    check_eval_error("stdev({5})", "Undefined result", "sample stdev of one element");
}

void test_list_wrappers() {
    const double base[] = {1, 2, 3};
    seed_list(0, base, 3);
    const double cs[] = {1, 3, 6};
    check_list("cumsum(l1)", cs, 3, "cumsum");
    const double dl[] = {1, 1};
    check_list("delta_list(l1)", dl, 2, "delta_list");
    const double sorted[] = {1, 2, 3};
    check_list("sort_asc({3,1,2})", sorted, 3, "sort_asc");
    const double desc[] = {3, 2, 1};
    check_list("sort_desc({3,1,2})", desc, 3, "sort_desc");
    check_list("cumsum(sort_asc({3,1,2}))", cs, 3, "wrappers compose");
    const double plus[] = {2, 4, 7, 11};
    check_list("cumsum(range(1,4))+1", plus, 4, "wrapper result feeds an elementwise op");

    // Value semantics in the expression tier, always. listexpr's in-place form
    // is a COMMIT behaviour and 5.2.8 restored it there, as an implicit store —
    // so a probe of the same input sorts nothing, which is what these two
    // checks are: the expression half in isolation.
    const double unsorted[] = {3, 1, 2};
    seed_list(3, unsorted, 3);
    check_list("sort_asc(l4)", sorted, 3, "sort by value");
    check(math::lists().list(3).get(0) == 3, "a probe leaves l4 alone (test_store_in_place_sort)");

    // What the phase is for. listexpr caps this chain at kMaxRec = 3 because
    // every level costs a call frame against core 0's 4 KB (D47); here the
    // depth is operand-stack slots and instructions, so it just evaluates.
    check_list("sort_asc(sort_asc(sort_asc(sort_asc({3,1,2}))))", sorted, 3,
               "nesting past listexpr's recursion cap now evaluates");

    const double r5[] = {1, 2, 3, 4, 5};
    check_list("range(1,5)", r5, 5, "range");
    const double down[] = {5, 4, 3, 2, 1};
    check_list("range(5,1)", down, 5, "range steps toward hi");
    const double halves[] = {0, 0.5, 1};
    check_list("range(0,1,0.5)", halves, 3, "range with an explicit step");
    const double r6[] = {2, 4, 6};
    check_list("range(1,3)*2", r6, 3, "range inside an expression");
    check_eval_error("range(1,2,-1)", "Bad seq range", "step points away from hi");
    check_eval_error("range(1)", "range needs (lo, hi[, step])", "range arity");
    check_eval_error("range(l1,2)", "Bad range argument", "range takes scalars");
}

void test_list_seq() {
    const double base[] = {1, 2, 3};
    seed_list(0, base, 3);

    check_rpn("seq(x,x,1,2,1)", "jmp3 v23 ret #1 #23 1 2 1 lf11/5",
              "seq compiles its body as a quoted range the top level skips");

    const double squares[] = {1, 4, 9, 16, 25};
    check_list("seq(x^2, x, 1, 5, 1)", squares, 5, "seq");
    const double thetas[] = {2, 4};
    check_list("seq(2*theta, theta, 1, 2, 1)", thetas, 2, "seq over theta");

    // The body is ordinary code: it can call anything, including a reduction
    // over a list. That also pins the scratch-arena rule — the body must not
    // run while a chunk buffer is live.
    const double scaled[] = {2, 4, 6};
    check_list("seq(x*mean(l1), x, 1, 3, 1)", scaled, 3, "a reduction inside a seq body");

    // The loop variable is restored, as listops::seq restores it.
    math::engine().vars()['x'] = 42;
    check_list("seq(x, x, 1, 3, 1)", base, 3, "seq over x");
    ++g_checks;
    if (math::engine().vars()['x'] != 42) {
        std::printf("FAIL: seq left %g in x (expected 42)\n",
                    static_cast<double>(math::engine().vars()['x']));
        ++g_failures;
    }

    check_eval_error("seq(x,x,1,5)", "seq needs (expr, var, lo, hi, step)", "seq arity");
    check_compile_error("seq(x,e,1,5,1)", "seq var must be a-z (not e/i) or theta",
                        "seq var cannot be e");
    check_compile_error("seq(x,i,1,5,1)", "seq var must be a-z (not e/i) or theta",
                        "seq var cannot be i");
    check_eval_error("seq(i*x, x, 1, 3, 1)", "Non-real result", "seq output stays real");
}

void test_complex_lists() {
    math::set_number_mode(math::NumberMode::kRectangular);
    const math::Complex c2[] = {math::Complex(1, 1), math::Complex(2, -1)};
    seed_clist(0, c2, 2);
    const double r2[] = {10, 20};
    seed_list(1, r2, 2);

    const math::Complex doubled[] = {math::Complex(2, 2), math::Complex(4, -2)};
    check_clist("l1+l1", doubled, 2, "complex list addition");
    const math::Complex scaled[] = {math::Complex(-2, 2), math::Complex(2, 4)};
    check_clist("2i*l1", scaled, 2, "complex scalar times complex list");
    const math::Complex halved[] = {math::Complex(0.5, 0.5), math::Complex(1, -0.5)};
    check_clist("l1/2", halved, 2, "complex list divided by a scalar");
    const math::Complex mixed[] = {math::Complex(11, 1), math::Complex(22, -1)};
    check_clist("l1+l2", mixed, 2, "complex list plus real list");
    const math::Complex promoted[] = {math::Complex(1, 1), math::Complex(2, 1)};
    check_clist("{1,2}+i", promoted, 2, "a complex scalar promotes a real list");
    const math::Complex cliteral[] = {math::Complex(1, 1), math::Complex(2, -1)};
    check_clist("{1+i, 2-i}", cliteral, 2, "complex literal");

    check_real("sum(l1)", 3, "sum of a complex list lands on the real axis");
    check_real("mean(l1)", 1.5, "mean of a complex list");
    // listexpr requires a complex sum/mean to BE the whole expression
    // ("Complex sum/mean must stand alone") because it splices results back
    // into text. A Value composes — widened, 5.2.10.
    check_real("sum(l1)+1", 4, "a complex reduction composes (widened, 5.2.10)");

    // Widened by construction: listexpr's complex lift is a narrow grammar
    // (sums of terms, one list factor each), so these three were errors —
    // "Complex lists support only +, -, scalar * and /", "one list per term",
    // "Cannot divide by a list". Here they are ordinary dispatch. 5.2.10 signs
    // them off; the values are pinned now so the widening is deliberate.
    const math::Complex squared[] = {math::Complex(0, 2), math::Complex(3, -4)};
    check_clist("l1*l1", squared, 2, "list times list (widened, 5.2.10)");
    const math::Complex reciprocal[] = {math::Complex(1, -1), math::Complex(0.8, 0.4)};
    check_clist("2/l1", reciprocal, 2, "scalar divided by a list (widened, 5.2.10)");
    const math::Complex sines[] = {math::Complex(1.298457581415977, 0.634963914784736),
                                   math::Complex(1.403119250622040, 0.489056259041294)};
    check_clist("sin(l1)", sines, 2, "a function maps over a complex list (widened, 5.2.10)");

    // Not widened: ordering and the moment statistics stay undefined on
    // complex data (D37 — never silently truncate).
    check_eval_error("prod(l1)", "Non-real list", "prod of a complex list");
    check_eval_error("median(l1)", "Non-real list", "median of a complex list");
    check_eval_error("stdev(l1)", "Non-real list", "stdev of a complex list");
    check_eval_error("sort_asc(l1)", "Non-real list", "sorting a complex list");
    check_eval_error("cumsum(l1)", "Non-real list", "cumsum of a complex list");
    check_real("length(l1)", 2, "length works on a complex list");

    const double three[] = {1, 2, 3};
    seed_list(2, three, 3);
    check_eval_error("l1+l3", "List length mismatch", "complex length mismatch");

    // A real result set that leaves the real axis mid-list promotes the whole
    // array rather than erroring or truncating — the list-level counterpart of
    // sqrt(-4) being 2i.
    const math::Complex mixed_roots[] = {math::Complex(2, 0), math::Complex(0, 1)};
    check_clist("sqrt({4,-1})", mixed_roots, 2, "a real list promotes when an element goes complex");

    math::set_number_mode(math::NumberMode::kReal);
    seed_list(0, three, 3);
}

// The chunked path: everything above fits one 256-element chunk, so nothing
// above would notice if the streaming were wrong.
void test_list_chunking() {
    ++g_checks;
    const math::Array* big = eval_list("range(1,1000)", "1000-element generator");
    if (big != nullptr && (big->size() != 1000 || big->get(999) != 1000 || !big->in_psram())) {
        std::printf("FAIL: range(1,1000) -> size %d, last %g, psram %d\n", big->size(),
                    static_cast<double>(big->get(999)), static_cast<int>(big->in_psram()));
        ++g_failures;
    }
    check_real("sum(range(1,1000))", 500500, "reduction over a PSRAM-tier list");

    ++g_checks;
    const math::Array* lifted = eval_list("range(1,1000)*3", "elementwise over four chunks");
    if (lifted != nullptr &&
        (lifted->size() != 1000 || lifted->get(0) != 3 || lifted->get(255) != 768 ||
         lifted->get(256) != 771 || lifted->get(999) != 3000)) {
        std::printf("FAIL: range(1,1000)*3 -> [0]=%g [255]=%g [256]=%g [999]=%g\n",
                    static_cast<double>(lifted->get(0)), static_cast<double>(lifted->get(255)),
                    static_cast<double>(lifted->get(256)), static_cast<double>(lifted->get(999)));
        ++g_failures;
    }
    check_real("sum(range(1,1000)*3-range(1,1000)*2)", 500500, "chunked operands stay aligned");

    // Promotion after a chunk has already been written as real: the array has
    // to be migrated, not just the staging buffer. range(300,-100,-1) turns
    // negative at index 301, so chunk 0 is real on disk when it happens.
    ++g_checks;
    const math::Array* late = eval_list("sqrt(range(300,-100,-1))", "promotion across chunks");
    if (late != nullptr) {
        const math::Complex first = late->cget(0);
        const math::Complex neg = late->cget(301);
        if (late->size() != 401 || late->dtype() != math::Dtype::kComplex ||
            std::fabs(first.re - std::sqrt(300.0)) > 1e-9 || first.im != 0 ||
            std::fabs(neg.im - 1.0) > 1e-9 || neg.re != 0) {
            std::printf("FAIL: sqrt(range(300,-100,-1)) -> size %d, [0]=(%.12g,%.12g), "
                        "[301]=(%.12g,%.12g)\n",
                        late->size(), first.re, first.im, neg.re, neg.im);
            ++g_failures;
        }
    }
}

// Temporaries are recycled between instructions, so a long chain of list
// results must not exhaust the pool — and one that genuinely holds too many
// operands live must say so rather than corrupt anything.
void test_list_temporaries() {
    const double base[] = {1, 2, 3};
    seed_list(0, base, 3);
    const double sixfold[] = {6, 12, 18};
    check_list("l1+l1+l1+l1+l1+l1", sixfold, 3, "a chain reuses temporaries");
    const double stacked[] = {3, 6, 9};
    check_list("{1,2,3}+({1,2,3}+{1,2,3})", stacked, 3, "nested literals");
    check_eval_error("{1}+({2}+({3}+({4}+({5}+({6}+{7})))))", "Too many list terms",
                     "too many simultaneously live list operands is a clean error");
}

// ---- 5.2.7: the matrix tier -----------------------------------------------
//
// Mirrors test_matrix's expression-layer checks (test_expr_basics,
// test_expr_errors, test_matrix_literals, test_matans_token,
// test_list_matrix_bridge, test_frobenius_norm, test_complex_expr_layer).
// Stores are 5.2.8, so the store forms in those tests are not mirrored here.

void seed_matrix(int slot, int rows, int cols, const double* v) {
    math::Array& m = math::matrices().matrix(slot);
    m.clear();
    m.set_dtype(math::Dtype::kDouble);
    m.resize(rows, cols);
    for (int i = 0; i < rows * cols; ++i) {
        m.set(i, v[i]);
    }
}

void seed_cmatrix(int slot, int rows, int cols, const math::Complex* v) {
    math::Array& m = math::matrices().matrix(slot);
    m.clear();
    m.set_dtype(math::Dtype::kComplex);
    m.resize(rows, cols);
    for (int i = 0; i < rows * cols; ++i) {
        m.cset(i, v[i]);
    }
}

const math::Array* eval_matrix(const char* src, const char* what) {
    Value v;
    const char* err = nullptr;
    if (!eval(src, &v, &err)) {
        std::printf("FAIL: %s: '%s' -> error %s\n", what, src, err != nullptr ? err : "?");
        ++g_failures;
        return nullptr;
    }
    if (v.kind != Kind::kMatrix) {
        std::printf("FAIL: %s: '%s' -> kind %d (expected matrix)\n", what, src,
                    static_cast<int>(v.kind));
        ++g_failures;
        return nullptr;
    }
    return v.a;
}

void check_matrix(const char* src, int rows, int cols, const double* expected, const char* what) {
    ++g_checks;
    const math::Array* m = eval_matrix(src, what);
    if (m == nullptr) {
        return;
    }
    if (m->dim(0) != rows || m->dim(1) != cols) {
        std::printf("FAIL: %s: '%s' -> %dx%d (expected %dx%d)\n", what, src, m->dim(0), m->dim(1),
                    rows, cols);
        ++g_failures;
        return;
    }
    for (int i = 0; i < rows * cols; ++i) {
        if (std::fabs(m->get(i) - expected[i]) > 1e-9) {
            std::printf("FAIL: %s: '%s'[%d] -> %.12g (expected %.12g)\n", what, src, i, m->get(i),
                        expected[i]);
            ++g_failures;
            return;
        }
    }
}

void test_matrix_refs() {
    const double va[4] = {1, 2, 3, 4};
    const double vb[4] = {5, 6, 7, 8};
    seed_matrix(0, 2, 2, va);
    seed_matrix(1, 2, 2, vb);

    check_rpn("[A]", "M0", "matrix reference");
    check_matrix("[A]", 2, 2, va, "bare reference");
    check_matrix("[a]", 2, 2, va, "lowercase reference");

    const double vsum[4] = {6, 8, 10, 12};
    check_matrix("[A]+[B]", 2, 2, vsum, "add");
    const double vdiff[4] = {4, 4, 4, 4};
    check_matrix("[B]-[A]", 2, 2, vdiff, "sub");
    // Not elementwise: this is the matrix product, which is why matexpr could
    // never be a lift the way listexpr is.
    const double vprod[4] = {19, 22, 43, 50};
    check_matrix("[A]*[B]", 2, 2, vprod, "matrix product");
    const double vscale[4] = {2, 4, 6, 8};
    check_matrix("2*[A]", 2, 2, vscale, "scalar times matrix");
    check_matrix("[A]*2", 2, 2, vscale, "matrix times scalar");
    const double vhalf[4] = {0.5, 1, 1.5, 2};
    check_matrix("[A]/2", 2, 2, vhalf, "matrix over scalar");
    const double vneg[4] = {-1, -2, -3, -4};
    check_matrix("-[A]", 2, 2, vneg, "unary minus");

    const double vt[4] = {1, 3, 2, 4};
    check_matrix("[A]^T", 2, 2, vt, "postfix transpose");
    check_matrix("transpose([A])", 2, 2, vt, "transpose()");
    const double vsq[4] = {7, 10, 15, 22};
    check_matrix("[A]^2", 2, 2, vsq, "integer power");
    const double vid[4] = {1, 0, 0, 1};
    check_matrix("[A]^-1*[A]", 2, 2, vid, "inverse via ^-1");
    check_matrix("inverse([A])*[A]", 2, 2, vid, "inverse()");
    const double vid3[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    check_matrix("identity(3)", 3, 3, vid3, "identity");
    const double vaug[8] = {1, 2, 5, 6, 3, 4, 7, 8};
    check_matrix("augment([A],[B])", 2, 4, vaug, "augment");
    check_matrix("rref([A])", 2, 2, vid, "rref");

    const double vmix[4] = {3, 6, 9, 12};
    check_matrix("(1+2)*[A]", 2, 2, vmix, "paren scalar times matrix");
    check_matrix("[A]*sin(pi/2)*3", 2, 2, vmix, "catalog call inside a matrix expression");

    check_real("det([A])", -2, "det");
    check_real("det([A])*2+1", -3, "det inside a scalar expression");
    check_real("rank([A])", 2, "rank");
    check_real("[A](2,1)", 3, "element access");
    check_real("[A](1,2)+[B](2,2)", 10, "element arithmetic");

    // `matans` resolves to the last matrix result, which matexpr still owns —
    // it is state with its own persistence, not evaluator logic. Nothing in
    // this evaluator WRITES it yet: committing a result is 5.2.8's job, so
    // here the token can only be read, and it reads empty.
    check_rpn("matans", "M10", "matans is a matrix register beyond [A]-[J]");
    check_eval_error("matans", "No matrix result", "no matrix result to recall yet");
}

void test_matrix_literals() {
    const double va[4] = {1, 2, 3, 4};
    check_rpn("[[1,2][3,4]]", "1 2 3 4 mkmat2x2", "literal compiles to a packing instruction");
    check_matrix("[[1,2][3,4]]", 2, 2, va, "matrix literal");
    const double vrow[3] = {5, 7, 11};
    check_matrix("[[2+3,7,11]]", 1, 3, vrow, "one-row literal with expressions");
    const double vsum[4] = {2, 4, 6, 8};
    check_matrix("[[1,2][3,4]]+[[1,2][3,4]]", 2, 2, vsum, "literal arithmetic");
    check_real("det([[1,2][3,4]])", -2, "det of a literal");

    check_compile_error("[[1,2][3]]", "Dim mismatch", "ragged literal");
    check_compile_error("[[1,2][3,4]", "Syntax error", "unterminated literal");
    check_compile_error("[[]]", "Bad matrix literal", "empty row");
    check_compile_error("[K]", "Syntax error", "there is no [K]");
}

void test_matrix_errors() {
    const double va[4] = {1, 2, 3, 4};
    const double vc[6] = {1, 2, 3, 4, 5, 6};
    seed_matrix(0, 2, 2, va);
    seed_matrix(1, 2, 3, vc);
    math::matrices().matrix(9).clear();

    check_eval_error("[A]+[B]", "Dim mismatch", "add dim mismatch");
    check_eval_error("[B]*[B]", "Dim mismatch", "mul dim mismatch");
    check_eval_error("[A]+2", "Dim mismatch", "matrix plus scalar");
    check_eval_error("[A]/[B]", "Matrix division: use ^-1", "matrix division");
    check_eval_error("2/[A]", "Matrix division: use ^-1", "scalar over matrix");
    check_eval_error("[J]", "Matrix is empty", "empty slot");
    check_eval_error("[A](3,1)", "Index out of range", "element out of range");
    check_eval_error("[A](1)", "Expected (row, col)", "one-index element");
    check_eval_error("sin([A])", "Matrix not allowed here", "matrix into a scalar function");
    check_eval_error("[A]^1.5", "Bad matrix exponent", "fractional matrix power");
    check_eval_error("[A]^[A]", "Bad matrix exponent", "matrix exponent");
    check_eval_error("det(2)", "Expected a matrix", "det of a scalar");
    check_compile_error("det([A]", "Syntax error", "unbalanced call");
    check_compile_error("[A]*", "Syntax error", "trailing operator");
}

// What the phase buys, again: matexpr caps its parser at depth 3 with 84 bytes
// of margin (D48), and these three shapes are exactly what that cap rejects —
// including the one that hard-faulted the Pico 1. Here depth is operand-stack
// slots, so they simply evaluate. Widened, 5.2.10.
void test_matrix_depth() {
    const double va[4] = {1, 2, 3, 4};
    seed_matrix(0, 2, 2, va);
    seed_matrix(1, 2, 2, va);

    check_real("det([A]*[B]+[A])", -8, "matexpr's depth 2");
    check_real("det([[1,2][3,4]])", -2, "matexpr's depth 3");
    check_real("det(identity(2))", 1, "matexpr's depth 3, nested call");
    check_real("det(([A]*([A]+[A]))+[A])", -6, "depth 4 — matexpr's hard-fault shape");
    check_matrix("((([A])))", 2, 2, va, "depth 4 paren chain");
    check_real("det(inverse(([A])))", -0.5, "depth 4 via nested calls");
    check_real("det(inverse(inverse(inverse(([A])))))", -0.5, "deeper still");
}

void test_matrix_bridge() {
    const double va[4] = {1, 2, 3, 4};
    seed_matrix(0, 2, 2, va);
    const double vb[6] = {1, 2, 3, 4, 5, 6};
    seed_matrix(1, 2, 3, vb);

    // dim() and eigenvals() are list-valued. matexpr had to make them
    // whole-expression forms ("dim/eigenvals must stand alone") because its
    // Value could not hold a list; this one can, so they compose. Widened,
    // 5.2.10.
    const double vdim[2] = {2, 3};
    check_list("dim([B])", vdim, 2, "dim is a list");
    check_real("sum(dim([B]))", 5, "dim composes into a reduction");
    const double vdim2[2] = {4, 6};
    check_list("dim([B])*2", vdim2, 2, "dim composes into an elementwise op");

    const double vsym[4] = {2, 1, 1, 2};
    seed_matrix(2, 2, 2, vsym);
    const double veig[2] = {3, 1};
    check_list("eigenvals([C])", veig, 2, "eigenvals descending");
    check_list("eig([C])", veig, 2, "eig alias");
    check_real("sum(eigenvals([C]))", 4, "eigenvalue sum is the trace");

    // A complex-conjugate pair was unstorable display text in matexpr because
    // "lists are real-only" — which 4D.24 changed. It is a complex list here.
    // Widened, 5.2.10.
    math::set_number_mode(math::NumberMode::kRectangular);
    const double vrot[4] = {0, -1, 1, 0};
    seed_matrix(3, 2, 2, vrot);
    const math::Complex vpair[2] = {math::Complex(0, 1), math::Complex(0, -1)};
    check_clist("eigenvals([D])", vpair, 2, "complex spectrum is a complex list");
    math::set_number_mode(math::NumberMode::kReal);

    // list2mat / mat2list round trip.
    const double l1v[3] = {1, 2, 3};
    const double l2v[2] = {4, 5};
    seed_list(0, l1v, 3);
    seed_list(1, l2v, 2);
    const double vpk[6] = {1, 4, 2, 5, 3, 0};
    check_matrix("list2mat(l1,l2)", 3, 2, vpk, "list2mat packs columns, zero-padded");
    check_matrix("list2mat(range(1,3),l2)", 3, 2, vpk,
                 "list2mat takes any list expression (widened, 5.2.10)");

    seed_matrix(6, 3, 2, vpk);
    // The one call in the expression tier that writes, so it needs kCommit —
    // and the refs it wrote come back in lists_mask, which is what persistence
    // keys off (5.2.8).
    Value mv;
    Commit mc;
    commit_eval("mat2list([G], l3, l4)", &mv, &mc, "mat2list writes two lists");
    check(mv.kind == Kind::kReal && mv.r == 2, "mat2list yields the count written");
    check(mc.lists_mask == ((1U << 2) | (1U << 3)), "mat2list reports both refs");
    ++g_checks;
    if (math::lists().list(2).size() != 3 || math::lists().list(2).get(2) != 3 ||
        math::lists().list(3).size() != 3 || math::lists().list(3).get(1) != 5) {
        std::printf("FAIL: mat2list column values\n");
        ++g_failures;
    }
    check_compile_error("mat2list([G], x)", "mat2list targets are l1-l6", "bad mat2list target");
    // A statement, not an expression: composing it would let one term rewrite a
    // list another term is holding by reference. 5.2.8's static-buffer audit
    // found this; matexpr's "must stand alone" was the same rule.
    check_compile_error("2*mat2list([G], l3)", "mat2list must stand alone", "no composition");
    check_compile_error("mat2list([G], l3)->a", "mat2list must stand alone", "and no store");
    check_eval_error("mat2list([G])", "mat2list needs ([A], l1, ...)", "mat2list needs targets");
    check_eval_error("list2mat(2)", "list2mat takes l1-l6 args", "list2mat needs lists");

    // Vector ops (4D.22) — listexpr's, absorbed here because `norm` is the one
    // name that means different things to a list and a matrix.
    check_real("dot(l1,{4,5,6})", 32, "dot");
    const double vx[3] = {-3, 6, -3};
    check_list("cross(l1,{4,5,6})", vx, 3, "cross");
    check_real("norm({3,4})", 5, "norm of a list is Euclidean");
    const double vfro[4] = {3, 4, 0, 0};
    seed_matrix(7, 2, 2, vfro);
    check_real("norm([H])", 5, "norm of a matrix is Frobenius");
    check_real("norm([H])^2", 25, "norm composes");
    check_eval_error("dot(l1,{1,2})", "Dim mismatch", "dot needs equal lengths");
    check_eval_error("cross({1,2},{3,4})", "cross needs 3-elem lists", "cross needs 3 elements");
    check_eval_error("norm(l1,l1)", "norm takes one list", "norm arity");
    check_eval_error("dot(l1)", "Need two lists", "dot arity");
}

void test_matrix_complex() {
    math::set_number_mode(math::NumberMode::kRectangular);
    const math::Complex va[4] = {math::Complex(1, 1), math::Complex(2, 0), math::Complex(3, 0),
                                 math::Complex(4, -1)};
    seed_cmatrix(0, 2, 2, va);
    const double vb[4] = {5, 6, 7, 8};
    seed_matrix(1, 2, 2, vb);

    check_cplx("det([A])", -1, 3, "det of a complex matrix");
    check_cplx("[A](1,1)", 1, 1, "complex element access");

    ++g_checks;
    const math::Array* scaled = eval_matrix("i*[B]", "complex scalar scales a real matrix");
    if (scaled != nullptr &&
        (scaled->dtype() != math::Dtype::kComplex || std::fabs(scaled->cget(0, 0).im - 5) > 1e-9)) {
        std::printf("FAIL: i*[B] did not promote\n");
        ++g_failures;
    }
    ++g_checks;
    const math::Array* mixed = eval_matrix("[A]+[B]", "complex plus real matrix");
    if (mixed != nullptr && std::fabs(mixed->cget(0, 0).re - 6) > 1e-9) {
        std::printf("FAIL: [A]+[B] value\n");
        ++g_failures;
    }
    const double vcl[4] = {0, 0, 0, 1};
    ++g_checks;
    const math::Array* clit = eval_matrix("[[i,0][0,1]]", "complex literal");
    if (clit != nullptr && (clit->dtype() != math::Dtype::kComplex ||
                            std::fabs(clit->cget(0, 0).im - 1) > 1e-9)) {
        std::printf("FAIL: complex literal dtype/value\n");
        ++g_failures;
    }
    (void)vcl;

    check_eval_error("eigenvals([A])", "Non-real matrix", "eigenvals of a complex matrix");

    // REAL mode never READS complex data, even for an operation with a real
    // result, so nothing silently takes a real part (4D.25's rule, kept).
    math::set_number_mode(math::NumberMode::kReal);
    check_eval_error("[A]+[A]", "Non-real result", "REAL gates a complex register");
    check_eval_error("det([A])", "Non-real result", "REAL gates det of a complex register");
    check_eval_error("[[i,0][0,1]]", "Non-real result", "REAL gates a complex literal");
    check_real("det([B])", -2, "real det still evaluates in REAL mode");

    // A complex RESULT built from real data — `i*[B]`, where no register is
    // complex — is matexpr's "gate the result too" case (mat_expr.cpp:1312).
    // 5.2.7 left it open because nothing here owned the commit; 5.2.8 does, so
    // the gate is on the mode: kCommit refuses it, kProbe still computes it.
    // That split is the point. `i^2` collapsing to a real -1 in any mode is the
    // same rule seen from the other side — intermediates are never gated, only
    // what would be committed or displayed.
    check_commit_error("i*[B]", "Non-real result", "REAL gates a complex result on commit");
    ++g_checks;
    const math::Array* promoted = eval_matrix("i*[B]", "complex result under a probe");
    if (promoted != nullptr && promoted->dtype() != math::Dtype::kComplex) {
        std::printf("FAIL: i*[B] must still compute under a probe\n");
        ++g_failures;
    }

    seed_matrix(0, 2, 2, vb);
}

// The acceptance criterion in phase5.2-spec.md §4 for this task: "compile
// once, eval N". The program is the artifact that makes it true — parsing
// happens once and the result is re-runnable, so per-element work is
// instruction dispatch and nothing else. Checked directly rather than inferred
// from timings, which a host suite cannot measure meaningfully.
void test_program_reuse() {
    const double base[] = {1, 2, 3};
    seed_list(0, base, 3);

    Program prog;
    const char* err = nullptr;
    check(compile("l1*2", prog, &err), "compiles once");
    const int code_len = prog.n_code;

    Value v;
    check(run(prog, &v, &err, Mode::kProbe) && v.kind == Kind::kList, "first run");
    check(v.kind == Kind::kList && v.a->size() == 3 && v.a->get(2) == 6, "first run values");

    // Re-run the SAME program against different data: no recompilation, and
    // the program is not consumed or mutated by execution.
    const double changed[] = {10, 20, 30};
    seed_list(0, changed, 3);
    check(run(prog, &v, &err, Mode::kProbe) && v.kind == Kind::kList,
          "second run of the same program");
    check(v.kind == Kind::kList && v.a->get(2) == 60, "second run tracks the new data");
    check(prog.n_code == code_len, "execution leaves the program unchanged");

    seed_list(0, base, 3);
}

// ---- 5.2.8: the store grammar and commit semantics ------------------------
//
// Five target forms in one grammar, where the three evaluators had four copies
// of a rightmost-"->" string search that disagreed about what counts as a
// target. The compile-side checks pin the grammar; the commit-side ones pin
// what a run is allowed to write, and — just as importantly — what a probe is
// not.

void test_store_compile() {
    check_rpn("2->a", "2 >v0", "scalar store to a");
    check_rpn("2->theta", "2 >v26", "theta is a store target");
    check_rpn("1+2->a", "1 2 + >v0", "the arrow ends the expression");
    check_rpn("{1,2}->l3", "1 2 mklist2 >l2", "list store");
    check_rpn("l1->l2", "L0 >l1", "list copy");
    check_rpn("[A]->[C]", "M0 >m2", "matrix store");
    check_rpn("[A]+[B]->[C]", "M0 M1 + >m2", "matrix expression store");

    // The pointed reserved-word errors, kept verbatim from the parsers being
    // retired: the silent case-fold these replaced was a wrong answer, not a
    // syntax error.
    check_compile_error("2->A", "Variables are lowercase a-z", "uppercase target");
    check_compile_error("2->e", "e is reserved (Euler's e)", "e is Euler's number");
    check_compile_error("2->i", "i is reserved (imaginary unit)", "i is the imaginary unit");

    check_compile_error("2->3", "Bad store target", "a number is not a target");
    check_compile_error("2->", "Bad store target", "bare arrow");
    check_compile_error("2->ans", "Bad store target", "Ans is not writable");
    check_compile_error("2->matans", "Bad store target", "MatAns is a result register");
    check_compile_error("2->[K]", "Bad store target", "matrix slots stop at [J]");
    check_compile_error("2->l7", "Bad store target", "l7 is not a slot, and not a name");
    // Two behaviours the old rightmost-arrow search got to by accident: a
    // chained store parsed as `(1->a) -> b` and failed inside tinyexpr, and a
    // trailing term left the arrow in the body. Both are pointed now.
    check_compile_error("1->a->b", "Bad store target", "stores do not chain");
    check_compile_error("1->a+2", "Bad store target", "the target must end the input");
}

void test_store_commit() {
    auto& vars = math::engine().vars();
    vars.set_real(0, 0);
    vars.set_real(math::Variables::kAns, 0);

    Value v;
    Commit c;
    commit_eval("6->a", &v, &c, "scalar store commits");
    check(c.var == 0, "the store reports its variable");
    check(vars.vars[0] == 6, "a holds 6");
    check(vars.vars[math::Variables::kAns] == 6, "Ans is written for any scalar result");
    check(c.list == -1 && c.matrix == -1 && c.lists_mask == 0, "nothing else was written");

    // Ans is written with no store at all — the rule matexpr and the engine
    // both have, and the reason `ans` resolves to something after every entry.
    vars.set_real(math::Variables::kAns, 0);
    commit_eval("2+3", &v, &c, "a storeless result still updates Ans");
    check(vars.vars[math::Variables::kAns] == 5, "Ans is 5");
    check(c.var == -1, "no store target reported");

    // A complex value stores whole (4D.15) rather than losing its imaginary
    // part, which is why the commit goes through set_complex.
    math::set_number_mode(math::NumberMode::kRectangular);
    commit_eval("3+4i->b", &v, &c, "complex store");
    check(vars.is_complex(1) && vars.vars[1] == 3 && vars.imag[1] == 4, "b holds 3+4i");
    math::set_number_mode(math::NumberMode::kReal);
    vars.set_real(0, 0);
    vars.set_real(1, 0);
}

void test_store_probe() {
    auto& vars = math::engine().vars();
    vars.set_real(2, 11);
    vars.set_real(math::Variables::kAns, 99);
    const double base[] = {1, 2, 3};
    seed_list(4, base, 3);

    // The whole contract of kProbe: the same value, and nothing written. This
    // is what the home screen's REAL-mode probe needs (it works today only
    // because complexexpr happens to have no state to write) and what 5.2.9's
    // differential harness needs to run one input through two evaluators.
    Value v;
    Commit c;
    const char* err = nullptr;
    check(eval("7->c", &v, &err, Mode::kProbe, &c), "a store compiles and runs under a probe");
    check(v.kind == Kind::kReal && v.r == 7, "the probe returns the value it would have stored");
    check(vars.vars[2] == 11, "the probe did not write c");
    check(vars.vars[math::Variables::kAns] == 99, "the probe did not write Ans");
    check(c.var == -1 && c.lists_mask == 0, "the probe reports no commits");

    check(eval("{9,9}->l5", &v, &err, Mode::kProbe, &c), "a list store runs under a probe");
    check(math::lists().list(4).size() == 3 && math::lists().list(4).get(0) == 1,
          "the probe did not write l5");

    // A kind mismatch is a property of the input, not of the mode, so a probe
    // reports it too — otherwise the probe would answer a different question
    // from the run it stands in for.
    ++g_checks;
    if (eval("2->l1", &v, &err, Mode::kProbe, &c)) {
        std::printf("FAIL: a probe must still reject a mismatched store\n");
        ++g_failures;
    }
    vars.set_real(2, 0);
}

void test_store_targets() {
    Value v;
    Commit c;
    const double base[] = {1, 2, 3};
    seed_list(0, base, 3);

    // l1 -> l2, the copy form.
    commit_eval("l1->l2", &v, &c, "list copy commits");
    check(c.list == 1 && c.lists_mask == (1U << 1), "l2 is the reported target");
    check(math::lists().list(1).size() == 3 && math::lists().list(1).get(2) == 3, "l2 has l1");
    check(v.kind == Kind::kList && v.a == &math::lists().list(1),
          "the value points at the stored slot, not the temporary");

    // A named list that does not exist yet is created when the store commits,
    // never when it compiles — a program that fails to evaluate must not leave
    // a stray empty list behind (4D.13).
    math::named_lists().remove(math::named_lists().find("cost"));
    check_rpn("l1*2->cost", "L0 2 * >n0", "a new named list compiles as a pending name");
    check(math::named_lists().find("cost") < 0, "compiling did not create the list");
    ++g_checks;
    if (eval("{1,2}->cost", &v, nullptr, Mode::kProbe, &c) &&
        math::named_lists().find("cost") >= 0) {
        std::printf("FAIL: a probe created a named list\n");
        ++g_failures;
    }
    commit_eval("l1*2->cost", &v, &c, "named-list store commits");
    const int slot = math::named_lists().find("cost");
    check(slot >= 0, "the named list exists after the commit");
    check(c.names_modified, "the registry change is reported so the directory persists");
    check(c.list == math::kNamedRefBase + slot, "the ref uses the one numbering (4D.13)");
    check(math::named_lists().list(slot).get(1) == 4, "and holds the value");

    // Re-running the SAME program must store again, not fail as a duplicate —
    // compile-once/eval-N applies to stores as much as to expressions.
    Program prog;
    const char* err = nullptr;
    check(compile("{7,8}->cost", prog, &err), "the pending-name program compiles");
    check(run(prog, &v, &err, Mode::kCommit, &c), "first commit creates");
    check(run(prog, &v, &err, Mode::kCommit, &c), "second commit stores into the same list");
    check(math::named_lists().list(slot).get(0) == 7, "the second run overwrote");
    math::named_lists().remove(slot);

    // Matrix store, and MatAns alongside it (mat_expr.cpp:1322): the matrix
    // editor and matans.dat both read that buffer, and it is what gives a
    // matrix result a lifetime past the next run.
    const double va[4] = {1, 2, 3, 4};
    seed_matrix(0, 2, 2, va);
    commit_eval("[A]+[A]->[C]", &v, &c, "matrix store commits");
    check(c.matrix == 2, "[C] is the reported target");
    check(c.mat_ans, "a matrix result rewrites MatAns");
    check(math::matrices().matrix(2).get(1, 1) == 8, "[C] holds the sum");
    check(math::matexpr::mat_ans().get(1, 1) == 8, "MatAns holds it too");
    ++g_checks;
    if (v.kind != Kind::kMatrix || v.a != &math::matrices().matrix(2)) {
        std::printf("FAIL: a stored matrix result must name the slot it went to\n");
        ++g_failures;
    }
    commit_eval("[A]*2", &v, &c, "a storeless matrix result still lands in MatAns");
    check(c.matrix == -1 && c.mat_ans, "MatAns without a store target");
    ++g_checks;
    if (v.kind != Kind::kMatrix || v.a != &math::matexpr::mat_ans()) {
        std::printf("FAIL: an unstored matrix result must name MatAns\n");
        ++g_failures;
    }

    // A list result from a MATRIX expression, into a named list. Impossible
    // today from either side: matexpr's store targets have no named-list branch
    // and listexpr never sees `dim([A])`. The target grammar stopped being
    // per-evaluator. Widened, 5.2.10.
    commit_eval("dim([A])->mydim", &v, &c, "a matrix-expression list into a named list");
    const int dim_slot = math::named_lists().find("mydim");
    check(dim_slot >= 0 && math::named_lists().list(dim_slot).get(0) == 2, "mydim holds dim([A])");
    math::named_lists().remove(dim_slot);

    // Kind mismatches. Two strings survive, each for the input it is pointed
    // about: a missing list, and a target of the wrong shape.
    check_commit_error("2->l1", "Store target needs a list", "a scalar is not a list");
    check_commit_error("[A]->l1", "Store target needs a list", "nor is a matrix");
    check_commit_error("l1->a", "Store target mismatch", "a list is not a scalar");
    check_commit_error("[A]->a", "Store target mismatch", "nor is a matrix");
    check_commit_error("2->[C]", "Store target mismatch", "a scalar is not a matrix");
    check_commit_error("l1->[C]", "Store target mismatch", "nor is a list");
}

// The other half of 5.2.7's outstanding pair: `sort_asc(l4)` sorts l4 in place
// in listexpr, because a bare list argument makes the call a statement. The
// expression tier evaluates it by value; the in-place half is recovered as an
// implicit store, which is why it lands with the store grammar rather than
// with the list tier.
void test_store_in_place_sort() {
    const double jumbled[] = {3, 1, 2};
    seed_list(3, jumbled, 3);

    check_rpn("sort_asc(l4)", "L3 lf9/1 >l3", "a bare-argument sort stores back");
    check_rpn("sort_asc(l4+0)", "L3 0 + lf9/1", "a compound argument stays by value");
    check_rpn("sort_asc(l4)*1", "L3 lf9/1 1 *", "and so does a sort inside an expression");

    Value v;
    Commit c;
    commit_eval("sort_asc(l4)", &v, &c, "in-place sort commits");
    check(math::lists().list(3).get(0) == 1 && math::lists().list(3).get(2) == 3, "l4 is sorted");
    check(c.lists_mask == (1U << 3), "the sorted list is reported for persistence (D35)");

    // sort + store writes both, which is listexpr's rule (list_expr.cpp:1387).
    seed_list(3, jumbled, 3);
    commit_eval("sort_asc(l4)->l5", &v, &c, "sort with a store target");
    check(math::lists().list(3).get(0) == 1, "the source is still sorted in place");
    check(math::lists().list(4).get(0) == 1, "and the target holds the result");
    check(c.lists_mask == ((1U << 3) | (1U << 4)), "both refs need persisting");
    check(c.list == 4, "the store target is the one reported as the result");

    // A probe of the same input sorts nothing.
    seed_list(3, jumbled, 3);
    const char* err = nullptr;
    check(eval("sort_asc(l4)", &v, &err, Mode::kProbe, &c), "the probe evaluates");
    check(v.kind == Kind::kList && v.a->get(0) == 1, "and returns the sorted value");
    check(math::lists().list(3).get(0) == 3, "but leaves l4 alone");
}

// REAL mode never commits or displays a non-real value (D30). The gate is on
// the result, never on intermediates — which is the distinction that lets
// abs(3+4i) work in REAL mode on every path, old and new.
void test_store_real_gate() {
    math::set_number_mode(math::NumberMode::kReal);
    check_commit_error("sqrt(-4)", "Non-real result", "a complex scalar result");
    check_commit_error("sqrt(-4)->a", "Non-real result", "and it is refused before the store");
    ++g_checks;
    if (math::engine().vars().is_complex(0)) {
        std::printf("FAIL: a rejected result must not reach the variable\n");
        ++g_failures;
    }
    check_commit_error("{1,-1}^0.5", "Non-real result", "a complex list result");

    Value v;
    Commit c;
    commit_eval("i^2", &v, &c, "an intermediate may be complex");
    check(v.kind == Kind::kReal && v.r == -1, "i^2 is a real -1 in any mode");
    commit_eval("abs(3+4i)", &v, &c, "and so may an argument");
    check(v.kind == Kind::kReal && v.r == 5, "abs(3+4i) is 5");

    // The same inputs under a probe: computed, not gated. That split is what
    // makes a probe usable as a probe.
    const char* err = nullptr;
    check(eval("sqrt(-4)", &v, &err, Mode::kProbe, &c), "the probe computes sqrt(-4)");
    check(v.kind == Kind::kComplex && std::fabs(v.c.im - 2) < 1e-9, "and it is 2i");
}

}  // namespace

int main() {
    test_sizes();
    test_vm_arithmetic();
    test_vm_matches_real_path();
    test_vm_complex();
    test_vm_variables();
    test_vm_constants();
    test_vm_errors();
    test_builtins();
    test_list_literals();
    test_list_refs();
    test_list_reductions();
    test_list_wrappers();
    test_list_seq();
    test_complex_lists();
    test_list_chunking();
    test_list_temporaries();
    test_matrix_refs();
    test_matrix_literals();
    test_matrix_errors();
    test_matrix_depth();
    test_matrix_bridge();
    test_matrix_complex();
    test_program_reuse();
    test_store_compile();
    test_store_commit();
    test_store_probe();
    test_store_targets();
    test_store_in_place_sort();
    test_store_real_gate();
    test_compile_basics();
    test_compile_associativity();
    test_compile_unary();
    test_compile_identifiers();
    test_compile_calls();
    test_compile_errors();
    test_compile_depth();
    test_depth_budget();
    test_value_construction();
    test_scalar_promotion();
    test_program_defaults();

    std::printf("test_unified: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
