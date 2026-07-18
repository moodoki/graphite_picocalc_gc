#pragma once

#include <cstdint>

#include "math/types.hpp"

// Extended function library registered into the engine (task 2.2).
// All functions take/return calc_t (double) to match tinyexpr.
namespace math::fn {

double sin_am(double x);  // Angle-mode aware trig
double cos_am(double x);
double tan_am(double x);
double asin_am(double x);
double acos_am(double x);
double atan_am(double x);

double log10_ti(double x);  // TI "log(" = base 10
double ln_nat(double x);

double factorial(double n);
double ncr(double n, double r);
double npr(double n, double r);

double rand01();                     // [0, 1)
void seed_rand(std::uint64_t seed);  // 0 falls back to the fixed default seed
double round_n(double x, double n);  // Round to n decimal places
double min2(double a, double b);
double max2(double a, double b);
double deg(double x);  // radians -> degrees
double rad(double x);  // degrees -> radians

}  // namespace math::fn
