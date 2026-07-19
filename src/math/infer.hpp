#pragma once

#include <cstdint>

#include "math/array.hpp"

// Inference: hypothesis tests + confidence intervals (sub-phase 3D,
// spec §6.1-6.2; conventions D27). All list access streams through the
// Array chunk API, so PSRAM-tier lists need no staging buffers.
//
// Conventions (D27):
//  - Tests take the alternative hypothesis (Alt); p-values use the
//    dist survival functions directly for far-tail precision.
//  - Tests do NOT bundle confidence intervals (P3-6): intervals are
//    their own functions, TI-style — the inference screen offers both.
//  - chi-square 2-way and ANOVA take their table columns / groups as
//    an array of Array pointers (the l1..l6 lists) — there is no 2-D
//    Array until the Phase 4 Matrix (deviation from the spec sketch).
//  - Counts (n, successes) are int; the UI validates integer entry.
namespace math::stats {

enum class Alt : uint8_t { kNotEqual, kLess, kGreater };

struct TestResult {
    bool ok = false;
    const char* error = nullptr;  // Static string when !ok
    calc_t statistic = 0;         // z, t, chi2, or F
    calc_t p_value = 0;
    calc_t df = 0;        // NaN where not applicable (z tests)
    calc_t df2 = 0;       // Second df (F/ANOVA), else NaN
    calc_t estimate = 0;  // Point estimate (x-bar, diff, p-hat, slope)
    calc_t se = 0;        // Standard error used by the statistic
    int n1 = 0;           // Sample size(s) for display
    int n2 = 0;
};

struct Interval {
    bool ok = false;
    const char* error = nullptr;
    calc_t point_estimate = 0;
    calc_t low = 0;
    calc_t high = 0;
    calc_t margin_of_error = 0;
    calc_t confidence = 0;
};

// --- z-tests (population sd known) ---
TestResult z_test_1samp(calc_t x_bar, calc_t mu0, calc_t sigma, int n, Alt alt);
TestResult z_test_2samp(calc_t x1, calc_t s1, int n1, calc_t x2, calc_t s2, int n2, Alt alt);

// --- t-tests ---
TestResult t_test_1samp(const Array& data, calc_t mu0, Alt alt);
TestResult t_test_1samp_summary(calc_t x_bar, calc_t s, int n, calc_t mu0, Alt alt);
TestResult t_test_2samp(const Array& d1, const Array& d2, bool pooled, Alt alt);
TestResult t_test_2samp_summary(calc_t x1, calc_t s1, int n1, calc_t x2, calc_t s2, int n2,
                                bool pooled, Alt alt);
TestResult t_test_paired(const Array& before, const Array& after, Alt alt);

// --- proportion tests ---
TestResult prop_test_1samp(int successes, int n, calc_t p0, Alt alt);
TestResult prop_test_2samp(int x1, int n1, int x2, int n2, Alt alt);

// --- chi-square ---
TestResult chisq_gof(const Array& observed, const Array& expected);
// Contingency table given as equal-length columns (e.g. l1..l3).
TestResult chisq_test_2way(const Array* const cols[], int ncols);

// --- one-way ANOVA (2..6 groups) ---
TestResult anova_oneway(const Array* const groups[], int group_count);

// --- linear regression t-test (H0: slope = 0) ---
TestResult linreg_test(const Array& x, const Array& y, Alt alt);

// --- confidence intervals (conf in (0, 1)) ---
Interval ci_mean_z(calc_t x_bar, calc_t sigma, int n, calc_t conf);
Interval ci_mean_t(const Array& data, calc_t conf);
Interval ci_mean_t_summary(calc_t x_bar, calc_t s, int n, calc_t conf);
Interval ci_diff_means(const Array& d1, const Array& d2, calc_t conf, bool pooled);
Interval ci_diff_means_summary(calc_t x1, calc_t s1, int n1, calc_t x2, calc_t s2, int n2,
                               calc_t conf, bool pooled);
Interval ci_proportion(int successes, int n, calc_t conf);
Interval ci_diff_proportions(int x1, int n1, int x2, int n2, calc_t conf);

}  // namespace math::stats
