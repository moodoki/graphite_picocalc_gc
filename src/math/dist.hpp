#pragma once

#include "math/types.hpp"

// Probability distributions (sub-phase 3C, spec §5; conventions D25).
// Thin wrappers over the vendored cephes primitives (drivers/cephes/)
// plus std::lgamma closed forms for the pdfs/pmfs.
//
// Conventions (D25):
//  - Continuous CDFs are TI-style two-sided: cdf(lo, hi, ...) =
//    P(lo <= X <= hi); hi < lo is a domain error. Use +/-1e99 (or any
//    far value) for an open tail.
//  - inv(area, ...) inverts the lower-tail area, area in (0, 1)
//    exclusive (chisq/F accept area = 0 -> 0).
//  - Discrete cdf(k, ...) = P(X <= floor(k)); pmf arguments k (and
//    binomial n) must be integers within 1e-9, matching TI's
//    integer-argument rule.
//  - All k/n parameters are calc_t (not int as in the spec sketch) so
//    every function binds directly to the parser's fixed-arity
//    double signature.
//  - Domain errors return NaN (project convention: no exceptions).
namespace math::dist {

// Normal (sd > 0)
calc_t normal_pdf(calc_t x, calc_t mean, calc_t sd);
calc_t normal_cdf(calc_t lo, calc_t hi, calc_t mean, calc_t sd);
calc_t normal_inv(calc_t area, calc_t mean, calc_t sd);

// Student's t (df > 0, real-valued)
calc_t t_pdf(calc_t x, calc_t df);
calc_t t_cdf(calc_t lo, calc_t hi, calc_t df);
calc_t t_inv(calc_t area, calc_t df);

// Chi-square (df > 0, real-valued)
calc_t chisq_pdf(calc_t x, calc_t df);
calc_t chisq_cdf(calc_t lo, calc_t hi, calc_t df);
calc_t chisq_inv(calc_t area, calc_t df);

// F (df1, df2 > 0, real-valued)
calc_t f_pdf(calc_t x, calc_t df1, calc_t df2);
calc_t f_cdf(calc_t lo, calc_t hi, calc_t df1, calc_t df2);
calc_t f_inv(calc_t area, calc_t df1, calc_t df2);

// Binomial (integer 0 <= k <= n, 0 <= p <= 1)
calc_t binomial_pmf(calc_t k, calc_t n, calc_t p);
calc_t binomial_cdf(calc_t k, calc_t n, calc_t p);

// Poisson (integer k >= 0, lambda > 0)
calc_t poisson_pmf(calc_t k, calc_t lambda);
calc_t poisson_cdf(calc_t k, calc_t lambda);

// Geometric: trials until first success (integer k >= 1, 0 < p <= 1)
calc_t geometric_pmf(calc_t k, calc_t p);
calc_t geometric_cdf(calc_t k, calc_t p);

// One-sided building blocks for inference (3D): lower-tail CDFs and
// survival functions (upper tails computed directly, so p-values keep
// full precision in the far tail instead of rounding through 1-cdf).
// No argument validation — inference callers check domains.
calc_t normal_cdf_1(calc_t z);  // Standard normal P(Z <= z)
calc_t normal_sf(calc_t z);     // P(Z > z)
calc_t t_cdf_1(calc_t t, calc_t df);
calc_t t_sf(calc_t t, calc_t df);
calc_t chisq_sf(calc_t x, calc_t df);  // P(X > x) via igamc
calc_t f_sf(calc_t x, calc_t df1, calc_t df2);

}  // namespace math::dist
