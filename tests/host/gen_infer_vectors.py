#!/usr/bin/env python3
"""Reference values for tests/host/test_infer.cpp (sub-phase 3D, D27).

Run with the project venv (see requirements-dev.txt):
  .venv/bin/python tests/host/gen_infer_vectors.py

Formulas mirror src/math/infer.cpp (TI conventions: sample sd, pooled /
Welch two-sample t, score-test 1-prop z, pooled 2-prop z, Wald
proportion intervals); p-values at 50-digit precision via mpmath.
"""

from mpmath import mp, mpf, sqrt, erfc, betainc, gammainc, findroot

mp.dps = 50

D1 = [mpf(v) for v in "12.9 13.5 12.8 15.6 17.2 19.2 12.6 15.3 14.4 11.3".split()]
D2 = [mpf(v) for v in "12.7 13.6 12.0 15.2 16.8 20.0 12.0 15.9 16.0 11.1".split()]
LX = [mpf(v) for v in range(1, 11)]
LY = [mpf(v) for v in "2.1 4.3 5.9 8.4 9.8 12.5 13.7 16.1 18.2 19.8".split()]
OBS = [mpf(v) for v in "16 18 16 14 12 12".split()]
EXP = [mpf(16)] * 6
TCOL1 = [mpf(10), mpf(30)]
TCOL2 = [mpf(20), mpf(40)]
G1 = [mpf(v) for v in "1 2 3 4".split()]
G2 = [mpf(v) for v in "2 3 4 5".split()]
G3 = [mpf(v) for v in "4 5 6 7".split()]


def mean(xs):
    return sum(xs) / len(xs)


def sd(xs):
    m = mean(xs)
    return sqrt(sum((x - m) ** 2 for x in xs) / (len(xs) - 1))


def phi(z):
    return 0.5 * erfc(-z / sqrt(2))


def t_cdf(t, df):
    df = mpf(df)
    if t == 0:
        return mpf(0.5)
    p = 0.5 * betainc(df / 2, mpf(0.5), 0, df / (df + t * t), regularized=True)
    return 1 - p if t > 0 else p


def chisq_sf(x, df):
    return 1 - gammainc(mpf(df) / 2, 0, mpf(x) / 2, regularized=True)


def f_sf(x, d1, d2):
    x, d1, d2 = mpf(x), mpf(d1), mpf(d2)
    return 1 - betainc(d1 / 2, d2 / 2, 0, d1 * x / (d1 * x + d2), regularized=True)


def z_inv(area):
    return findroot(lambda z: phi(z) - area, 2)


def t_inv(area, df):
    return findroot(lambda t: t_cdf(t, df) - area, 2)


def emit(name, value, tol="1e-10"):
    print(f'    check_near({name}, {mp.nstr(mpf(value), 17)}, "{name}", {tol});')


print("// --- z 1-samp: z_test_1samp(105, 100, 15, 30, alt) ---")
z = (mpf(105) - 100) / (15 / sqrt(30))
emit("r.statistic", z)
emit("r.p_value (!=)", 2 * (1 - phi(abs(z))))
emit("p (>)", 1 - phi(z))
emit("p (<)", phi(z))

print("// --- z 2-samp: z_test_2samp(20, 3, 40, 18.5, 4, 50, kNotEqual) ---")
se = sqrt(mpf(9) / 40 + mpf(16) / 50)
z = mpf("1.5") / se
emit("r.statistic", z)
emit("r.p_value", 2 * (1 - phi(abs(z))))

print("// --- t 1-samp on D1, mu0 = 14 (kNotEqual) ---")
m, s, n = mean(D1), sd(D1), len(D1)
t = (m - 14) / (s / sqrt(n))
emit("r.estimate (mean)", m)
emit("r.statistic", t)
emit("r.p_value", 2 * (1 - t_cdf(abs(t), n - 1)))

print("// --- t 2-samp D1 vs D2 (Welch, kNotEqual) ---")
m2, s2, n2 = mean(D2), sd(D2), len(D2)
v1, v2 = s * s / n, s2 * s2 / n2
se = sqrt(v1 + v2)
dfw = (v1 + v2) ** 2 / (v1 * v1 / (n - 1) + v2 * v2 / (n2 - 1))
t2 = (m - m2) / se
emit("r.statistic", t2)
emit("r.df (Welch)", dfw)
emit("r.p_value", 2 * (1 - t_cdf(abs(t2), dfw)))

