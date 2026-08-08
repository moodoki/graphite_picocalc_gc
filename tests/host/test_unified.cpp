// Host-side tests for the Phase 5.2 unified evaluator.
//
// Task 5.2.2 covers the value type, instruction encoding and sizing only; the
// compiler (5.2.3) and stack machine (5.2.4) follow. These checks exist because
// the sizing is a *budget commitment* — the phase's premise is that it frees
// bss rather than spending it, and that only holds if these types stay the size
// they were measured at.

#include <cstdio>

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

}  // namespace

int main() {
    test_sizes();
    test_depth_budget();
    test_value_construction();
    test_scalar_promotion();
    test_program_defaults();

    std::printf("test_unified: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
