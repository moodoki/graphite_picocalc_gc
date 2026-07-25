// Host-side tests for the Phase 4C home-screen complex evaluator
// (math::complexexpr, src/math/complex_expr.{hpp,cpp}) and the
// math::NumberMode plumbing it depends on.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/complex_expr.hpp"
#include "math/engine.hpp"
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

void check_ok(const char* expr, double re, double im, const char* what, double tol = 1e-9) {
    ++g_checks;
    const auto r = math::complexexpr::evaluate(expr);
    if (!r.ok || std::fabs(r.value.re - re) > tol || std::fabs(r.value.im - im) > tol) {
        std::printf("FAIL: %s -> ok=%d (%.9g,%.9g) (expected (%.9g,%.9g)) [%s]\n", expr,
                    r.ok ? 1 : 0, r.value.re, r.value.im, re, im, what);
        ++g_failures;
    }
}

void check_err(const char* expr, const char* expected, const char* what) {
    ++g_checks;
    const auto r = math::complexexpr::evaluate(expr);
    if (r.ok || r.error == nullptr || std::strcmp(r.error, expected) != 0) {
        std::printf("FAIL: %s -> ok=%d err='%s' (expected '%s') [%s]\n", expr, r.ok ? 1 : 0,
                    r.error != nullptr ? r.error : "-", expected, what);
        ++g_failures;
    }
}

void test_mentions_i() {
    check(math::complexexpr::mentions_i("3+2i"), "mentions_i 2i");
    check(math::complexexpr::mentions_i("i"), "mentions_i bare");
    check(math::complexexpr::mentions_i("i*2"), "mentions_i i*2");
    check(!math::complexexpr::mentions_i("sin(3)"), "mentions_i not in sin");
    check(!math::complexexpr::mentions_i("fix"), "mentions_i not in fix");
    check(!math::complexexpr::mentions_i("2+3"), "mentions_i plain");
    check(math::complexexpr::mentions_i("2*i+1"), "mentions_i standalone");
}

void test_arithmetic() {
    check_ok("i", 0, 1, "bare i");
    check_ok("2i", 0, 2, "2i shorthand");
    check_ok("3+2i", 3, 2, "3+2i");
    check_ok("(1+i)^2", 0, 2, "(1+i)^2");
    check_ok("i^2", -1, 0, "i^2");
    check_ok("i*i", -1, 0, "i*i");
    check_ok("(3+4i)*(3-4i)", 25, 0, "conjugate product");
    check_ok("1/i", 0, -1, "1/i");
    check_ok("-i", 0, -1, "unary minus i");
    check_ok("2^3", 8, 0, "real power stays real", 1e-8);
    // Postfix factorial must work on the complex path too (shares
    // Engine's `!`->fac() rewrite; regression for the non-REAL-mode
    // syntax-error bug root-caused 2026-07-22).
    check_ok("5!", 120, 0, "postfix factorial");
    check_ok("3!+2", 8, 0, "factorial then add");
    check_ok("(2+1)!", 6, 0, "factorial of paren group");
    check_ok("2*4!", 48, 0, "factorial binds before multiply");
}

void test_functions() {
    check_ok("sqrt(-4)", 0, 2, "sqrt(-4)");
    check_ok("sqrt(9)", 3, 0, "sqrt(9)");
    check_ok("abs(3+4i)", 5, 0, "abs(3+4i)");
    check_ok("arg(1+i)", M_PI / 4, 0, "arg(1+i)");
    check_ok("conj(3+2i)", 3, -2, "conj(3+2i)");
    check_ok("real(3+2i)", 3, 0, "real(3+2i)");
    check_ok("imag(3+2i)", 2, 0, "imag(3+2i)");
    check_ok("e^(i*pi)", -1, 0, "e^(i*pi) via scalar-span pi", 1e-9);
}

void test_scalar_span_fallback() {
    // Identifiers/functions this evaluator doesn't special-case route
    // through eval_field (real engine), same technique as matexpr.
    check_ok("pi", M_PI, 0, "pi via span");
    check_ok("2*pi", 2 * M_PI, 0, "2*pi via span");
    check_ok("ncr(5,2)", 10, 0, "ncr via span");
    check_ok("round(3.456,1)", 3.5, 0, "round via span");
}

void test_store() {
    const auto r = math::complexexpr::evaluate("5->a");
    check(r.ok && r.stored_var == 0 && r.value.is_real() && r.value.re == 5, "store real to a");

    // Complex stores are allowed since 4D.15 (the dispatch layer
    // commits them into the widened Variables).
    const auto c = math::complexexpr::evaluate("2i->a");
    check(c.ok && c.stored_var == 0 && c.value.re == 0 && c.value.im == 2, "store complex to a");
    check_err("5->E", "Variables are lowercase a-z", "store uppercase errors");
    check_err("5->e", "e is reserved (Euler's e)", "store e errors");
    check_err("5->i", "i is reserved (imaginary unit)", "store i errors");

    const auto t = math::complexexpr::evaluate("7->theta");
    check(t.ok && t.stored_var == math::Variables::kTheta, "store theta");
}

