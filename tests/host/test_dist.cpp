// Host-side tests for math::dist (sub-phase 3C, D25). Reference values
// generated at 50-digit precision by gen_dist_vectors.py (mpmath, see
// requirements-dev.txt); tolerances reflect the cephes double paths.

#include <cmath>
#include <cstdio>

#include "math/dist.hpp"

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

void check_nan(double got, const char* what) {
    ++g_checks;
    if (!std::isnan(got)) {
        std::printf("FAIL: %s -> %.17g (expected NaN)\n", what, got);
        ++g_failures;
    }
}

using namespace math::dist;

void test_reference_values() {
    // Normal
    check_near(normal_pdf(0, 0, 1), 0.39894228040143268, "normal_pdf(0, 0, 1)", 1e-15);
    check_near(normal_pdf(1.5, 1, 2), 0.1933340584014246, "normal_pdf(1.5, 1, 2)", 1e-15);
    check_near(normal_cdf(-1e99, 0, 0, 1), 0.5, "normal_cdf(-1e99, 0, 0, 1)", 1e-12);
    check_near(normal_cdf(-1, 1, 0, 1), 0.6826894921370859, "normal_cdf(-1, 1, 0, 1)", 1e-12);
    check_near(normal_cdf(-1e99, 1.96, 0, 1), 0.97500210485177956, "normal_cdf(-1e99, 1.96, 0, 1)",
               1e-12);
    check_near(normal_cdf(85, 115, 100, 15), 0.6826894921370859, "normal_cdf(85, 115, 100, 15)",
               1e-12);
    check_near(normal_inv(0.975, 0, 1), 1.9599639845400542, "normal_inv(0.975, 0, 1)", 1e-9);
    check_near(normal_inv(0.9, 100, 15), 119.22327348316901, "normal_inv(0.9, 100, 15)", 1e-7);

    // Student's t
    check_near(t_pdf(0, 10), 0.38910838396603105, "t_pdf(0, 10)", 1e-15);
    check_near(t_pdf(-1.3, 3.5), 0.15321582498019391, "t_pdf(-1.3, 3.5)", 1e-15);
    check_near(t_cdf(-1e99, 2.2281388519649385, 10), 0.97499999999909566,
               "t_cdf(-1e99, 2.2281388519649385, 10)", 1e-12);
    check_near(t_cdf(-2, 2, 5), 0.89806052117014164, "t_cdf(-2, 2, 5)", 1e-12);
    check_near(t_inv(0.975, 10), 2.2281388519862747, "t_inv(0.975, 10)", 1e-9);
    check_near(t_inv(0.05, 2.5), -2.5582186141359366, "t_inv(0.05, 2.5)", 1e-9);

    // Chi-square
    check_near(chisq_pdf(3, 4), 0.16734762011132237, "chisq_pdf(3, 4)", 1e-15);
    check_near(chisq_cdf(0, 3.841458820694124, 1), 0.94999999999999994,
               "chisq_cdf(0, 3.841458820694124, 1)", 1e-12);
    check_near(chisq_cdf(2, 7, 5.5), 0.61977064030222071, "chisq_cdf(2, 7, 5.5)", 1e-12);
    check_near(chisq_inv(0.95, 1), 3.841458820694126, "chisq_inv(0.95, 1)", 1e-8);
    check_near(chisq_inv(0.5, 10), 9.3418177655919674, "chisq_inv(0.5, 10)", 1e-8);

    // F
    check_near(f_pdf(1, 5, 10), 0.49547978348663871, "f_pdf(1, 5, 10)", 1e-15);
    check_near(f_cdf(0, 2.5, 3, 12), 0.89084528760499372, "f_cdf(0, 2.5, 3, 12)", 1e-12);
    check_near(f_inv(0.95, 3, 12), 3.4902948194976057, "f_inv(0.95, 3, 12)", 1e-8);

    // Binomial
    check_near(binomial_pmf(3, 10, 0.5), 0.1171875, "binomial_pmf(3, 10, 0.5)", 1e-15);
    check_near(binomial_pmf(0, 10, 0.3), 0.0282475249, "binomial_pmf(0, 10, 0.3)", 1e-15);
    check_near(binomial_cdf(3, 10, 0.5), 0.171875, "binomial_cdf(3, 10, 0.5)", 1e-12);
    check_near(binomial_cdf(5, 20, 0.25), 0.61717265438710456, "binomial_cdf(5, 20, 0.25)", 1e-12);

    // Poisson
    check_near(poisson_pmf(2, 3), 0.22404180765538774, "poisson_pmf(2, 3)", 1e-15);
    check_near(poisson_pmf(0, 2.5), 0.082084998623898795, "poisson_pmf(0, 2.5)", 1e-15);
    check_near(poisson_cdf(2, 3), 0.42319008112684352, "poisson_cdf(2, 3)", 1e-12);
    check_near(poisson_cdf(10, 6.5), 0.93316120975419641, "poisson_cdf(10, 6.5)", 1e-12);

    // Geometric
    check_near(geometric_pmf(3, 0.2), 0.128, "geometric_pmf(3, 0.2)", 1e-15);
    check_near(geometric_cdf(3, 0.2), 0.488, "geometric_cdf(3, 0.2)", 1e-15);
    check_near(geometric_cdf(8, 0.35), 0.9681355187109375, "geometric_cdf(8, 0.35)", 1e-15);
}

