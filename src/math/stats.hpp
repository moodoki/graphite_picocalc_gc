#pragma once

#include <cstddef>

#include "math/array.hpp"

// Descriptive statistics and regression (sub-phase 3B, spec §4). All
// data access is chunked through the Array get/set API (D22), so a
// 10000-element PSRAM-tier list never needs an SRAM staging buffer.
// Quartiles/medians use a streaming rank-selection (binary search over
// the ordered double bit space) instead of sorting a temp copy — no
// allocation, works identically for weighted data and for the
// median-median group medians.
//
// Structs follow the spec field lists plus the project error
// convention (explicit ok + static error string).
namespace math::stats {

struct OneVarStats {
    bool ok = false;
    const char* error = nullptr;  // Static string when !ok
    int n = 0;                    // Element count (Σfreq when weighted)
    calc_t mean = 0;
    calc_t sum = 0;            // Σx
    calc_t sum_sq = 0;         // Σx²
    calc_t sample_stddev = 0;  // s (n-1 denominator; NaN when n < 2)
    calc_t pop_stddev = 0;     // σ (n denominator)
    calc_t min_val = 0;
    calc_t q1 = 0;
    calc_t median = 0;
    calc_t q3 = 0;
    calc_t max_val = 0;
};

OneVarStats one_var(const Array& data);
// freq must be same-length, non-negative integers (TI rule); n = Σfreq.
OneVarStats one_var_weighted(const Array& data, const Array& freq);

struct TwoVarStats {
    bool ok = false;
    const char* error = nullptr;
    int n = 0;
    calc_t mean_x = 0, mean_y = 0;
    calc_t sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
    calc_t sample_stddev_x = 0, sample_stddev_y = 0;
    calc_t pop_stddev_x = 0, pop_stddev_y = 0;
    calc_t min_x = 0, max_x = 0, min_y = 0, max_y = 0;
};

TwoVarStats two_var(const Array& x, const Array& y);

// The ten TI regression models (spec §4.3).
enum class RegressionType : uint8_t {
    kLinear,        // y = ax + b
    kQuadratic,     // y = ax² + bx + c
    kCubic,         // y = ax³ + bx² + cx + d
    kQuartic,       // y = ax⁴ + ... + e
    kLogarithmic,   // y = a + b·ln(x)
    kExponential,   // y = a·bˣ
    kPower,         // y = a·xᵇ
    kLogistic,      // y = c / (1 + a·e^(−bx))
    kSinusoidal,    // y = a·sin(bx + c) + d
    kMedianMedian,  // resistant line (Tukey three-group)
};
constexpr int kRegressionTypeCount = 10;

struct RegressionResult {
    bool ok = false;
    const char* error = nullptr;
    RegressionType type = RegressionType::kLinear;
    int n = 0;
    calc_t coeffs[5] = {};  // TI order: leading coefficient first
    int coeff_count = 0;
    calc_t r = 0;           // Correlation; NaN where undefined (poly>1,
                            // logistic, sinusoidal, med-med)
    calc_t r_squared = 0;   // 1 - SSE/SST on the original data
    bool converged = true;  // False when LM hit the iteration cap
};

RegressionResult regress(const Array& x, const Array& y, RegressionType type);

// Model value at x (used for the r² residual pass; exposed for tests
// and the stats screen preview).
calc_t eval_model(RegressionType type, const calc_t* coeffs, calc_t x);

// Display metadata.
const char* regression_name(RegressionType t);    // "LinReg", ...
const char* regression_form(RegressionType t);    // "y=a*x+b", ...
const char* coeff_name(RegressionType t, int i);  // "a", "b", ...

// Engine-parseable model expression in x with numeric coefficients,
// e.g. "2.5*x^2-3*x+0.5" — ready for a Y slot (task 3B.8). Sinusoidal
// emits degree-converted b and c when degree_trig is set so the stored
// function matches under the global DEGREE mode (spec §10).
void format_model(const RegressionResult& r, bool degree_trig, char* buf, size_t cap);

}  // namespace math::stats