// 4D.15: complex-valued Variables — the widened storage, the
// complex evaluator resolving such variables, and every real-only
// consumer erroring instead of silently reading the real part (D37).
void test_complex_vars() {
    auto& vars = math::engine().vars();

    vars.set_complex('a' - 'a', 3, 2);  // a = 3+2i
    check(vars.is_complex(0), "is_complex after set_complex");

    check_ok("a", 3, 2, "read complex var");
    check_ok("a+1", 4, 2, "complex var in expression");
    check_ok("a*a", 5, 12, "complex var squared");
    check_ok("conj(a)", 3, -2, "conj of complex var");

    // Real-only consumers error (never truncate).
    const auto er = math::engine().evaluate("a+1");
    check(!er.ok && er.error != nullptr && std::strcmp(er.error, "Non-real variable") == 0,
          "real engine errors on complex var");
    math::calc_t out = 0;
    check(!math::eval_field("a+1", &out), "eval_field errors on complex var");
    check(math::engine().compile("a+1") == nullptr, "compile errors on complex var");
    check_err("fac(a)", "Non-real variable", "opaque span with complex var errors");

    check(math::refs_complex_var("a+1"), "refs_complex_var hit");
    check(!math::refs_complex_var("b+1"), "refs_complex_var miss");
    check(!math::refs_complex_var("abs(1)"), "refs_complex_var ignores fn names");

    // Ans and theta slots participate too.
    vars.set_complex(math::Variables::kAns, 0, 1);  // Ans = i
    check_ok("ans*ans", -1, 0, "complex ans");
    check(math::refs_complex_var("ans+1"), "refs_complex_var ans");
    vars.set_real(math::Variables::kAns, 0);

    // Sweep-slot exclusion: a stale complex x must not block compiling
    // an expression about to sweep x — and the sweep evaluates on the
    // real parts it writes.
    vars.set_complex('x' - 'a', 0, 5);
    check(math::engine().compile("sin(x)") == nullptr, "complex x blocks plain compile");
    void* h = math::engine().compile("x*x", 'x' - 'a');
    check(h != nullptr, "sweep compile skips the sweep slot");
    if (h != nullptr) {
        const math::calc_t v = math::engine().eval_compiled(h, 'x' - 'a', 3.0);
        check(std::fabs(v - 9.0) < 1e-12, "sweep eval uses swept real value");
        math::engine().free_compiled(h);
    }
    vars.set_real('x' - 'a', 0);

    // A real store clears the complex tag.
    const auto rs = math::engine().evaluate("7->a");
    check(rs.ok && !vars.is_complex(0) && vars.vars[0] == 7, "real store clears imag");

    // evaluate_at preserves a complex x across unrelated evaluations.
    vars.set_complex('x' - 'a', 1, 4);
    const auto ea = math::engine().evaluate_at("2+2", 5.0);
    check(ea.ok && ea.value == 4, "evaluate_at ok with complex x");
    check(vars.is_complex('x' - 'a') && vars.imag['x' - 'a'] == 4,
          "evaluate_at restores complex x");
    vars.set_real('x' - 'a', 0);
}

void test_errors() {
    check_err("", "Syntax error", "empty");
    check_err("3+", "Syntax error", "trailing operator");
    check_err("(1+2", "Syntax error", "unbalanced paren");
    check_err("sqrt(", "Syntax error", "unbalanced call");
    check_err("bogusfn(1+i)", "Syntax error", "unknown fn with complex arg");
}

// format_complex now emits the font's real angle/imag-i glyph bytes
// (testdrive 2026-07-20). Expected strings stay readable ASCII here;
// this maps the ASCII 'i'/'<' placeholders to those glyph bytes so the
// comparison verifies the glyphs are actually emitted.
void to_glyphs(const char* ascii, char* out, size_t out_len) {
    size_t i = 0;
    for (; ascii[i] != 0 && i + 1 < out_len; ++i) {
        out[i] = ascii[i] == 'i'   ? math::kImagUnitGlyph
                 : ascii[i] == '<' ? math::kAngleGlyph
                                   : ascii[i];
    }
    out[i] = 0;
}

void check_fmt_rect(double re, double im, const char* expected, const char* what) {
    ++g_checks;
    char buf[32];
    char want[32];
    to_glyphs(expected, want, sizeof(want));
    math::format_complex(math::Complex(re, im), math::NumberMode::kRectangular, buf, sizeof(buf));
    if (std::strcmp(buf, want) != 0) {
        std::printf("FAIL: format_complex(%.9g,%.9g) -> '%s' (expected '%s') [%s]\n", re, im, buf,
                    expected, what);
        ++g_failures;
    }
}

void test_format_complex() {
    check_fmt_rect(4, 0, "4", "pure real");
    check_fmt_rect(0, 2, "2i", "pure imag");
    check_fmt_rect(0, 1, "i", "unit imag");
    check_fmt_rect(0, -1, "-i", "unit neg imag");
    check_fmt_rect(3, 2, "3 + 2i", "general");
    check_fmt_rect(-1.5, -0.5, "-1.5 - 0.5i", "negative both");
    check_fmt_rect(1, 1, "1 + i", "unit imag with real part");

    char buf[32];
    char want[32];
    math::format_complex(math::Complex(2, 2), math::NumberMode::kPolar, buf, sizeof(buf));
    to_glyphs("2.828427125<0.7853981634", want, sizeof(want));
    check(std::strcmp(buf, want) == 0, "polar 2+2i");
}

void test_number_mode_default() {
    ++g_checks;
    if (math::number_mode() != math::NumberMode::kReal) {
        std::printf("FAIL: number_mode default should be kReal\n");
        ++g_failures;
    }
    math::set_number_mode(math::NumberMode::kRectangular);
    check(math::number_mode() == math::NumberMode::kRectangular, "set_number_mode rect");
    math::set_number_mode(math::NumberMode::kReal);
}

}  // namespace

int main() {
    test_mentions_i();
    test_arithmetic();
    test_functions();
    test_scalar_span_fallback();
    test_store();
    test_complex_vars();
    test_errors();
    test_format_complex();
    test_number_mode_default();

    std::printf("%d/%d checks passed\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}
