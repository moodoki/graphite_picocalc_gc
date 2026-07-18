#include "math/functions.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace math {

namespace {
AngleMode g_angle_mode = AngleMode::kRadians;
constexpr double kPi = 3.14159265358979323846;
}  // namespace

AngleMode angle_mode() {
    return g_angle_mode;
}
void set_angle_mode(AngleMode m) {
    g_angle_mode = m;
}

namespace fn {

namespace {
double to_rad(double x) {
    return g_angle_mode == AngleMode::kDegrees ? x * kPi / 180.0 : x;
}
double from_rad(double x) {
    return g_angle_mode == AngleMode::kDegrees ? x * 180.0 / kPi : x;
}
}  // namespace

double sin_am(double x) {
    return std::sin(to_rad(x));
}
double cos_am(double x) {
    return std::cos(to_rad(x));
}
double tan_am(double x) {
    return std::tan(to_rad(x));
}
double asin_am(double x) {
    return from_rad(std::asin(x));
}
double acos_am(double x) {
    return from_rad(std::acos(x));
}
double atan_am(double x) {
    return from_rad(std::atan(x));
}

double log10_ti(double x) {
    return std::log10(x);
}
double ln_nat(double x) {
    return std::log(x);
}

double factorial(double n) {
    if (n < 0 || n != std::floor(n) || n > 170) {
        return NAN;  // 171! overflows double
    }
    double r = 1.0;
    for (int i = 2; i <= static_cast<int>(n); ++i) {
        r *= i;
    }
    return r;
}

double ncr(double n, double r) {
    if (n < 0 || r < 0 || r > n || n != std::floor(n) || r != std::floor(r)) {
        return NAN;
    }
    // Multiplicative form; stays in range far longer than fac(n)/...
    const double k = r < n - r ? r : n - r;
    double result = 1.0;
    for (int i = 1; i <= static_cast<int>(k); ++i) {
        result = result * (n - k + i) / i;
    }
    return std::round(result);
}

double npr(double n, double r) {
    if (n < 0 || r < 0 || r > n || n != std::floor(n) || r != std::floor(r)) {
        return NAN;
    }
    double result = 1.0;
    for (int i = 0; i < static_cast<int>(r); ++i) {
        result *= (n - i);
    }
    return result;
}

namespace {
// xorshift64* state. The default keeps host tests deterministic when nothing
// seeds; firmware main() reseeds from the SDK entropy source at boot.
constexpr std::uint64_t kDefaultRandSeed = 0x9E3779B97F4A7C15ULL;
std::uint64_t g_rand_state = kDefaultRandSeed;
}  // namespace

void seed_rand(std::uint64_t seed) {
    g_rand_state = seed != 0 ? seed : kDefaultRandSeed;
}

double rand01() {
    std::uint64_t x = g_rand_state;
    x ^= x >> 12U;
    x ^= x << 25U;
    x ^= x >> 27U;
    g_rand_state = x;
    // Top 53 bits of the scrambled output -> uniform double in [0, 1).
    return static_cast<double>((x * 0x2545F4914F6CDD1DULL) >> 11U) / 9007199254740992.0;
}

double round_n(double x, double n) {
    const double p = std::pow(10.0, std::floor(n));
    return std::round(x * p) / p;
}

double min2(double a, double b) {
    return a < b ? a : b;
}
double max2(double a, double b) {
    return a > b ? a : b;
}
double deg(double x) {
    return x * 180.0 / kPi;
}
double rad(double x) {
    return x * kPi / 180.0;
}

}  // namespace fn
}  // namespace math
