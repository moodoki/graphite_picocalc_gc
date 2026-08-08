// Host-side tests for the Phase 4C complex-number subsystem
// (math::Complex, src/math/complex.{hpp,cpp}).

#include <cmath>
#include <cstdio>

#include "math/complex.hpp"

namespace {

using math::Complex;

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

void check_near(double got, double expected, const char* what, double tol = 1e-9) {
    ++g_checks;
    if (std::isnan(got) || std::fabs(got - expected) > tol) {
        std::printf("FAIL: %s -> %.12g (expected %.12g)\n", what, got, expected);
        ++g_failures;
    }
}

void check_c(const Complex& got, double re, double im, const char* what, double tol = 1e-9) {
    check_near(got.re, re, what, tol);
    check_near(got.im, im, what, tol);
}

void test_arithmetic() {
    const Complex a(1, 2);
    const Complex b(3, -1);
    check_c(a + b, 4, 1, "add");
    check_c(a - b, -2, 3, "sub");
    check_c(a * b, 5, 5, "mul");   // (1+2i)(3-i) = 3-i+6i-2i^2 = 3+5i+2 = 5+5i
    check_c(a / b, 0.1, 0.7, "div");  // (1+2i)/(3-i) = (1+2i)(3+i)/10 = (3+i+6i-2)/10 = (1+7i)/10
    check_c(-a, -1, -2, "neg");

    const Complex conj_pair(1, 1);
    check_near((conj_pair * Complex(1, -1)).re, 2, "conj-pair-mul-re");
    check_near((conj_pair * Complex(1, -1)).im, 0, "conj-pair-mul-im");

    check(Complex(3, 0).is_real(), "is_real true");
    check(!Complex(3, 1e-6).is_real(), "is_real false");

    check_near(Complex(3, 4).modulus(), 5.0, "modulus 3-4-5");
    check_near(Complex(1, 1).argument(), M_PI / 4, "argument pi/4");

    check_c(Complex::from_polar(2, M_PI / 2), 0, 2, "from_polar", 1e-9);

    // Division by a large-imaginary divisor exercises the |im|>|re| branch.
    check_c(Complex(1, 0) / Complex(0, 2), 0, -0.5, "div large-im branch");
}

void test_elementary() {
    // sqrt(-4) = 2i
    check_c(math::c_sqrt(Complex(-4, 0)), 0, 2, "sqrt(-4)");
    // sqrt(3+4i) = 2+i
    check_c(math::c_sqrt(Complex(3, 4)), 2, 1, "sqrt(3+4i)");
    // sqrt of a positive real matches the real sqrt
    check_c(math::c_sqrt(Complex(9, 0)), 3, 0, "sqrt(9)");
    check_c(math::c_sqrt(Complex(0, 0)), 0, 0, "sqrt(0)");

    // e^(i*pi) = -1 (Euler)
    check_c(math::c_exp(Complex(0, M_PI)), -1, 0, "e^(i*pi)", 1e-9);
    // e^0 = 1
    check_c(math::c_exp(Complex(0, 0)), 1, 0, "e^0");

    // ln(-1) = i*pi (principal branch)
    check_c(math::c_ln(Complex(-1, 0)), 0, M_PI, "ln(-1)");
    // ln(e) = 1
    check_c(math::c_ln(Complex(M_E, 0)), 1, 0, "ln(e)", 1e-9);

    // (1+i)^2 = 2i
    check_c(math::c_pow(Complex(1, 1), Complex(2, 0)), 0, 2, "(1+i)^2", 1e-9);
    // i^2 = -1
    check_c(math::c_pow(Complex(0, 1), Complex(2, 0)), -1, 0, "i^2", 1e-9);
    // 2^3 = 8 (real path through the complex power)
    check_c(math::c_pow(Complex(2, 0), Complex(3, 0)), 8, 0, "2^3", 1e-8);
}

// D49: a complex base with an integer exponent must go through repeated
// multiplication, not exp(ln), which cannot produce an exact zero component.
// (1+i)^2 returned 1.2246e-16 + 2i on hardware — that real part being exactly
// 2*cos(pi/2) in double.
//
// These assert with tol = 0 deliberately. test_elementary above already
// checked "(1+i)^2 = 2i" and passed throughout, because 1e-9 cannot tell an
// exact zero from a 1e-16 one — the assertion was right and the tolerance
// hid the defect. Exactness is the property under test here, so any nonzero
// tolerance would defeat the point.
void test_integer_powers() {
    const double exact = 0.0;

    check_c(math::c_pow(Complex(1, 1), Complex(2, 0)), 0, 2, "(1+i)^2 exact", exact);
    check_c(math::c_pow(Complex(1, 1), Complex(3, 0)), -2, 2, "(1+i)^3 exact", exact);
    check_c(math::c_pow(Complex(1, 1), Complex(4, 0)), -4, 0, "(1+i)^4 exact", exact);
    check_c(math::c_pow(Complex(3, 4), Complex(2, 0)), -7, 24, "(3+4i)^2 exact", exact);
    check_c(math::c_pow(Complex(0, 2), Complex(2, 0)), -4, 0, "(2i)^2 exact", exact);

    // Powers of i cycle exactly; these were the cases that already worked and
    // must keep working.
    check_c(math::c_pow(Complex(0, 1), Complex(2, 0)), -1, 0, "i^2 exact", exact);
    check_c(math::c_pow(Complex(0, 1), Complex(3, 0)), 0, -1, "i^3 exact", exact);
    check_c(math::c_pow(Complex(0, 1), Complex(4, 0)), 1, 0, "i^4 exact", exact);

    // z^0 = 1, and negative exponents invert the same exact result:
    // (1+i)^-2 = 1/(2i) = -i/2.
    check_c(math::c_pow(Complex(1, 1), Complex(0, 0)), 1, 0, "(1+i)^0 exact", exact);
    check_c(math::c_pow(Complex(1, 1), Complex(-2, 0)), 0, -0.5, "(1+i)^-2 exact", exact);
    check_c(math::c_pow(Complex(0, 1), Complex(-1, 0)), 0, -1, "i^-1 exact", exact);

    // A real base still takes the D46 std::pow branch, not this one.
    check_c(math::c_pow(Complex(2, 0), Complex(3, 0)), 8, 0, "2^3 exact", exact);
    check_c(math::c_pow(Complex(-2, 0), Complex(2, 0)), 4, 0, "(-2)^2 exact", exact);

    // Non-integer and complex exponents still go through exp(ln) — inherently
    // approximate, and no camp-1 system avoids that. Pinned so the fallback
    // is not accidentally removed.
    check_c(math::c_pow(Complex(1, 1), Complex(0.5, 0)), 1.098684113, 0.4550898606,
            "(1+i)^0.5 approx", 1e-9);
    check_c(math::c_pow(Complex(0, 1), Complex(0, 1)), 0.2078795764, 0, "i^i approx", 1e-9);

    // Past the cap the exp(ln) path takes over; the result must still be
    // right, just not bit-exact.
    check_c(math::c_pow(Complex(0, 1), Complex(400, 0)), 1, 0, "i^400 beyond cap", 1e-6);
}

void test_trig() {
    // sin(i) = i*sinh(1)
    check_c(math::c_sin(Complex(0, 1)), 0, std::sinh(1.0), "sin(i)");
    // cos(i) = cosh(1)
    check_c(math::c_cos(Complex(0, 1)), std::cosh(1.0), 0, "cos(i)");
    // sin/cos of a real angle match the real functions
    check_c(math::c_sin(Complex(M_PI / 6, 0)), 0.5, 0, "sin(pi/6)");
    check_c(math::c_cos(Complex(0, 0)), 1, 0, "cos(0)");
    check_c(math::c_tan(Complex(M_PI / 4, 0)), 1, 0, "tan(pi/4)", 1e-9);

    // Round-trip: asin(sin(z)) == z for a modest z (principal branch).
    const Complex z(0.3, 0.4);
    const Complex s = math::c_sin(z);
    check_c(math::c_asin(s), z.re, z.im, "asin(sin(z))", 1e-8);
    const Complex c = math::c_cos(z);
    check_c(math::c_acos(c), z.re, z.im, "acos(cos(z))", 1e-8);
    const Complex t = math::c_tan(z);
    check_c(math::c_atan(t), z.re, z.im, "atan(tan(z))", 1e-8);

    // atan(i) is a pole on the unit circle (not a general regression
    // target) — instead check a real input matches real atan.
    check_c(math::c_atan(Complex(1, 0)), M_PI / 4, 0, "atan(1)", 1e-9);
}

void test_components() {
    check_near(math::c_abs(Complex(3, 4)), 5, "abs(3+4i)");
    check_near(math::c_arg(Complex(1, 1)), M_PI / 4, "arg(1+i)");
    check_c(math::c_conj(Complex(3, 2)), 3, -2, "conj(3+2i)");
    check_near(math::c_real(Complex(3, 2)), 3, "real(3+2i)");
    check_near(math::c_imag(Complex(3, 2)), 2, "imag(3+2i)");
}

}  // namespace

int main() {
    test_arithmetic();
    test_elementary();
    test_integer_powers();
    test_trig();
    test_components();

    std::printf("%d/%d checks passed\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}