print("// --- t 2-samp pooled ---")
sp2 = ((n - 1) * s * s + (n2 - 1) * s2 * s2) / (n + n2 - 2)
sep = sqrt(sp2 * (mpf(1) / n + mpf(1) / n2))
tp = (m - m2) / sep
emit("r.statistic", tp)
emit("r.p_value", 2 * (1 - t_cdf(abs(tp), n + n2 - 2)))

print("// --- paired t: D1 vs D2 (kNotEqual) ---")
diffs = [a - b for a, b in zip(D1, D2)]
md, sdd = mean(diffs), sd(diffs)
tpair = md / (sdd / sqrt(len(diffs)))
emit("r.statistic", tpair)
emit("r.p_value", 2 * (1 - t_cdf(abs(tpair), len(diffs) - 1)))

print("// --- 1-prop z: prop_test_1samp(57, 100, 0.5, kGreater) ---")
p_hat = mpf(57) / 100
zp = (p_hat - mpf("0.5")) / sqrt(mpf("0.25") / 100)
emit("r.statistic", zp)
emit("r.p_value", 1 - phi(zp))

print("// --- 2-prop z: prop_test_2samp(38, 100, 23, 90, kNotEqual) ---")
p1, p2 = mpf(38) / 100, mpf(23) / 90
pp = mpf(38 + 23) / 190
sep2 = sqrt(pp * (1 - pp) * (mpf(1) / 100 + mpf(1) / 90))
zp2 = (p1 - p2) / sep2
emit("r.statistic", zp2)
emit("r.p_value", 2 * (1 - phi(abs(zp2))))

print("// --- chisq GOF: OBS vs EXP ---")
chi2 = sum((o - e) ** 2 / e for o, e in zip(OBS, EXP))
emit("r.statistic", chi2)
emit("r.p_value", chisq_sf(chi2, len(OBS) - 1))

print("// --- chisq 2-way: cols [10,30] and [20,40] ---")
total = sum(TCOL1) + sum(TCOL2)
c1, c2 = sum(TCOL1), sum(TCOL2)
chi2b = mpf(0)
for i in range(2):
    rs = TCOL1[i] + TCOL2[i]
    for cs, val in ((c1, TCOL1[i]), (c2, TCOL2[i])):
        e = rs * cs / total
        chi2b += (val - e) ** 2 / e
emit("r.statistic", chi2b)
emit("r.p_value", chisq_sf(chi2b, 1))

print("// --- ANOVA G1/G2/G3 ---")
groups = [G1, G2, G3]
N = sum(len(g) for g in groups)
gm = sum(sum(g) for g in groups) / N
ssb = sum(len(g) * (mean(g) - gm) ** 2 for g in groups)
ssw = sum((len(g) - 1) * sd(g) ** 2 for g in groups)
f = (ssb / 2) / (ssw / (N - 3))
emit("r.statistic", f)
emit("r.p_value", f_sf(f, 2, N - 3))

print("// --- linreg t-test LX/LY (kNotEqual) ---")
mx, my = mean(LX), mean(LY)
sxx = sum((x - mx) ** 2 for x in LX)
syy = sum((y - my) ** 2 for y in LY)
sxy = sum((x - mx) * (y - my) for x, y in zip(LX, LY))
b = sxy / sxx
sse = syy - b * sxy
seb = sqrt((sse / (len(LX) - 2)) / sxx)
tl = b / seb
emit("r.estimate (slope)", b)
emit("r.statistic", tl)
emit("r.p_value", 2 * (1 - t_cdf(abs(tl), len(LX) - 2)))

print("// --- intervals ---")
zs = z_inv((1 + mpf("0.95")) / 2)
emit("ci_mean_z moe", zs * 15 / sqrt(30), "1e-8")
ts = t_inv((1 + mpf("0.95")) / 2, n - 1)
emit("ci_mean_t(D1) low", m - ts * s / sqrt(n), "1e-8")
emit("ci_mean_t(D1) high", m + ts * s / sqrt(n), "1e-8")
tw = t_inv((1 + mpf("0.90")) / 2, dfw)
emit("ci_diff_means(D1,D2,0.9,welch) moe", tw * se, "1e-8")
emit("ci_proportion(57,100,0.95) moe", zs * sqrt(p_hat * (1 - p_hat) / 100), "1e-8")
sedp = sqrt(p1 * (1 - p1) / 100 + p2 * (1 - p2) / 90)
emit("ci_diff_proportions moe", zs * sedp, "1e-8")
