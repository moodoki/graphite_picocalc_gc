// Host-side unit tests for the math engine (no hardware required).
// Built and run by scripts/host-tests.sh with the host compiler.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/catalog.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/types.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

void check_near(const char* expr, double expected, double tol = 1e-9) {
    ++g_checks;
    const auto r = math::engine().evaluate(expr);
    if (!r.ok) {
        std::printf("FAIL: '%s' -> error: %s (expected %g)\n", expr, r.error,
                    expected);
        ++g_failures;
        return;
    }
    if (std::fabs(r.value - expected) > tol) {
        std::printf("FAIL: '%s' -> %.12g (expected %.12g)\n", expr, r.value,
                    expected);
        ++g_failures;
    }
}

void check_error(const char* expr) {
    ++g_checks;
    const auto r = math::engine().evaluate(expr);
    if (r.ok) {
        std::printf("FAIL: '%s' evaluated to %g, expected an error\n", expr,
                    r.value);
        ++g_failures;
    }
}

void check_fmt(double x, const char* expected) {
    ++g_checks;
    char buf[40];
    math::format_number(x, buf, sizeof(buf));
    if (std::strcmp(buf, expected) != 0) {
        std::printf("FAIL: format(%.12g) -> '%s' (expected '%s')\n", x, buf,
                    expected);
        ++g_failures;
    }
}

}  // namespace

