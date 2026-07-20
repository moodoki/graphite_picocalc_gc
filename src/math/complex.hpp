#pragma once

#include <cstddef>

#include "math/types.hpp"

// Complex-number subsystem (Phase 4C, phase4-spec.md §5). Backs the
// home-screen complex evaluator (math::complexexpr) and, per decision
// D30, the matrix eigenvalue solver's conjugate-pair output. Graphing,
// tables, and stats never touch this type — they stay on the real-only
// fast path (D30 / spec §5.2 performance note).
namespace math {

struct Complex {
    calc_t re = 0.0;
    calc_t im = 0.0;

    Complex() = default;
    Complex(calc_t real) : re(real), im(0.0) {}
    Complex(calc_t real, calc_t imag) : re(real), im(imag) {}

    bool is_real(calc_t eps = 1e-12) const;

    Complex operator+(const Complex& o) const;
    Complex operator-(const Complex& o) const;
    Complex operator*(const Complex& o) const;
    Complex operator/(const Complex& o) const;
    Complex operator-() const;

    calc_t modulus() const;   // |z| = sqrt(re^2 + im^2)
    calc_t argument() const;  // arg(z) in radians, always -pi..pi

    static Complex from_polar(calc_t r, calc_t theta);
};

// Complex-aware elementary functions.
Complex c_sqrt(const Complex& z);
Complex c_exp(const Complex& z);
Complex c_ln(const Complex& z);  // Principal branch
Complex c_pow(const Complex& base, const Complex& exp);
Complex c_sin(const Complex& z);
Complex c_cos(const Complex& z);
Complex c_tan(const Complex& z);
Complex c_asin(const Complex& z);
Complex c_acos(const Complex& z);
Complex c_atan(const Complex& z);

// Component/utility functions.
calc_t c_abs(const Complex& z);  // modulus
calc_t c_arg(const Complex& z);  // argument, radians
Complex c_conj(const Complex& z);
calc_t c_real(const Complex& z);
calc_t c_imag(const Complex& z);

}  // namespace math
