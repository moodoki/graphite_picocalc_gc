#include "math/catalog.hpp"

#include "math/dist.hpp"
#include "math/functions.hpp"

namespace math {

namespace {

const void* fp1(double (*f)(double)) {
    return reinterpret_cast<const void*>(f);
}
const void* fp2(double (*f)(double, double)) {
    return reinterpret_cast<const void*>(f);
}
const void* fp0(double (*f)()) {
    return reinterpret_cast<const void*>(f);
}
const void* fp3(double (*f)(double, double, double)) {
    return reinterpret_cast<const void*>(f);
}
const void* fp4(double (*f)(double, double, double, double)) {
    return reinterpret_cast<const void*>(f);
}

// Display order: trig, logs, combinatorics, misc. Summaries must fit
// the help screen's ~26-char column.
const FnDescriptor kCatalog[] = {
    {"sin", "sin(x)", "Sine (angle mode)", fp1(fn::sin_am), 1},
    {"cos", "cos(x)", "Cosine (angle mode)", fp1(fn::cos_am), 1},
    {"tan", "tan(x)", "Tangent (angle mode)", fp1(fn::tan_am), 1},
    {"asin", "asin(x)", "Inverse sine", fp1(fn::asin_am), 1},
    {"acos", "acos(x)", "Inverse cosine", fp1(fn::acos_am), 1},
    {"atan", "atan(x)", "Inverse tangent", fp1(fn::atan_am), 1},
    {"log", "log(x)", "Log base 10", fp1(fn::log10_ti), 1},
    {"ln", "ln(x)", "Natural log", fp1(fn::ln_nat), 1},
    {"fac", "fac(n)", "Factorial (also n!)", fp1(fn::factorial), 1},
    {"ncr", "ncr(n, r)", "n choose r", fp2(fn::ncr), 2},
    {"npr", "npr(n, r)", "Permutations", fp2(fn::npr), 2},
    {"rand", "rand()", "Random in [0, 1)", fp0(fn::rand01), 0},
    {"round", "round(x, n)", "Round to n decimals", fp2(fn::round_n), 2},
    {"min", "min(a, b)", "Smaller of a and b", fp2(fn::min2), 2},
    {"max", "max(a, b)", "Larger of a and b", fp2(fn::max2), 2},
    {"deg", "deg(x)", "Radians to degrees", fp1(fn::deg), 1},
    {"rad", "rad(x)", "Degrees to radians", fp1(fn::rad), 1},
    // List functions (Phase 3A): help-only rows (fn == nullptr) — they
    // take list arguments, which tinyexpr can't, so math::listexpr
    // intercepts them before the engine sees the expression.
    {"sum", "sum(l)", "Sum of list elements", nullptr, 1},
    {"prod", "prod(l)", "Product of elements", nullptr, 1},
    {"length", "length(l)", "List element count", nullptr, 1},
    {"sort_asc", "sort_asc(l)", "Sort list ascending", nullptr, 1},
    {"sort_desc", "sort_desc(l)", "Sort list descending", nullptr, 1},
    {"cumsum", "cumsum(l)", "Cumulative sums", nullptr, 1},
    {"delta_list", "delta_list(l)", "Pairwise differences", nullptr, 1},
    {"seq", "seq(f,v,lo,hi,st)", "Sequence into a list", nullptr, 5},
    // D24 additions: list generator + scalar reductions.
    {"range", "range(lo,hi,st?)", "List lo..hi, step 1", nullptr, 3},
    {"mean", "mean(l)", "Mean of elements", nullptr, 1},
    {"median", "median(l)", "Median of elements", nullptr, 1},
    {"stdev", "stdev(l)", "Sample stddev (Sx)", nullptr, 1},
    // Distributions (Phase 3C, D25). Full arity — tinyexpr has no
    // default args (spec §5.3). Continuous cdf = P(lo<=X<=hi),
    // inv = lower-tail inverse; long signatures push the help summary
    // column right, so keep these summaries short.
    {"normal_pdf", "normal_pdf(x,mu,sd)", "Normal density", fp3(dist::normal_pdf), 3},
    {"normal_cdf", "normal_cdf(lo,hi,mu,sd)", "P(lo<=X<=hi)", fp4(dist::normal_cdf), 4},
    {"normal_inv", "normal_inv(area,mu,sd)", "Inverse CDF", fp3(dist::normal_inv), 3},
    {"t_pdf", "t_pdf(x,df)", "Student t density", fp2(dist::t_pdf), 2},
    {"t_cdf", "t_cdf(lo,hi,df)", "t P(lo<=X<=hi)", fp3(dist::t_cdf), 3},
    {"t_inv", "t_inv(area,df)", "t inverse CDF", fp2(dist::t_inv), 2},
    {"chisq_pdf", "chisq_pdf(x,df)", "Chi-sq density", fp2(dist::chisq_pdf), 2},
    {"chisq_cdf", "chisq_cdf(lo,hi,df)", "P(lo<=X<=hi)", fp3(dist::chisq_cdf), 3},
    {"chisq_inv", "chisq_inv(area,df)", "Chi-sq inv CDF", fp2(dist::chisq_inv), 2},
    {"f_pdf", "f_pdf(x,d1,d2)", "F density", fp3(dist::f_pdf), 3},
    {"f_cdf", "f_cdf(lo,hi,d1,d2)", "F P(lo<=X<=hi)", fp4(dist::f_cdf), 4},
    {"f_inv", "f_inv(area,d1,d2)", "F inverse CDF", fp3(dist::f_inv), 3},
    {"binomial_pmf", "binomial_pmf(k,n,p)", "P(X=k)", fp3(dist::binomial_pmf), 3},
    {"binomial_cdf", "binomial_cdf(k,n,p)", "P(X<=k)", fp3(dist::binomial_cdf), 3},
    {"poisson_pmf", "poisson_pmf(k,lam)", "P(X=k)", fp2(dist::poisson_pmf), 2},
    {"poisson_cdf", "poisson_cdf(k,lam)", "P(X<=k)", fp2(dist::poisson_cdf), 2},
    {"geometric_pmf", "geometric_pmf(k,p)", "P(1st success=k)", fp2(dist::geometric_pmf), 2},
    {"geometric_cdf", "geometric_cdf(k,p)", "P(X<=k)", fp2(dist::geometric_cdf), 2},
    // Matrices (Phase 4A): help-only rows — [A]-[J] arguments are
    // intercepted by math::matexpr before the engine. Also [A]^-1,
    // [A]^T, [A](r,c), and `-> [C]` store (see help text).
    {"det", "det([A])", "Determinant", nullptr, 1},
    {"inverse", "inverse([A])", "Matrix inverse", nullptr, 1},
    {"transpose", "transpose([A])", "Transpose", nullptr, 1},
    {"rref", "rref([A])", "Reduced row echelon", nullptr, 1},
    {"ref", "ref([A])", "Row echelon form", nullptr, 1},
    {"rank", "rank([A])", "Matrix rank", nullptr, 1},
    {"identity", "identity(n)", "n x n identity", nullptr, 1},
    {"augment", "augment([A],[B])", "Concat columns", nullptr, 2},
    {"dim", "dim([A])", "{rows, cols} list", nullptr, 1},
    {"eigenvals", "eigenvals([A])", "Real eigenvalues", nullptr, 1},
    {"eig", "eig([A])", "Eigenvalues (alias)", nullptr, 1},
    // Numeric solver (Phase 4A): solve_expr intercepts; bare `solve`
    // opens the solver screen.
    {"solve", "solve(f,x,lo,hi)", "Root of f (or guess)", nullptr, 4},
};

constexpr int kCount = sizeof(kCatalog) / sizeof(kCatalog[0]);
static_assert(kCount <= kMaxCatalogEntries, "grow kMaxCatalogEntries");

}  // namespace

const FnDescriptor* catalog(int* count) {
    *count = kCount;
    return kCatalog;
}

}  // namespace math
