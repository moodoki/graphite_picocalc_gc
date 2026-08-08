#include "math/complex.hpp"

#include <cmath>

namespace math {

bool Complex::is_real(calc_t eps) const {
    return std::fabs(im) < eps;
}

Complex Complex::operator+(const Complex& o) const {
    return {re + o.re, im + o.im};
}

Complex Complex::operator-(const Complex& o) const {
    return {re - o.re, im - o.im};
}

Complex Complex::operator*(const Complex& o) const {
    return {re * o.re - im * o.im, re * o.im + im * o.re};
}

Complex Complex::operator/(const Complex& o) const {
    // Smith's algorithm: scale by the larger component to avoid
    // overflow/underflow on extreme magnitudes.
    if (std::fabs(o.re) >= std::fabs(o.im)) {
        const calc_t r = o.im / o.re;
        const calc_t d = o.re + o.im * r;
        return {(re + im * r) / d, (im - re * r) / d};
    }
    const calc_t r = o.re / o.im;
    const calc_t d = o.re * r + o.im;
    return {(re * r + im) / d, (im * r - re) / d};
}

Complex Complex::operator-() const {
    return {-re, -im};
}

calc_t Complex::modulus() const {
    return std::hypot(re, im);
}

calc_t Complex::argument() const {
    return std::atan2(im, re);
}

Complex Complex::from_polar(calc_t r, calc_t theta) {
    return {r * std::cos(theta), r * std::sin(theta)};
}

Complex c_sqrt(const Complex& z) {
    // Numerically stable form (Numerical Recipes 5.4), avoids
    // cancellation for z near the negative real axis.
    if (z.re == 0.0 && z.im == 0.0) {
        return {0.0, 0.0};
    }
    const calc_t m = z.modulus();
    const calc_t w = std::sqrt(0.5 * (m + std::fabs(z.re)));
    if (z.re >= 0.0) {
        return {w, z.im / (2.0 * w)};
    }
    const calc_t im = z.im >= 0.0 ? w : -w;
    return {std::fabs(z.im) / (2.0 * w), im};
}

Complex c_exp(const Complex& z) {
    const calc_t k = std::exp(z.re);
    return {k * std::cos(z.im), k * std::sin(z.im)};
}

Complex c_ln(const Complex& z) {
    return {std::log(z.modulus()), z.argument()};
}

Complex c_pow(const Complex& base, const Complex& exp) {
    if (base.re == 0.0 && base.im == 0.0) {
        return {0.0, 0.0};
    }
    // Real base, real exponent: go straight to pow(). The exp(ln) form below
    // is correct but not exact — 10202^2 came back a hair off 104080804,
    // which is enough to fail format_number's `x == floor(x)` integer test
    // and print "104080805.x" where REAL mode printed "104080805". Two
    // evaluators must not disagree about ordinary real arithmetic (D46);
    // pow() also handles a negative base with an integer exponent, which
    // exp(ln) cannot without going through the complex plane.
    if (base.im == 0.0 && exp.im == 0.0 && (base.re > 0.0 || exp.re == std::floor(exp.re))) {
        return {std::pow(base.re, exp.re), 0.0};
    }
    return c_exp(c_ln(base) * exp);
}

Complex c_sin(const Complex& z) {
    return {std::sin(z.re) * std::cosh(z.im), std::cos(z.re) * std::sinh(z.im)};
}

Complex c_cos(const Complex& z) {
    return {std::cos(z.re) * std::cosh(z.im), -std::sin(z.re) * std::sinh(z.im)};
}

Complex c_tan(const Complex& z) {
    return c_sin(z) / c_cos(z);
}

Complex c_asin(const Complex& z) {
    // -i * ln(iz + sqrt(1 - z^2))
    const Complex i(0.0, 1.0);
    const Complex root = c_sqrt(Complex(1.0) - z * z);
    return -i * c_ln(i * z + root);
}

Complex c_acos(const Complex& z) {
    const Complex i(0.0, 1.0);
    const Complex root = c_sqrt(Complex(1.0) - z * z);
    return -i * c_ln(z + i * root);
}

Complex c_atan(const Complex& z) {
    // (i/2) * ln((1 - iz) / (1 + iz))
    const Complex i(0.0, 1.0);
    const Complex half_i(0.0, 0.5);
    return half_i * c_ln((Complex(1.0) - i * z) / (Complex(1.0) + i * z));
}

calc_t c_abs(const Complex& z) {
    return z.modulus();
}

calc_t c_arg(const Complex& z) {
    return z.argument();
}

Complex c_conj(const Complex& z) {
    return {z.re, -z.im};
}

calc_t c_real(const Complex& z) {
    return z.re;
}

calc_t c_imag(const Complex& z) {
    return z.im;
}

}  // namespace math
