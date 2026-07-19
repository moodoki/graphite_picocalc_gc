#!/usr/bin/env python3
"""Reference values for tests/host/test_dist.cpp (sub-phase 3C).

Run with the project venv (see requirements-dev.txt):
  .venv/bin/python tests/host/gen_dist_vectors.py

Prints C check lines at 50-digit working precision (mpmath), rounded to
17 significant digits. Conventions match D25: continuous cdf(lo, hi) is
two-sided lower-upper difference; inv() inverts the lower tail;
geometric counts trials until first success.
"""

from mpmath import mp, mpf, erfc, sqrt, exp, betainc, gammainc, findroot, binomial, factorial

mp.dps = 50


def normal_cdf1(x, mu=0, sd=1):
    return 0.5 * erfc(-(mpf(x) - mu) / (sd * sqrt(2)))


def normal_pdf(x, mu=0, sd=1):
    z = (mpf(x) - mu) / sd
    return exp(-z * z / 2) / (sd * sqrt(2 * mp.pi))


def t_cdf1(t, df):
    t, df = mpf(t), mpf(df)
    if t == 0:
        return mpf(0.5)
    p = 0.5 * betainc(df / 2, mpf(0.5), 0, df / (df + t * t), regularized=True)
    return 1 - p if t > 0 else p


def t_pdf(x, df):
    x, df = mpf(x), mpf(df)
    from mpmath import gamma
    return gamma((df + 1) / 2) / (sqrt(df * mp.pi) * gamma(df / 2)) * (1 + x * x / df) ** (-(df + 1) / 2)


def chisq_cdf1(x, df):
    return gammainc(mpf(df) / 2, 0, mpf(x) / 2, regularized=True)


def chisq_pdf(x, df):
    from mpmath import gamma
    x, df = mpf(x), mpf(df)
    return x ** (df / 2 - 1) * exp(-x / 2) / (2 ** (df / 2) * gamma(df / 2))


def f_cdf1(x, d1, d2):
    x, d1, d2 = mpf(x), mpf(d1), mpf(d2)
    return betainc(d1 / 2, d2 / 2, 0, d1 * x / (d1 * x + d2), regularized=True)


def f_pdf(x, d1, d2):
    from mpmath import beta
    x, d1, d2 = mpf(x), mpf(d1), mpf(d2)
    a, b = d1 / 2, d2 / 2
    return (d1 / d2) ** a * x ** (a - 1) * (1 + d1 * x / d2) ** (-(a + b)) / beta(a, b)


def inv(f, area, x0):
    return findroot(lambda x: f(x) - area, x0)


def binom_pmf(k, n, p):
    p = mpf(p)
    return binomial(n, k) * p ** k * (1 - p) ** (n - k)


def binom_cdf(k, n, p):
    return sum(binom_pmf(i, n, p) for i in range(0, k + 1))


def poisson_pmf(k, lam):
    lam = mpf(lam)
    return exp(-lam) * lam ** k / factorial(k)


def poisson_cdf(k, lam):
    return sum(poisson_pmf(i, lam) for i in range(0, k + 1))


def geom_pmf(k, p):
    p = mpf(p)
    return (1 - p) ** (k - 1) * p


def geom_cdf(k, p):
    p = mpf(p)
    return 1 - (1 - p) ** k


def emit(expr, value, tol):
    print(f'    check_near({expr}, {mp.nstr(value, 17)}, "{expr}", {tol});')


print("// Normal")
emit("normal_pdf(0, 0, 1)", normal_pdf(0), "1e-15")
emit("normal_pdf(1.5, 1, 2)", normal_pdf(1.5, 1, 2), "1e-15")
emit("normal_cdf(-1e99, 0, 0, 1)", mpf(0.5), "1e-12")
emit("normal_cdf(-1, 1, 0, 1)", normal_cdf1(1) - normal_cdf1(-1), "1e-12")
emit("normal_cdf(-1e99, 1.96, 0, 1)", normal_cdf1(1.96), "1e-12")
emit("normal_cdf(85, 115, 100, 15)", normal_cdf1(115, 100, 15) - normal_cdf1(85, 100, 15), "1e-12")
emit("normal_inv(0.975, 0, 1)", inv(normal_cdf1, mpf("0.975"), 2), "1e-9")
emit("normal_inv(0.9, 100, 15)", inv(lambda x: normal_cdf1(x, 100, 15), mpf("0.9"), 119), "1e-7")

print("// Student's t")
emit("t_pdf(0, 10)", t_pdf(0, 10), "1e-15")
emit("t_pdf(-1.3, 3.5)", t_pdf(-1.3, mpf("3.5")), "1e-15")
emit("t_cdf(-1e99, 2.2281388519649385, 10)", t_cdf1(mpf("2.2281388519649385"), 10), "1e-12")
emit("t_cdf(-2, 2, 5)", t_cdf1(2, 5) - t_cdf1(-2, 5), "1e-12")
emit("t_inv(0.975, 10)", inv(lambda t: t_cdf1(t, 10), mpf("0.975"), 2), "1e-9")
emit("t_inv(0.05, 2.5)", inv(lambda t: t_cdf1(t, mpf("2.5")), mpf("0.05"), -2), "1e-9")

print("// Chi-square")
emit("chisq_pdf(3, 4)", chisq_pdf(3, 4), "1e-15")
emit("chisq_cdf(0, 3.841458820694124, 1)", chisq_cdf1(mpf("3.841458820694124"), 1), "1e-12")
emit("chisq_cdf(2, 7, 5.5)", chisq_cdf1(7, mpf("5.5")) - chisq_cdf1(2, mpf("5.5")), "1e-12")
emit("chisq_inv(0.95, 1)", inv(lambda x: chisq_cdf1(x, 1), mpf("0.95"), 4), "1e-8")
emit("chisq_inv(0.5, 10)", inv(lambda x: chisq_cdf1(x, 10), mpf("0.5"), 9), "1e-8")

print("// F")
emit("f_pdf(1, 5, 10)", f_pdf(1, 5, 10), "1e-15")
emit("f_cdf(0, 2.5, 3, 12)", f_cdf1(mpf("2.5"), 3, 12), "1e-12")
emit("f_inv(0.95, 3, 12)", inv(lambda x: f_cdf1(x, 3, 12), mpf("0.95"), 3.5), "1e-8")

print("// Binomial")
emit("binomial_pmf(3, 10, 0.5)", binom_pmf(3, 10, mpf("0.5")), "1e-15")
emit("binomial_pmf(0, 10, 0.3)", binom_pmf(0, 10, mpf("0.3")), "1e-15")
emit("binomial_cdf(3, 10, 0.5)", binom_cdf(3, 10, mpf("0.5")), "1e-12")
emit("binomial_cdf(5, 20, 0.25)", binom_cdf(5, 20, mpf("0.25")), "1e-12")

print("// Poisson")
emit("poisson_pmf(2, 3)", poisson_pmf(2, 3), "1e-15")
emit("poisson_pmf(0, 2.5)", poisson_pmf(0, mpf("2.5")), "1e-15")
emit("poisson_cdf(2, 3)", poisson_cdf(2, 3), "1e-12")
emit("poisson_cdf(10, 6.5)", poisson_cdf(10, mpf("6.5")), "1e-12")

print("// Geometric")
emit("geometric_pmf(3, 0.2)", geom_pmf(3, mpf("0.2")), "1e-15")
emit("geometric_cdf(3, 0.2)", geom_cdf(3, mpf("0.2")), "1e-15")
emit("geometric_cdf(8, 0.35)", geom_cdf(8, mpf("0.35")), "1e-15")
