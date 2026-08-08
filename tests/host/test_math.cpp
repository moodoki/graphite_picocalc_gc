// Host-side unit tests for the math engine (no hardware required).
// Built and run by scripts/host-tests.sh with the host compiler.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/catalog.hpp"
#include "math/engine.hpp"
#include "math/frac.hpp"
#include "math/functions.hpp"
#include "math/format.hpp"
#include "math/types.hpp"

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

void check_compact(double x, const char* expected) {
    ++g_checks;
    char buf[40];
    math::format_number_compact(x, buf, sizeof(buf));
    if (std::strcmp(buf, expected) != 0) {
        std::printf("FAIL: format_compact(%.12g) -> '%s' (expected '%s')\n", x,
                    buf, expected);
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

    // Parse-nesting cap (D47). tinyexpr's recursive-descent parser costs
    // ~200 B of stack per nesting level and had no limit of its own, so a
    // deeply nested stored expression walked off core 0's 4 KB — this is
    // the defect behind the 2026-08-05 Y=-editor lockup, whose slot held a
    // 20-deep nesting stress probe. Must be a clean error, never a crash.
    {
        // Seven levels is the documented cap and has to keep working.
        char ok_expr[64] = "sin(cos(sin(cos(sin(cos(sin(1)))))))";
        ++g_checks;
        void* h = math::engine().compile(ok_expr);
        if (h == nullptr) {
            std::printf("FAIL: 7-level nesting should compile\n");
            ++g_failures;
        } else {
            math::engine().free_compiled(h);
        }

        // Build one deeper than the cap, then far deeper, from a loop so
        // the test does not depend on hand-counting parens.
        const int depths[] = {8, 20, 40};
        for (const int depth : depths) {
            char deep[512];
            std::size_t w = 0;
            for (int i = 0; i < depth; ++i) {
                w += static_cast<std::size_t>(std::snprintf(deep + w, sizeof(deep) - w, "sin("));
            }
            w += static_cast<std::size_t>(std::snprintf(deep + w, sizeof(deep) - w, "1"));
            for (int i = 0; i < depth; ++i) {
                w += static_cast<std::size_t>(std::snprintf(deep + w, sizeof(deep) - w, ")"));
            }
            ++g_checks;
            if (math::engine().compile(deep) != nullptr) {
                std::printf("FAIL: %d-level nesting should be rejected\n", depth);
                ++g_failures;
            }
            ++g_checks;
            const auto res = math::engine().evaluate(deep);
            if (res.ok || res.error == nullptr ||
                std::strcmp(res.error, "Too deeply nested") != 0) {
                std::printf("FAIL: evaluate(%d-level) -> ok=%d error=%s\n", depth,
                            static_cast<int>(res.ok), res.error != nullptr ? res.error : "(null)");
                ++g_failures;
            }
        }
    }

    // The binding table is shared across calls and lives in bss, not on
    // the caller's stack (D47). compile_with appends its extras past the
    // shared tail, so a plain compile afterwards must not see them, and
    // extras from one compile_with must not leak into the next.
    {
        double l1 = 7;
        double l2 = 11;
        const math::Engine::ExtraVar ex1[] = {{"l1", &l1}};
        const math::Engine::ExtraVar ex2[] = {{"l2", &l2}};

        void* a = math::engine().compile_with("l1*2", ex1, 1);
        ++g_checks;
        if (a == nullptr || std::fabs(math::engine().eval_compiled_raw(a) - 14.0) > 1e-12) {
            std::printf("FAIL: compile_with('l1*2') with extras\n");
            ++g_failures;
        }
        math::engine().free_compiled(a);

        // A plain compile must not still see l1.
        ++g_checks;
        void* leaked = math::engine().compile("l1*2");
        if (leaked != nullptr) {
            std::printf("FAIL: extras leaked into a plain compile\n");
            ++g_failures;
            math::engine().free_compiled(leaked);
        }

        // A second compile_with with a *different* extra must not still
        // see the first one either.
        void* b = math::engine().compile_with("l2*2", ex2, 1);
        ++g_checks;
        if (b == nullptr || std::fabs(math::engine().eval_compiled_raw(b) - 22.0) > 1e-12) {
            std::printf("FAIL: compile_with('l2*2') after a different extras set\n");
            ++g_failures;
        }
        math::engine().free_compiled(b);
        ++g_checks;
        void* stale = math::engine().compile_with("l1*2", ex2, 1);
        if (stale != nullptr) {
            std::printf("FAIL: stale extra l1 survived into the next compile_with\n");
            ++g_failures;
            math::engine().free_compiled(stale);
        }

        // Standard bindings still resolve after all that.
        ++g_checks;
        void* plain = math::engine().compile("2+3");
        if (plain == nullptr || std::fabs(math::engine().eval_compiled_raw(plain) - 5.0) > 1e-12) {
            std::printf("FAIL: plain compile broken after compile_with\n");
            ++g_failures;
        }
        math::engine().free_compiled(plain);
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

    // ---- format_number_compact (4 sig figs for fractional floats;
    // integers / sci-range / FIX-SCI defer to format_number) ----
    check_compact(1.0 / 3.0, "0.3333");
    check_compact(0.70710678118654752, "0.7071");
    check_compact(2.5, "2.5");
    check_compact(5, "5");          // integer: unchanged
    check_compact(-17, "-17");      // integer: unchanged
    check_compact(1e10, "1e10");    // sci range: unchanged
    check_compact(1.5e-7, "1.5e-7");
    // Fractional-but-large defers to %.4g (raw exponent, not normalized).
    check_compact(123456789.5, "1.235e+08");

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
    // ENG (4D.1): exponents are multiples of 3, mantissa in [1, 1000).
    math::set_display_mode(math::DisplayMode::kEng);
    check_fmt(12345, "12.345e3");
    check_fmt(0.005, "5e-3");
    check_fmt(1000, "1e3");
    check_fmt(999, "999e0");
    check_fmt(-2500000, "-2.5e6");
    check_fmt(0.0001234, "123.4e-6");
    check_fmt(5, "5e0");
    check_fmt(0, "0");
    math::set_display_mode(math::DisplayMode::kFloat);
    math::set_fix_digits(2);

    // ---- 4D.2 fractions (frac.hpp) ----
    {
        long p = 0;
        long q = 1;
        check(math::frac::decimal_to_fraction(0.75, 10000, &p, &q) && p == 3 && q == 4,
              "0.75 = 3/4");
        check(math::frac::decimal_to_fraction(-1.0 / 3.0, 10000, &p, &q) && p == -1 && q == 3,
              "-1/3 detected");
        check(math::frac::decimal_to_fraction(5, 10000, &p, &q) && p == 5 && q == 1,
              "integer is /1");
        check(!math::frac::decimal_to_fraction(0.7071067811865476, 10000, &p, &q),
              "irrational refused");
        char fb[24];
        check(math::frac::format_fraction(0.4, 10000, fb, sizeof(fb)) &&
                  std::strcmp(fb, "2/5") == 0,
              "format 2/5");
        check(math::frac::pi_multiple(3.14159265358979 / 2, 12, 6, &p, &q) && p == 1 && q == 2,
              "pi/2 detected");
        check(math::frac::pi_multiple(3 * 3.14159265358979323846 / 4, 12, 6, &p, &q) && p == 3 &&
                  q == 4,
              "3pi/4 detected");
        check(!math::frac::pi_multiple(3.0, 12, 6, &p, &q), "3 is not a pi multiple");
        check(!math::frac::pi_multiple(0.0, 12, 6, &p, &q), "0 excluded");
    }

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
                cat[i].summary == nullptr) {
                std::printf("FAIL: catalog entry %d malformed\n", i);
                ++g_failures;
                continue;
            }
            // Help-only rows (Phase 3A list functions, fn == nullptr)
            // are not engine-registered; their signatures take list
            // args and don't parse as scalar expressions.
            if (cat[i].fn == nullptr) {
                continue;
            }
            ++g_checks;
            if (cat[i].arity < 0 || cat[i].arity > 7) {  // TE_FUNCTION7 cap
                std::printf("FAIL: catalog entry %d bad arity\n", i);
                ++g_failures;
                continue;
            }
            // Every registered function must be callable at its
            // declared arity — the same registration path the
            // calculator uses. (The signatures themselves use
            // descriptive parameter names since 3C — "normal_cdf(lo,
            // hi,mu,sd)" — which are help text, not variables, so
            // build a numeric call instead of parsing the signature.)
            char call[64];
            int pos = std::snprintf(call, sizeof(call), "%s(", cat[i].name);
            for (int a = 0; a < cat[i].arity; ++a) {
                pos += std::snprintf(call + pos, sizeof(call) - static_cast<size_t>(pos),
                                     "%s0.5", a > 0 ? "," : "");
            }
            std::snprintf(call + pos, sizeof(call) - static_cast<size_t>(pos), ")");
            const auto r = math::engine().evaluate(call);
            if (!r.ok) {
                std::printf("FAIL: catalog call '%s' does not parse: %s\n", call, r.error);
                ++g_failures;
            }
        }
    }

    // ---- rand01 seeding (xorshift64*) ----
    {
        math::fn::seed_rand(42);
        double a[4];
        for (double& v : a) {
            v = math::fn::rand01();
            ++g_checks;
            if (v < 0.0 || v >= 1.0) {
                std::printf("FAIL: rand01 %.12g out of [0, 1)\n", v);
                ++g_failures;
            }
        }
        // Same seed replays the same sequence; a different seed diverges.
        math::fn::seed_rand(42);
        ++g_checks;
        if (math::fn::rand01() != a[0] || math::fn::rand01() != a[1]) {
            std::printf("FAIL: rand01 not deterministic under a fixed seed\n");
            ++g_failures;
        }
        math::fn::seed_rand(43);
        ++g_checks;
        if (math::fn::rand01() == a[0]) {
            std::printf("FAIL: rand01 identical under different seeds\n");
            ++g_failures;
        }
    }

    // ---- 3C distributions through the parser (arity 2-4 registration;
    // values themselves are covered by test_dist) ----
    check_near("normal_cdf(-1, 1, 0, 1)", 0.6826894921370859, 1e-12);
    check_near("normal_pdf(0, 0, 1)", 0.39894228040143268, 1e-12);
    check_near("normal_inv(0.975, 0, 1)", 1.9599639845400542, 1e-8);
    check_near("t_cdf(-2, 2, 5)", 0.89806052117014164, 1e-12);
    check_near("chisq_inv(0.95, 1)", 3.841458820694126, 1e-7);
    check_near("f_cdf(0, 2.5, 3, 12)", 0.89084528760499372, 1e-12);
    check_near("binomial_pmf(3, 10, 0.5)", 0.1171875, 1e-15);
    check_near("poisson_cdf(2, 3)", 0.42319008112684352, 1e-12);
    check_near("geometric_cdf(3, 0.2)", 0.488, 1e-15);
    check_near("2*normal_cdf(-1e99, 1.96, 0, 1)-1", 0.95000420970355911, 1e-12);

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