int main() {
    using math::AngleMode;

    // ---- 2.1 basics ----
    check_near("2+3", 5);
    check_near("2+3*4", 14);
    check_near("(2+3)*4", 20);
    check_near("7/2", 3.5);
    check_near("2^10", 1024);
    check_near("2^3^2", 512);  // Right-associative (TE_POW_FROM_RIGHT)
    check_near("-3^2", -9);    // TI convention
    check_near("sqrt(16)", 4);
    check_near("abs(-5)", 5);
    check_near("pi", 3.14159265358979, 1e-10);
    check_error("2+");
    check_error("(2+3");

    // ---- 2.2 extended functions ----
    check_near("ncr(10,3)", 120);
    check_near("npr(5,2)", 20);
    check_near("5!", 120);
    check_near("(3+2)!", 120);
    check_near("3!!", 720);  // (3!)! = 6! = 720
    check_near("ln(exp(1))", 1);
    check_near("log(1000)", 3);  // TI: log = base 10
    check_near("min(3,7)+max(3,7)", 10);
    check_near("round(3.14159,2)", 3.14);
    check_near("deg(pi)", 180);
    check_near("rad(180)", 3.14159265358979, 1e-10);

    // ---- 2.3 angle mode ----
    math::set_angle_mode(AngleMode::kDegrees);
    check_near("sin(90)", 1);
    check_near("cos(180)", -1);
    check_near("asin(1)", 90);
    math::set_angle_mode(AngleMode::kRadians);
    check_near("sin(pi/2)", 1);
    check_near("2+3*sin(pi/4)", 2 + 3 * std::sin(3.14159265358979323846 / 4),
               1e-9);

    // ---- 2.6 variables + Ans + store ----
    check_near("2->a", 2);
    check_near("a+1", 3);
    check_near("ans+1", 4);  // Ans was 3
    check_near("a*a->b", 4);
    check_near("b", 4);
    check_near("10->theta", 10);
    check_near("theta/2", 5);
    // Case-SENSITIVE entry (2026-07-18): uppercase identifiers are
    // errors, not folded — functions, variables, and store targets are
    // lowercase only.
    check_error("A+B");
    check_error("SIN(0)");
    check_error("Sin(0)");
    check_error("PI");
    check_error("2->A");
    check_error("2->Theta");
    check_near("sin(0)", 0);

    // 'e' is Euler's constant (not a variable — it would shadow the
    // tinyexpr builtin) and can't be stored to; uppercase E is an
    // unknown identifier, but strtod still accepts it inside numeric
    // literals.
    check_near("e", 2.71828182845904523536, 1e-12);
    check_near("ln(e)", 1);
    check_near("e^2", std::exp(2.0), 1e-9);
    check_near("2e3", 2000);  // Scientific literals still parse
    check_near("1e10", 1e10);
    check_near("1E10", 1e10);  // Uppercase-E literal via strtod
    check_error("5->e");
    check_error("5->E");
    check_error("E");

    // Scientific display formatting (HW-found: Pico printf %e can emit
    // unnormalized mantissas; normalize_mantissa must fix them up).
    check_fmt(1e10, "1e10");
    check_fmt(1.5e12, "1.5e12");
    check_fmt(2.5e-7, "2.5e-7");

    // evaluate_at (graphing path) must not clobber Ans or X
    math::engine().evaluate("42->x");
    const auto at = math::engine().evaluate_at("x^2+1", 3.0);
    if (!at.ok || std::fabs(at.value - 10.0) > 1e-12) {
        std::printf("FAIL: evaluate_at x^2+1 @3 -> %g\n", at.value);
        ++g_failures;
    }
    ++g_checks;
    check_near("x", 42);  // Restored

    // compile/eval_compiled (fast graphing path): compile once, sweep X.
    {
        void* h = math::engine().compile("x^2 + 1");
        ++g_checks;
        if (h == nullptr) {
            std::printf("FAIL: compile('x^2+1') returned null\n");
            ++g_failures;
        } else {
            const double vals[] = {-2, 0, 3, 5};
            const double want[] = {5, 1, 10, 26};
            for (int i = 0; i < 4; ++i) {
                ++g_checks;
                const double got = math::engine().eval_compiled(h, vals[i]);
                if (std::fabs(got - want[i]) > 1e-9) {
                    std::printf("FAIL: eval_compiled @%g -> %g (want %g)\n",
                                vals[i], got, want[i]);
                    ++g_failures;
                }
            }
            math::engine().free_compiled(h);
        }
        // Bad expression compiles to null.
        ++g_checks;
        if (math::engine().compile("x^") != nullptr) {
            std::printf("FAIL: compile('x^') should be null\n");
            ++g_failures;
        }
    }
    // eval_compiled leaves X bound to its last argument (the graphing
    // caller owns save/restore); a fresh store re-establishes X.
    math::engine().evaluate("42->x");
    check_near("x", 42);

    // ---- 2.4 format_number ----
    check_fmt(5, "5");
    check_fmt(-17, "-17");
    check_fmt(3.5, "3.5");
    check_fmt(0.5, "0.5");
    check_fmt(1.0 / 3.0, "0.3333333333");
    check_fmt(1e10, "1e10");
    check_fmt(1.5e-7, "1.5e-7");
    check_fmt(0.0001, "0.0001");
    check_fmt(0.00009999, "9.999e-5");
    check_fmt(123456789.5, "123456789.5");
    check_fmt(NAN, "NaN");
    check_fmt(INFINITY, "Inf");
    check_fmt(-INFINITY, "-Inf");
    check_fmt(0, "0");
    check_fmt(0.70710678118654752, "0.7071067812");

    // ---- 5.3 display modes (FIX / SCI) ----
    math::set_display_mode(math::DisplayMode::kFix);
    math::set_fix_digits(2);
    check_fmt(3.14159, "3.14");
    check_fmt(5, "5.00");
    check_fmt(-0.5, "-0.50");
    math::set_fix_digits(0);
    check_fmt(3.7, "4");
    math::set_display_mode(math::DisplayMode::kSci);
    math::set_fix_digits(2);
    check_fmt(12345, "1.23e4");
    check_fmt(0.005, "5.00e-3");
    check_fmt(INFINITY, "Inf");  // Specials still apply in FIX/SCI
    math::set_display_mode(math::DisplayMode::kFloat);
    math::set_fix_digits(2);

    // ---- 5.4 error handling: division by zero yields Inf/NaN, no crash ----
    {
        const auto dz = math::engine().evaluate("1/0");
        ++g_checks;
        if (!dz.ok || !std::isinf(dz.value)) {
            std::printf("FAIL: 1/0 -> ok=%d val=%g (want Inf)\n", dz.ok,
                        dz.value);
            ++g_failures;
        }
        const auto zz = math::engine().evaluate("0/0");
        ++g_checks;
        if (!zz.ok || !std::isnan(zz.value)) {
            std::printf("FAIL: 0/0 -> ok=%d val=%g (want NaN)\n", zz.ok,
                        zz.value);
            ++g_failures;
        }
    }

    // ---- 2.26 function catalog: single source of truth for parser + help ----
    {
        int count = 0;
        const math::FnDescriptor* cat = math::catalog(&count);
        ++g_checks;
        if (count < 17) {
            std::printf("FAIL: catalog count %d (want >= 17)\n", count);
            ++g_failures;
        }
        for (int i = 0; i < count; ++i) {
            ++g_checks;
            if (cat[i].name == nullptr || cat[i].signature == nullptr ||
                cat[i].summary == nullptr || cat[i].fn == nullptr || cat[i].arity < 0 ||
                cat[i].arity > 2) {
                std::printf("FAIL: catalog entry %d malformed\n", i);
                ++g_failures;
                continue;
            }
            // Every signature must parse — the same registration path
            // the calculator uses (a-z are variables, so "ncr(n, r)"
            // is a valid expression as written).
            const auto r = math::engine().evaluate(cat[i].signature);
            if (!r.ok) {
                std::printf("FAIL: catalog signature '%s' does not parse: %s\n",
                            cat[i].signature, r.error);
                ++g_failures;
            }
        }
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
