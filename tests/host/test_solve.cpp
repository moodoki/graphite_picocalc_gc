// Host-side tests for the Phase 4A numeric solver: numeric_solve /
// numeric_solve_equation and the inline solve() substitution layer.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/engine.hpp"
#include "math/numeric_solve.hpp"
#include "math/solve_expr.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

void check_near(double got, double expected, const char* what, double tol = 1e-8) {
    ++g_checks;
    if (std::isnan(got) || std::fabs(got - expected) > tol) {
        std::printf("FAIL: %s -> %.15g (expected %.15g)\n", what, got, expected);
        ++g_failures;
    }
}

void check_root(const char* expr, double lo, double hi, double expected, const char* what) {
    ++g_checks;
    const auto r = math::numeric_solve(expr, 'x' - 'a', lo, hi);
    if (!r.converged) {
        std::printf("FAIL: %s: '%s' -> %s\n", what, expr, r.error != nullptr ? r.error : "-");
        ++g_failures;
        return;
    }
    if (std::fabs(r.root - expected) > 1e-8) {
        std::printf("FAIL: %s: '%s' -> %.15g (expected %.15g, %d iters)\n", what, expr, r.root,
                    expected, r.iterations);
        ++g_failures;
    }
}

void test_numeric_solve() {
    using namespace math;

    // The spec's acceptance case.
    check_root("x^3-2*x-5", 2, 3, 2.0945514815423265, "cubic acceptance");
    // Root inside a wide bracket.
    check_root("sin(x)", 3, 3.3, M_PI, "sin near pi");
    // Reversed bounds are accepted.
    check_root("x^2-2", 2, 0, std::sqrt(2.0), "reversed bounds");
    // No sign change on [2,10]: Newton fallback from the midpoint.
    check_root("x^2-2", 2, 10, std::sqrt(2.0), "newton fallback");
    // Guess form (lo == hi).
    check_root("x^2-2", 2, 2, std::sqrt(2.0), "newton from guess");

    // Residual and iterations are reported.
    auto r = numeric_solve("x^3-2*x-5", 'x' - 'a', 2, 3);
    check(r.converged && r.residual < 1e-9, "residual small");
    check(r.iterations > 0 && r.iterations <= 100, "iterations sane");

    // Equation form.
    r = numeric_solve_equation("sin(x)", "0.5", 'x' - 'a', 0, 1.2);
    check(r.converged, "equation converged");
    check_near(r.root, std::asin(0.5), "sin(x)=0.5");

    // Theta as the variable.
    r = numeric_solve("cos(theta)", Variables::kTheta, 1, 2);
    check(r.converged, "theta converged");
    check_near(r.root, M_PI / 2, "cos(theta)=0");

    // The solve variable's value is restored afterwards.
    engine().vars()['x'] = 42;
    numeric_solve("x^2-2", 'x' - 'a', 0, 2);
    check_near(engine().vars()['x'], 42.0, "variable restored");

    // Failures.
    r = numeric_solve("x^2+1", 'x' - 'a', -5, 5);
    check(!r.converged && std::strcmp(r.error, "No solution found") == 0, "no real root");
    r = numeric_solve("x^)", 'x' - 'a', 0, 1);
    check(!r.converged && std::strcmp(r.error, "Syntax error") == 0, "syntax error");
}

void check_subst(const char* input, double expected, const char* what) {
    ++g_checks;
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s", input);
    const char* err = nullptr;
    if (!math::solveexpr::substitute(buf, sizeof(buf), &err)) {
        std::printf("FAIL: %s: '%s' -> %s\n", what, input, err != nullptr ? err : "-");
        ++g_failures;
        return;
    }
    const auto r = math::engine().evaluate(buf);
    if (!r.ok || std::fabs(r.value - expected) > 1e-8) {
        std::printf("FAIL: %s: '%s' -> '%s' -> %.15g (expected %.15g)\n", what, input, buf,
                    r.ok ? r.value : NAN, expected);
        ++g_failures;
    }
}

void check_subst_error(const char* input, const char* expected_err, const char* what) {
    ++g_checks;
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s", input);
    const char* err = nullptr;
    if (math::solveexpr::substitute(buf, sizeof(buf), &err)) {
        std::printf("FAIL: %s: '%s' substituted ok (expected error '%s')\n", what, input,
                    expected_err);
        ++g_failures;
        return;
    }
    if (err == nullptr || std::strcmp(err, expected_err) != 0) {
        std::printf("FAIL: %s: '%s' -> '%s' (expected '%s')\n", what, input,
                    err != nullptr ? err : "-", expected_err);
        ++g_failures;
    }
}

void test_solve_expr() {
    using namespace math;

    check(solveexpr::contains_solve("solve(x,x,1)"), "contains_solve positive");
    check(!solveexpr::contains_solve("resolve(x)"), "mid-identifier no match");
    check(!solveexpr::contains_solve("2+2"), "plain expr no match");

    check_subst("solve(x^2-2, x, 0, 2)", std::sqrt(2.0), "bracket form");
    check_subst("solve(x^3-2*x-5, x, 2)", 2.0945514815423265, "guess form");
    check_subst("2*solve(x^2-2, x, 0, 2)+1", 2 * std::sqrt(2.0) + 1, "composes in expression");
    check_subst("solve(sin(x)=0.5, x, 0, 1.2)", std::asin(0.5), "equation form");
    check_subst("solve(x-solve(x^2-4, x, 0, 3), x, 0, 5)", 2.0, "nested solve, inner first");
    check_subst("solve(cos(theta), theta, 1, 2)", M_PI / 2, "theta variable");

    check_subst_error("solve(x^2-2, x)", "solve needs (expr, var, guess) or (expr, var, lo, hi)",
                      "too few args");
    check_subst_error("solve(x^2-2, xy, 0, 2)", "solve var must be a-z (not e/i) or theta",
                      "bad variable");
    check_subst_error("solve(x^2+1, x, 0, 1)", "No solution found", "no root propagates");
    check_subst_error("solve(x^2-2, x, foo, 2)", "Bad solve bound", "bad bound");
}

}  // namespace

int main() {
    test_numeric_solve();
    test_solve_expr();

    std::printf("test_solve: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
