// Host-side tests for the Phase 5.2 unified evaluator.
//
// Task 5.2.2 covers the value type, instruction encoding and sizing only; the
// compiler (5.2.3) and stack machine (5.2.4) follow. These checks exist because
// the sizing is a *budget commitment* — the phase's premise is that it frees
// bss rather than spending it, and that only holds if these types stay the size
// they were measured at.

#include <cstdio>
#include <cstring>

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

    // Program is measured, not derived: the struct pads around its three
    // bookkeeping ints, so code+consts understates it by 16 B. The budget in
    // the header quotes the measured figure.
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
    check(p.n_elem_slots == 0, "no element slots bound");
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

}  // namespace

int main() {
    test_sizes();
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
