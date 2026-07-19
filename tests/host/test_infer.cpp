// Host-side tests for math::stats inference (sub-phase 3D, D27).
// Reference values from gen_infer_vectors.py (mpmath, 50-digit);
// datasets match that script.

#include <cmath>
#include <cstdio>

#include "math/array.hpp"
#include "math/infer.hpp"

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

void check_near(double got, double expected, const char* what, double tol) {
    ++g_checks;
    if (std::isnan(got) || std::fabs(got - expected) > tol) {
        std::printf("FAIL: %s -> %.17g (expected %.17g)\n", what, got, expected);
        ++g_failures;
    }
}

void fill(math::Array& a, const double* v, int n) {
    check(a.resize(n), "array resize");
    a.write_range(0, n, v);
}

constexpr double kD1[] = {12.9, 13.5, 12.8, 15.6, 17.2, 19.2, 12.6, 15.3, 14.4, 11.3};
constexpr double kD2[] = {12.7, 13.6, 12.0, 15.2, 16.8, 20.0, 12.0, 15.9, 16.0, 11.1};
constexpr double kLX[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
constexpr double kLY[] = {2.1, 4.3, 5.9, 8.4, 9.8, 12.5, 13.7, 16.1, 18.2, 19.8};
constexpr double kObs[] = {16, 18, 16, 14, 12, 12};
constexpr double kExp[] = {16, 16, 16, 16, 16, 16};
constexpr double kTC1[] = {10, 30};
constexpr double kTC2[] = {20, 40};
constexpr double kG1[] = {1, 2, 3, 4};
constexpr double kG2[] = {2, 3, 4, 5};
constexpr double kG3[] = {4, 5, 6, 7};

using namespace math::stats;

void test_z_tests() {
    {
        const auto r = z_test_1samp(105, 100, 15, 30, Alt::kNotEqual);
        check(r.ok, "z 1-samp ok");
        check_near(r.statistic, 1.8257418583505537, "z1 statistic", 1e-10);
        check_near(r.p_value, 0.067889154861829024, "z1 p (!=)", 1e-10);
        check(std::isnan(r.df), "z1 df is NaN");
    }
    check_near(z_test_1samp(105, 100, 15, 30, Alt::kGreater).p_value, 0.033944577430914512,
               "z1 p (>)", 1e-10);
    check_near(z_test_1samp(105, 100, 15, 30, Alt::kLess).p_value, 0.96605542256908549, "z1 p (<)",
               1e-10);
    {
        const auto r = z_test_2samp(20, 3, 40, 18.5, 4, 50, Alt::kNotEqual);
        check(r.ok, "z 2-samp ok");
        check_near(r.statistic, 2.0318563844357891, "z2 statistic", 1e-10);
        check_near(r.p_value, 0.042168197097155992, "z2 p", 1e-10);
        check_near(r.estimate, 1.5, "z2 estimate", 1e-12);
    }
    check(!z_test_1samp(0, 0, -1, 10, Alt::kNotEqual).ok, "z sigma<0 errors");
    check(!z_test_1samp(0, 0, 1, 0, Alt::kNotEqual).ok, "z n=0 errors");
}

void test_t_tests() {
    math::Array d1;
    math::Array d2;
    fill(d1, kD1, 10);
    fill(d2, kD2, 10);
    {
        const auto r = t_test_1samp(d1, 14, Alt::kNotEqual);
        check(r.ok, "t 1-samp ok");
        check_near(r.estimate, 14.48, "t1 mean", 1e-10);
        check_near(r.statistic, 0.63431815020410756, "t1 statistic", 1e-10);
        check_near(r.p_value, 0.5416586377642516, "t1 p", 1e-10);
        check_near(r.df, 9, "t1 df", 1e-12);
    }
    {
        const auto r = t_test_2samp(d1, d2, false, Alt::kNotEqual);
        check(r.ok, "t 2-samp welch ok");
        check_near(r.statistic, -0.043323479164802966, "t2w statistic", 1e-10);
        check_near(r.df, 17.653082778438187, "t2w Welch df", 1e-9);
        check_near(r.p_value, 0.9659299532017771, "t2w p", 1e-10);
    }
    {
        const auto r = t_test_2samp(d1, d2, true, Alt::kNotEqual);
        check(r.ok, "t 2-samp pooled ok");
        check_near(r.statistic, -0.043323479164802966, "t2p statistic", 1e-10);
        check_near(r.df, 18, "t2p df", 1e-12);
        check_near(r.p_value, 0.96592065565249464, "t2p p", 1e-10);
    }
    {
        const auto r = t_test_paired(d1, d2, Alt::kNotEqual);
        check(r.ok, "paired t ok");
        check_near(r.statistic, -0.21330847512739127, "tpair statistic", 1e-10);
        check_near(r.p_value, 0.83584000571392705, "tpair p", 1e-10);
        check_near(r.df, 9, "tpair df", 1e-12);
    }
    check(!t_test_1samp_summary(1, 0, 10, 0, Alt::kNotEqual).ok, "t s=0 errors");
    math::Array short_list;
    fill(short_list, kD1, 3);
    check(!t_test_paired(d1, short_list, Alt::kNotEqual).ok, "paired length mismatch errors");
}

void test_prop_tests() {
    {
        const auto r = prop_test_1samp(57, 100, 0.5, Alt::kGreater);
        check(r.ok, "1-prop ok");
        check_near(r.statistic, 1.4, "prop1 statistic", 1e-12);
        check_near(r.p_value, 0.080756659233771046, "prop1 p", 1e-10);
        check_near(r.estimate, 0.57, "prop1 p-hat", 1e-12);
    }
    {
        const auto r = prop_test_2samp(38, 100, 23, 90, Alt::kNotEqual);
        check(r.ok, "2-prop ok");
        check_near(r.statistic, 1.8344834402808342, "prop2 statistic", 1e-10);
        check_near(r.p_value, 0.066582263716817076, "prop2 p", 1e-10);
    }
    check(!prop_test_1samp(5, 4, 0.5, Alt::kNotEqual).ok, "x>n errors");
    check(!prop_test_1samp(2, 4, 0, Alt::kNotEqual).ok, "p0=0 errors");
}

void test_chisq_anova_linreg() {
    math::Array obs;
    math::Array exp;
    fill(obs, kObs, 6);
    fill(exp, kExp, 6);
    {
        const auto r = chisq_gof(obs, exp);
        check(r.ok, "GOF ok");
        check_near(r.statistic, 2.5, "gof chi2", 1e-10);
        check_near(r.df, 5, "gof df", 1e-12);
        check_near(r.p_value, 0.77649507112332271, "gof p", 1e-10);
    }
    math::Array c1;
    math::Array c2;
    fill(c1, kTC1, 2);
    fill(c2, kTC2, 2);
    {
        const math::Array* cols[] = {&c1, &c2};
        const auto r = chisq_test_2way(cols, 2);
        check(r.ok, "2-way ok");
        check_near(r.statistic, 0.79365079365079365, "2way chi2", 1e-10);
        check_near(r.df, 1, "2way df", 1e-12);
        check_near(r.p_value, 0.37299848361348712, "2way p", 1e-10);
    }
    math::Array g1;
    math::Array g2;
    math::Array g3;
    fill(g1, kG1, 4);
    fill(g2, kG2, 4);
    fill(g3, kG3, 4);
    {
        const math::Array* groups[] = {&g1, &g2, &g3};
        const auto r = anova_oneway(groups, 3);
        check(r.ok, "anova ok");
        check_near(r.statistic, 5.6, "anova F", 1e-10);
        check_near(r.df, 2, "anova df1", 1e-12);
        check_near(r.df2, 9, "anova df2", 1e-12);
        check_near(r.p_value, 0.026303293439047273, "anova p", 1e-10);
    }
    math::Array lx;
    math::Array ly;
    fill(lx, kLX, 10);
    fill(ly, kLY, 10);
    {
        const auto r = linreg_test(lx, ly, Alt::kNotEqual);
        check(r.ok, "linreg ok");
        check_near(r.estimate, 1.976969696969697, "linreg slope", 1e-10);
        check_near(r.statistic, 63.874889696517921, "linreg t", 1e-8);
        check_near(r.p_value, 4.0134149296625275e-12, "linreg p", 1e-10);
        check_near(r.df, 8, "linreg df", 1e-12);
    }
    // Degenerate inputs.
    math::Array flat;
    const double kFlat[] = {3, 3, 3, 3};
    fill(flat, kFlat, 4);
    {
        const math::Array* groups[] = {&flat, &flat};
        check(!anova_oneway(groups, 2).ok, "anova zero variance errors");
    }
    math::Array ly4;
    fill(ly4, kLY, 4);
    check(!linreg_test(flat, ly4, Alt::kNotEqual).ok, "linreg constant x errors");
}

void test_intervals() {
    math::Array d1;
    math::Array d2;
    fill(d1, kD1, 10);
    fill(d2, kD2, 10);
    {
        const auto r = ci_mean_z(105, 15, 30, 0.95);
        check(r.ok, "ci_mean_z ok");
        check_near(r.margin_of_error, 5.3675824311514718, "ci_mean_z moe", 1e-8);
        check_near(r.point_estimate, 105, "ci_mean_z center", 1e-12);
    }
    {
        const auto r = ci_mean_t(d1, 0.95);
        check(r.ok, "ci_mean_t ok");
        check_near(r.low, 12.768184820513579, "ci_mean_t low", 1e-8);
        check_near(r.high, 16.191815179486421, "ci_mean_t high", 1e-8);
    }
    {
        const auto r = ci_diff_means(d1, d2, 0.9, false);
        check(r.ok, "ci_diff_means ok");
        check_near(r.margin_of_error, 2.0034305954142649, "ci_diff_means moe", 1e-8);
    }
    {
        const auto r = ci_proportion(57, 100, 0.95);
        check(r.ok, "ci_proportion ok");
        check_near(r.margin_of_error, 0.097033064310683823, "ci_prop moe", 1e-8);
    }
    {
        const auto r = ci_diff_proportions(38, 100, 23, 90, 0.95);
        check(r.ok, "ci_diff_prop ok");
        check_near(r.margin_of_error, 0.1310372556278326, "ci_diff_prop moe", 1e-8);
    }
    check(!ci_mean_z(0, 1, 10, 1.0).ok, "conf=1 errors");
    check(!ci_mean_z(0, 1, 10, 0).ok, "conf=0 errors");
    check(!ci_mean_t_summary(0, 1, 1, 0.9).ok, "n=1 errors");
}

}  // namespace

int main() {
    test_z_tests();
    test_t_tests();
    test_prop_tests();
    test_chisq_anova_linreg();
    test_intervals();

    std::printf("test_infer: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
