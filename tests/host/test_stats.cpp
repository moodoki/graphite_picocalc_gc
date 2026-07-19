// Host-side tests for the Phase 3B statistics stack: 1-var/2-var
// descriptive stats (rank-selection quartiles) and all ten regression
// models. PSRAM-tier arrays run against the malloc shim in
// host_psram_backend.cpp. Model strings are additionally checked to
// compile in the expression engine (they feed Y slots, task 3B.8).

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/array.hpp"
#include "math/engine.hpp"
#include "math/stats.hpp"

namespace {

using math::Array;
using math::calc_t;
namespace stats = math::stats;
using stats::RegressionType;

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

void fill(Array& a, const double* v, int n) {
    if (!a.resize(n)) {
        std::printf("FAIL: fill resize(%d)\n", n);
        ++g_failures;
        return;
    }
    a.write_range(0, n, v);
}

Array g_a;
Array g_b;

void test_one_var() {
    const double d[] = {2, 4, 4, 4, 5, 5, 7, 9};
    fill(g_a, d, 8);
    auto s = stats::one_var(g_a);
    check(s.ok, "one_var ok");
    check(s.n == 8, "one_var n");
    check_near(s.mean, 5.0, "one_var mean");
    check_near(s.sum, 40.0, "one_var sum");
    check_near(s.sum_sq, 232.0, "one_var sum_sq");
    check_near(s.pop_stddev, 2.0, "one_var pop sd");
    check_near(s.sample_stddev, std::sqrt(32.0 / 7.0), "one_var sample sd");
    check_near(s.min_val, 2.0, "one_var min");
    check_near(s.max_val, 9.0, "one_var max");
    check_near(s.median, 4.5, "one_var median");
    check_near(s.q1, 4.0, "one_var q1");
    check_near(s.q3, 6.0, "one_var q3");

    // Odd count: median is the middle element, halves exclude it.
    const double d7[] = {9, 1, 3, 8, 3, 6, 7};  // unsorted on purpose
    fill(g_a, d7, 7);
    s = stats::one_var(g_a);
    check_near(s.median, 6.0, "one_var odd median");
    check_near(s.q1, 3.0, "one_var odd q1");
    check_near(s.q3, 8.0, "one_var odd q3");

    // Single element: quartiles undefined, sample sd undefined.
    const double d1[] = {42};
    fill(g_a, d1, 1);
    s = stats::one_var(g_a);
    check(s.ok, "one_var single ok");
    check_near(s.median, 42.0, "one_var single median");
    check(std::isnan(s.q1) && std::isnan(s.q3), "one_var single quartiles NaN");
    check(std::isnan(s.sample_stddev), "one_var single sample sd NaN");

    // Errors.
    g_a.resize(0);
    check(!stats::one_var(g_a).ok, "one_var empty errors");
    const double dn[] = {1, NAN, 3};
    fill(g_a, dn, 3);
    check(std::strcmp(stats::one_var(g_a).error, "Non-finite data") == 0, "one_var NaN errors");
}

void test_one_var_weighted() {
    // Same multiset as the 8-element test above, via frequencies.
    const double d[] = {2, 4, 5, 7, 9};
    const double f[] = {1, 3, 2, 1, 1};
    fill(g_a, d, 5);
    fill(g_b, f, 5);
    auto s = stats::one_var_weighted(g_a, g_b);
    check(s.ok, "weighted ok");
    check(s.n == 8, "weighted n");
    check_near(s.mean, 5.0, "weighted mean");
    check_near(s.pop_stddev, 2.0, "weighted pop sd");
    check_near(s.median, 4.5, "weighted median");
    check_near(s.q1, 4.0, "weighted q1");
    check_near(s.q3, 6.0, "weighted q3");

    // Zero frequency excludes the element (min/max too).
    const double f0[] = {0, 3, 2, 1, 1};
    fill(g_b, f0, 5);
    s = stats::one_var_weighted(g_a, g_b);
    check(s.n == 7, "freq-0 n");
    check_near(s.min_val, 4.0, "freq-0 min excludes");
    check_near(s.median, 5.0, "freq-0 median");

    // Errors: length mismatch, non-integer freq.
    fill(g_b, f, 4);
    check(std::strcmp(stats::one_var_weighted(g_a, g_b).error, "List length mismatch") == 0,
          "weighted length mismatch");
    const double fbad[] = {1, 1.5, 1, 1, 1};
    fill(g_b, fbad, 5);
    check(std::strcmp(stats::one_var_weighted(g_a, g_b).error, "Freq must be int >= 0") == 0,
          "weighted non-integer freq");
}

void test_one_var_psram() {
    // 1..1000 exercises the PSRAM tier + the multi-rank selection.
    if (!g_a.resize(1000)) {
        check(false, "psram resize");
        return;
    }
    for (int i = 0; i < 1000; ++i) {
        g_a.set(i, static_cast<calc_t>(i + 1));
    }
    check(g_a.in_psram(), "1000-elem in PSRAM");
    const auto s = stats::one_var(g_a);
    check(s.ok, "psram one_var ok");
    check_near(s.mean, 500.5, "psram mean");
    check_near(s.median, 500.5, "psram median");
    check_near(s.q1, 250.5, "psram q1");
    check_near(s.q3, 750.5, "psram q3");
    check_near(s.pop_stddev, std::sqrt((1000.0 * 1000.0 - 1) / 12.0), "psram pop sd", 1e-6);
}

void test_two_var() {
    const double x[] = {1, 2, 3, 4, 5};
    const double y[] = {2, 4, 5, 4, 5};
    fill(g_a, x, 5);
    fill(g_b, y, 5);
    const auto s = stats::two_var(g_a, g_b);
    check(s.ok, "two_var ok");
    check(s.n == 5, "two_var n");
    check_near(s.mean_x, 3.0, "two_var mean_x");
    check_near(s.mean_y, 4.0, "two_var mean_y");
    check_near(s.sum_xy, 66.0, "two_var sum_xy");
    check_near(s.sample_stddev_x, std::sqrt(10.0 / 4.0), "two_var sx");
    check_near(s.min_y, 2.0, "two_var min_y");
    check_near(s.max_x, 5.0, "two_var max_x");

    fill(g_b, y, 4);
    check(!stats::two_var(g_a, g_b).ok, "two_var length mismatch");
}

stats::RegressionResult fit(const double* x, const double* y, int n, RegressionType t) {
    fill(g_a, x, n);
    fill(g_b, y, n);
    return stats::regress(g_a, g_b, t);
}

// Every model string must compile in the engine — it feeds a Y slot.
void check_model_compiles(const stats::RegressionResult& r, const char* what) {
    ++g_checks;
    char buf[128];
    stats::format_model(r, false, buf, sizeof(buf));
    void* h = math::engine().compile(buf);
    if (h == nullptr) {
        std::printf("FAIL: %s model does not compile: '%s'\n", what, buf);
        ++g_failures;
        return;
    }
    // And it evaluates to the same value as eval_model.
    const calc_t at2 = math::engine().eval_compiled(h, 2.0);
    math::engine().free_compiled(h);
    const calc_t want = stats::eval_model(r.type, r.coeffs, 2.0);
    if (std::fabs(at2 - want) > 1e-6 * (1 + std::fabs(want))) {
        std::printf("FAIL: %s model/engine mismatch at x=2: %g vs %g ('%s')\n", what,
                    static_cast<double>(at2), static_cast<double>(want), buf);
        ++g_failures;
    }
}

void test_linear_reg() {
    const double x[] = {1, 2, 3, 4, 5};
    const double y[] = {2, 4, 5, 4, 5};
    const auto r = fit(x, y, 5, RegressionType::kLinear);
    check(r.ok, "linreg ok");
    check_near(r.coeffs[0], 0.6, "linreg slope");
    check_near(r.coeffs[1], 2.2, "linreg intercept");
    check_near(r.r, 30.0 / std::sqrt(50.0 * 30.0), "linreg r");
    check_near(r.r_squared, 0.6, "linreg r^2");
    check_model_compiles(r, "linreg");

    // Exact fit, awkward magnitudes (centering/scaling must hold up).
    double xb[20];
    double yb[20];
    for (int i = 0; i < 20; ++i) {
        xb[i] = 2000 + i;
        yb[i] = -3.5 * xb[i] + 12345;
    }
    const auto rb = fit(xb, yb, 20, RegressionType::kLinear);
    check_near(rb.coeffs[0], -3.5, "linreg exact slope", 1e-7);
    check_near(rb.coeffs[1], 12345.0, "linreg exact intercept", 1e-3);
    check_near(rb.r_squared, 1.0, "linreg exact r^2", 1e-9);

    // Degenerate: all x equal.
    const double xs[] = {2, 2, 2};
    const double ys[] = {1, 2, 3};
    check(std::strcmp(fit(xs, ys, 3, RegressionType::kLinear).error, "Singular matrix") == 0,
          "linreg singular");
}

void test_poly_reg() {
    // Quadratic, exact: y = x^2 - 2x + 3.
    const double x[] = {-2, -1, 0, 1, 2, 3};
    double y[6];
    for (int i = 0; i < 6; ++i) {
        y[i] = x[i] * x[i] - 2 * x[i] + 3;
    }
    auto r = fit(x, y, 6, RegressionType::kQuadratic);
    check(r.ok, "quadreg ok");
    check(r.coeff_count == 3, "quadreg coeff count");
    check_near(r.coeffs[0], 1.0, "quadreg a", 1e-8);
    check_near(r.coeffs[1], -2.0, "quadreg b", 1e-8);
    check_near(r.coeffs[2], 3.0, "quadreg c", 1e-8);
    check_near(r.r_squared, 1.0, "quadreg r^2", 1e-9);
    check(std::isnan(r.r), "quadreg r undefined");
    check_model_compiles(r, "quadreg");

    // Cubic, exact: y = 0.5x^3 - x + 2 (year-scale x for conditioning).
    const double xc[] = {-3, -2, -1, 0, 1, 2, 3};
    double yc[7];
    for (int i = 0; i < 7; ++i) {
        yc[i] = 0.5 * xc[i] * xc[i] * xc[i] - xc[i] + 2;
    }
    r = fit(xc, yc, 7, RegressionType::kCubic);
    check_near(r.coeffs[0], 0.5, "cubicreg a", 1e-8);
    check_near(r.coeffs[1], 0.0, "cubicreg b", 1e-8);
    check_near(r.coeffs[2], -1.0, "cubicreg c", 1e-8);
    check_near(r.coeffs[3], 2.0, "cubicreg d", 1e-8);
    check_model_compiles(r, "cubicreg");

    // Quartic, exact: y = x^4 - 3x^2 + 1.
    double yq[7];
    for (int i = 0; i < 7; ++i) {
        yq[i] = std::pow(xc[i], 4) - 3 * xc[i] * xc[i] + 1;
    }
    r = fit(xc, yq, 7, RegressionType::kQuartic);
    check(r.coeff_count == 5, "quartreg coeff count");
    check_near(r.coeffs[0], 1.0, "quartreg a", 1e-7);
    check_near(r.coeffs[1], 0.0, "quartreg b", 1e-7);
    check_near(r.coeffs[2], -3.0, "quartreg c", 1e-6);
    check_near(r.coeffs[3], 0.0, "quartreg d", 1e-7);
    check_near(r.coeffs[4], 1.0, "quartreg e", 1e-6);
    check_model_compiles(r, "quartreg");

    // Not enough points for the degree.
    const double x3[] = {1, 2, 3};
    const double y3[] = {1, 2, 3};
    check(std::strcmp(fit(x3, y3, 3, RegressionType::kQuartic).error, "Not enough data") == 0,
          "quartreg needs 5 points");
}

void test_linearized_reg() {
    // LnReg exact: y = 2 + 3 ln x.
    const double x[] = {1, 2, 4, 8, 16};
    double y[5];
    for (int i = 0; i < 5; ++i) {
        y[i] = 2 + 3 * std::log(x[i]);
    }
    auto r = fit(x, y, 5, RegressionType::kLogarithmic);
    check(r.ok, "lnreg ok");
    check_near(r.coeffs[0], 2.0, "lnreg a", 1e-9);
    check_near(r.coeffs[1], 3.0, "lnreg b", 1e-9);
    check_near(r.r, 1.0, "lnreg r", 1e-12);
    check_model_compiles(r, "lnreg");

    // ExpReg exact: y = 3 * 1.5^x.
    const double xe[] = {0, 1, 2, 3};
    double ye[4];
    for (int i = 0; i < 4; ++i) {
        ye[i] = 3 * std::pow(1.5, xe[i]);
    }
    r = fit(xe, ye, 4, RegressionType::kExponential);
    check_near(r.coeffs[0], 3.0, "expreg a", 1e-9);
    check_near(r.coeffs[1], 1.5, "expreg b", 1e-9);
    check_near(r.r_squared, 1.0, "expreg r^2", 1e-12);
    check_model_compiles(r, "expreg");

    // PwrReg exact: y = 2 x^1.5.
    const double xp[] = {1, 4, 9, 16};
    double yp[4];
    for (int i = 0; i < 4; ++i) {
        yp[i] = 2 * std::pow(xp[i], 1.5);
    }
    r = fit(xp, yp, 4, RegressionType::kPower);
    check_near(r.coeffs[0], 2.0, "pwrreg a", 1e-9);
    check_near(r.coeffs[1], 1.5, "pwrreg b", 1e-9);
    check_model_compiles(r, "pwrreg");

    // Domain errors.
    const double xneg[] = {-1, 2, 3};
    const double yok[] = {1, 2, 3};
    check(std::strcmp(fit(xneg, yok, 3, RegressionType::kLogarithmic).error, "Domain error") == 0,
          "lnreg domain");
    const double xok[] = {1, 2, 3};
    const double yneg[] = {1, -2, 3};
    check(std::strcmp(fit(xok, yneg, 3, RegressionType::kExponential).error, "Domain error") == 0,
          "expreg domain");
}

void test_logistic_reg() {
    // Exact logistic: y = 10 / (1 + 9 e^-x).
    double x[13];
    double y[13];
    for (int i = 0; i < 13; ++i) {
        x[i] = i - 4;
        y[i] = 10.0 / (1 + 9 * std::exp(-x[i]));
    }
    const auto r = fit(x, y, 13, RegressionType::kLogistic);
    check(r.ok, "logistic ok");
    check(r.coeff_count == 3, "logistic coeff count");
    check_near(r.coeffs[0], 9.0, "logistic a", 5e-3);
    check_near(r.coeffs[1], 1.0, "logistic b", 5e-4);
    check_near(r.coeffs[2], 10.0, "logistic c", 5e-3);
    check(r.r_squared > 0.99999, "logistic r^2");
    check(std::isnan(r.r), "logistic r undefined");
    check_model_compiles(r, "logistic");

    // Domain: needs positive y.
    const double xb[] = {1, 2, 3};
    const double yb[] = {1, -1, 2};
    check(std::strcmp(fit(xb, yb, 3, RegressionType::kLogistic).error, "Domain error") == 0,
          "logistic domain");
}

void test_sinusoid_reg() {
    // Exact: y = 2 sin(1.5x + 0.5) + 3 over ~3 periods.
    double x[26];
    double y[26];
    for (int i = 0; i < 26; ++i) {
        x[i] = 0.5 * i;
        y[i] = 2 * std::sin(1.5 * x[i] + 0.5) + 3;
    }
    const auto r = fit(x, y, 26, RegressionType::kSinusoidal);
    check(r.ok, "sinreg ok");
    check(r.coeff_count == 4, "sinreg coeff count");
    check_near(r.coeffs[0], 2.0, "sinreg a", 1e-3);
    check_near(r.coeffs[1], 1.5, "sinreg b", 1e-3);
    check_near(r.coeffs[2], 0.5, "sinreg c", 1e-3);
    check_near(r.coeffs[3], 3.0, "sinreg d", 1e-3);
    check(r.converged, "sinreg converged");
    check_model_compiles(r, "sinreg");

    // Constant y can't fit.
    const double xc[] = {1, 2, 3, 4};
    const double yc[] = {5, 5, 5, 5};
    check(!fit(xc, yc, 4, RegressionType::kSinusoidal).ok, "sinreg constant errors");
}

void test_medmed_reg() {
    const double x[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    const double y[] = {2, 3, 5, 6, 7, 9, 10, 12, 13};
    const auto r = fit(x, y, 9, RegressionType::kMedianMedian);
    check(r.ok, "medmed ok");
    check_near(r.coeffs[0], 1.5, "medmed slope");
    check_near(r.coeffs[1], -1.0 / 6.0, "medmed intercept");
    check_model_compiles(r, "medmed");

    // Outlier resistance: one wild y barely moves the line.
    double yo[9];
    std::memcpy(yo, y, sizeof(yo));
    yo[4] = 500;
    const auto ro = fit(x, yo, 9, RegressionType::kMedianMedian);
    check_near(ro.coeffs[0], 1.5, "medmed outlier slope");
}

void test_format_model() {
    stats::RegressionResult r;
    r.ok = true;
    r.type = RegressionType::kLinear;
    r.coeffs[0] = 2;
    r.coeffs[1] = -3;
    r.coeff_count = 2;
    char buf[128];
    stats::format_model(r, false, buf, sizeof(buf));
    check(std::strcmp(buf, "2*x-3") == 0, "format linear");

    r.type = RegressionType::kLogarithmic;
    r.coeffs[0] = -1.5;
    r.coeffs[1] = 2;
    stats::format_model(r, false, buf, sizeof(buf));
    check(std::strcmp(buf, "-1.5+2*ln(x)") == 0, "format ln");

    // Degree conversion applies only to the sinusoidal trig arguments.
    r.type = RegressionType::kSinusoidal;
    r.coeffs[0] = 1;
    r.coeffs[1] = 3.14159265358979323846 / 180.0;  // 1 degree per x unit
    r.coeffs[2] = 0;
    r.coeffs[3] = 0;
    r.coeff_count = 4;
    stats::format_model(r, true, buf, sizeof(buf));
    check(std::strstr(buf, "sin(1*x") != nullptr, "format sin degree b");

    check(std::strcmp(stats::regression_name(RegressionType::kMedianMedian), "Med-Med") == 0,
          "regression_name");
    check(std::strcmp(stats::coeff_name(RegressionType::kLinear, 1), "b") == 0, "coeff_name");
}

}  // namespace

int main() {
    test_one_var();
    test_one_var_weighted();
    test_one_var_psram();
    test_two_var();
    test_linear_reg();
    test_poly_reg();
    test_linearized_reg();
    test_logistic_reg();
    test_sinusoid_reg();
    test_medmed_reg();
    test_format_model();

    std::printf("test_stats: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
