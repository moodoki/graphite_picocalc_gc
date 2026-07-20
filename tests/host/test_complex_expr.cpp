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

    check_err("2i->a", "Complex results can't be stored", "store complex errors");
    check_err("5->E", "Variables are lowercase a-z", "store uppercase errors");
    check_err("5->e", "e is reserved (Euler's e)", "store e errors");
    check_err("5->i", "i is reserved (imaginary unit)", "store i errors");

    const auto t = math::complexexpr::evaluate("7->theta");
    check(t.ok && t.stored_var == math::Variables::kTheta, "store theta");
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
    test_errors();
    test_format_complex();
    test_number_mode_default();

    std::printf("%d/%d checks passed\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}