void test_identities_and_edges() {
    // Symmetries / complements.
    check_near(t_inv(0.025, 10), -t_inv(0.975, 10), "t_inv symmetry", 1e-12);
    check_near(normal_inv(0.5, 3, 2), 3.0, "normal_inv(0.5) = mean", 1e-12);
    check_near(t_cdf(0, 1e99, 7), 0.5, "t upper half", 1e-12);
    check_near(t_inv(0.5, 7), 0.0, "t_inv(0.5) = 0", 1e-15);
    // Round trips.
    check_near(chisq_cdf(0, chisq_inv(0.7, 6.5), 6.5), 0.7, "chisq inv round trip (real df)",
               1e-9);
    check_near(f_cdf(0, f_inv(0.25, 4, 9), 4, 9), 0.25, "f inv round trip", 1e-9);
    check_near(t_cdf(-1e99, t_inv(0.6, 3.3), 3.3), 0.6, "t inv round trip (real df)", 1e-9);
    // Discrete edges.
    check_near(binomial_cdf(3.7, 10, 0.5), binomial_cdf(3, 10, 0.5), "binomial_cdf floors k",
               1e-15);
    check_near(binomial_cdf(10, 10, 0.5), 1.0, "binomial_cdf(k=n) = 1", 1e-15);
    check_near(binomial_pmf(11, 10, 0.5), 0.0, "binomial_pmf k>n = 0", 1e-15);
    check_near(binomial_pmf(10, 10, 1), 1.0, "binomial_pmf p=1 k=n", 1e-15);
    check_near(poisson_cdf(-1, 3), 0.0, "poisson_cdf k<0 = 0", 1e-15);
    check_near(geometric_pmf(1, 1), 1.0, "geometric_pmf p=1 k=1", 1e-15);
    check_near(geometric_cdf(0.5, 0.3), 0.0, "geometric_cdf k<1 = 0", 1e-15);
    // CDF lower bound clamps below the support.
    check_near(chisq_cdf(-5, 3.841458820694124, 1), chisq_cdf(0, 3.841458820694124, 1),
               "chisq_cdf clamps lo<0", 1e-15);
    check_near(f_cdf(-1, 2.5, 3, 12), f_cdf(0, 2.5, 3, 12), "f_cdf clamps lo<0", 1e-15);
    // Infinite-ish tails.
    check_near(normal_cdf(-1e99, 1e99, 0, 1), 1.0, "normal full-range = 1", 1e-12);
    check(chisq_inv(0, 5) == 0.0, "chisq_inv(0) = 0");
    check(f_inv(0, 3, 5) == 0.0, "f_inv(0) = 0");
}

void test_domain_errors() {
    check_nan(normal_pdf(0, 0, -1), "normal_pdf sd<0");
    check_nan(normal_pdf(0, 0, 0), "normal_pdf sd=0");
    check_nan(normal_cdf(1, -1, 0, 1), "normal_cdf hi<lo");
    check_nan(normal_inv(0, 0, 1), "normal_inv area=0");
    check_nan(normal_inv(1, 0, 1), "normal_inv area=1");
    check_nan(normal_inv(1.5, 0, 1), "normal_inv area>1");
    check_nan(t_pdf(0, 0), "t_pdf df=0");
    check_nan(t_cdf(2, -2, 5), "t_cdf hi<lo");
    check_nan(t_inv(0.5, -1), "t_inv df<0");
    check_nan(chisq_inv(1, 5), "chisq_inv area=1");
    check_nan(f_cdf(0, 1, 0, 5), "f_cdf df1=0");
    check_nan(binomial_pmf(2.5, 10, 0.5), "binomial_pmf non-integer k");
    check_nan(binomial_pmf(2, 10.5, 0.5), "binomial_pmf non-integer n");
    check_nan(binomial_pmf(2, 10, 1.5), "binomial_pmf p>1");
    check_nan(binomial_cdf(2, 10, -0.1), "binomial_cdf p<0");
    check_nan(poisson_pmf(1.5, 3), "poisson_pmf non-integer k");
    check_nan(poisson_pmf(2, -1), "poisson_pmf lambda<0");
    check_nan(geometric_pmf(2, 0), "geometric_pmf p=0");
    check_nan(geometric_cdf(2, 1.1), "geometric_cdf p>1");
}

}  // namespace

int main() {
    test_reference_values();
    test_identities_and_edges();
    test_domain_errors();

    std::printf("test_dist: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
