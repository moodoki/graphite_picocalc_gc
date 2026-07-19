#include "math/infer.hpp"

#include <cmath>
#include <limits>

#include "math/dist.hpp"

namespace math::stats {

namespace {

constexpr int kChunk = 256;
// Streaming buffers (single-core application code, no reentrancy).
calc_t g_buf_a[kChunk];
calc_t g_buf_b[kChunk];
calc_t g_row_sum[kChunk];  // chisq_test_2way per-chunk row sums

calc_t nan_v() {
    return std::numeric_limits<calc_t>::quiet_NaN();
}

TestResult test_error(const char* msg) {
    TestResult r;
    r.error = msg;
    return r;
}

Interval interval_error(const char* msg) {
    Interval r;
    r.error = msg;
    return r;
}

calc_t p_from_alt_z(calc_t z, Alt alt) {
    switch (alt) {
        case Alt::kLess:
            return dist::normal_cdf_1(z);
        case Alt::kGreater:
            return dist::normal_sf(z);
        default:
            return 2 * dist::normal_sf(std::fabs(z));
    }
}

calc_t p_from_alt_t(calc_t t, calc_t df, Alt alt) {
    switch (alt) {
        case Alt::kLess:
            return dist::t_cdf_1(t, df);
        case Alt::kGreater:
            return dist::t_sf(t, df);
        default:
            return 2 * dist::t_sf(std::fabs(t), df);
    }
}

bool conf_ok(calc_t conf) {
    return !std::isnan(conf) && conf > 0 && conf < 1;
}

// Mean + sample sd of a list, streaming, two-pass centered sums.
// Deliberately NOT stats::one_var — that also runs the (much more
// expensive) quartile rank selection the t machinery doesn't need.
bool mean_sd(const Array& a, calc_t* mean, calc_t* sd, int* n_out) {
    const int n = a.size();
    if (n < 2) {
        return false;
    }
    calc_t sum = 0;
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        a.read_range(at, m, g_buf_a);
        for (int i = 0; i < m; ++i) {
            sum += g_buf_a[i];
        }
    }
    const calc_t mu = sum / n;
    calc_t ss = 0;
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        a.read_range(at, m, g_buf_a);
        for (int i = 0; i < m; ++i) {
            const calc_t d = g_buf_a[i] - mu;
            ss += d * d;
        }
    }
    *mean = mu;
    *sd = std::sqrt(ss / (n - 1));
    *n_out = n;
    return true;
}

// Mean + sample sd of a streamed elementwise difference a[i] - b[i]
// (Welford over chunks would be overkill; two-pass centered sums).
bool diff_mean_sd(const Array& a, const Array& b, calc_t* mean, calc_t* sd, int* n_out) {
    const int n = a.size();
    if (n != b.size() || n < 2) {
        return false;
    }
    calc_t sum = 0;
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        a.read_range(at, m, g_buf_a);
        b.read_range(at, m, g_buf_b);
        for (int i = 0; i < m; ++i) {
            sum += g_buf_a[i] - g_buf_b[i];
        }
    }
    const calc_t mu = sum / n;
    calc_t ss = 0;
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        a.read_range(at, m, g_buf_a);
        b.read_range(at, m, g_buf_b);
        for (int i = 0; i < m; ++i) {
            const calc_t d = g_buf_a[i] - g_buf_b[i] - mu;
            ss += d * d;
        }
    }
    *mean = mu;
    *sd = std::sqrt(ss / (n - 1));
    *n_out = n;
    return true;
}

// Two-sample t machinery shared by the test and the interval: fills
// se and df (pooled or Welch) from summary stats.
bool two_samp_t_se_df(calc_t s1, int n1, calc_t s2, int n2, bool pooled, calc_t* se, calc_t* df) {
    if (n1 < 2 || n2 < 2 || s1 < 0 || s2 < 0) {
        return false;
    }
    const calc_t v1 = s1 * s1 / n1;
    const calc_t v2 = s2 * s2 / n2;
    if (pooled) {
        const calc_t sp2 =
            ((n1 - 1) * s1 * s1 + (n2 - 1) * s2 * s2) / static_cast<calc_t>(n1 + n2 - 2);
        *se = std::sqrt(sp2 * (1.0 / n1 + 1.0 / n2));
        *df = n1 + n2 - 2;
    } else {
        *se = std::sqrt(v1 + v2);
        // Welch-Satterthwaite
        const calc_t num = (v1 + v2) * (v1 + v2);
        const calc_t den = v1 * v1 / (n1 - 1) + v2 * v2 / (n2 - 1);
        *df = den > 0 ? num / den : nan_v();
    }
    return *se > 0 && !std::isnan(*df);
}

}  // namespace

// ---- z-tests ----

TestResult z_test_1samp(calc_t x_bar, calc_t mu0, calc_t sigma, int n, Alt alt) {
    if (!(sigma > 0) || n < 1) {
        return test_error("Need sigma>0, n>=1");
    }
    TestResult r;
    r.se = sigma / std::sqrt(static_cast<calc_t>(n));
    r.statistic = (x_bar - mu0) / r.se;
    r.p_value = p_from_alt_z(r.statistic, alt);
    r.df = nan_v();
    r.df2 = nan_v();
    r.estimate = x_bar;
    r.n1 = n;
    r.ok = true;
    return r;
}

TestResult z_test_2samp(calc_t x1, calc_t s1, int n1, calc_t x2, calc_t s2, int n2, Alt alt) {
    if (!(s1 > 0) || !(s2 > 0) || n1 < 1 || n2 < 1) {
        return test_error("Need sigmas>0, n>=1");
    }
    TestResult r;
    r.se = std::sqrt(s1 * s1 / n1 + s2 * s2 / n2);
    r.estimate = x1 - x2;
    r.statistic = r.estimate / r.se;
    r.p_value = p_from_alt_z(r.statistic, alt);
    r.df = nan_v();
    r.df2 = nan_v();
    r.n1 = n1;
    r.n2 = n2;
    r.ok = true;
    return r;
}

// ---- t-tests ----

TestResult t_test_1samp_summary(calc_t x_bar, calc_t s, int n, calc_t mu0, Alt alt) {
    if (n < 2 || !(s > 0)) {
        return test_error("Need n>=2, s>0");
    }
    TestResult r;
    r.se = s / std::sqrt(static_cast<calc_t>(n));
    r.statistic = (x_bar - mu0) / r.se;
    r.df = n - 1;
    r.df2 = nan_v();
    r.p_value = p_from_alt_t(r.statistic, r.df, alt);
    r.estimate = x_bar;
    r.n1 = n;
    r.ok = true;
    return r;
}

TestResult t_test_1samp(const Array& data, calc_t mu0, Alt alt) {
    calc_t m = 0;
    calc_t s = 0;
    int n = 0;
    if (!mean_sd(data, &m, &s, &n)) {
        return test_error("Need n>=2");
    }
    return t_test_1samp_summary(m, s, n, mu0, alt);
}

TestResult t_test_2samp_summary(calc_t x1, calc_t s1, int n1, calc_t x2, calc_t s2, int n2,
                                bool pooled, Alt alt) {
    calc_t se = 0;
    calc_t df = 0;
    if (!two_samp_t_se_df(s1, n1, s2, n2, pooled, &se, &df)) {
        return test_error("Need n>=2 per sample, s>0");
    }
    TestResult r;
    r.se = se;
    r.df = df;
    r.df2 = nan_v();
    r.estimate = x1 - x2;
    r.statistic = r.estimate / se;
    r.p_value = p_from_alt_t(r.statistic, df, alt);
    r.n1 = n1;
    r.n2 = n2;
    r.ok = true;
    return r;
}

TestResult t_test_2samp(const Array& d1, const Array& d2, bool pooled, Alt alt) {
    calc_t m1 = 0;
    calc_t s1 = 0;
    int n1 = 0;
    calc_t m2 = 0;
    calc_t s2 = 0;
    int n2 = 0;
    if (!mean_sd(d1, &m1, &s1, &n1) || !mean_sd(d2, &m2, &s2, &n2)) {
        return test_error("Need n>=2 per sample");
    }
    return t_test_2samp_summary(m1, s1, n1, m2, s2, n2, pooled, alt);
}

TestResult t_test_paired(const Array& before, const Array& after, Alt alt) {
    calc_t mean = 0;
    calc_t sd = 0;
    int n = 0;
    if (!diff_mean_sd(before, after, &mean, &sd, &n)) {
        return test_error("Need equal lists, n>=2");
    }
    return t_test_1samp_summary(mean, sd, n, 0, alt);
}

// ---- proportion tests ----

TestResult prop_test_1samp(int successes, int n, calc_t p0, Alt alt) {
    if (n < 1 || successes < 0 || successes > n || !(p0 > 0) || !(p0 < 1)) {
        return test_error("Need 0<=x<=n, 0<p0<1");
    }
    TestResult r;
    const calc_t p_hat = static_cast<calc_t>(successes) / n;
    r.se = std::sqrt(p0 * (1 - p0) / n);
    r.estimate = p_hat;
    r.statistic = (p_hat - p0) / r.se;
    r.p_value = p_from_alt_z(r.statistic, alt);
    r.df = nan_v();
    r.df2 = nan_v();
    r.n1 = n;
    r.ok = true;
    return r;
}

TestResult prop_test_2samp(int x1, int n1, int x2, int n2, Alt alt) {
    if (n1 < 1 || n2 < 1 || x1 < 0 || x1 > n1 || x2 < 0 || x2 > n2) {
        return test_error("Need 0<=x<=n");
    }
    const calc_t p1 = static_cast<calc_t>(x1) / n1;
    const calc_t p2 = static_cast<calc_t>(x2) / n2;
    const calc_t pp = static_cast<calc_t>(x1 + x2) / (n1 + n2);
    const calc_t se = std::sqrt(pp * (1 - pp) * (1.0 / n1 + 1.0 / n2));
    if (!(se > 0)) {
        return test_error("Pooled p is 0 or 1");
    }
    TestResult r;
    r.se = se;
    r.estimate = p1 - p2;
    r.statistic = r.estimate / se;
    r.p_value = p_from_alt_z(r.statistic, alt);
    r.df = nan_v();
    r.df2 = nan_v();
    r.n1 = n1;
    r.n2 = n2;
    r.ok = true;
    return r;
}

// ---- chi-square ----

TestResult chisq_gof(const Array& observed, const Array& expected) {
    const int n = observed.size();
    if (n < 2 || expected.size() != n) {
        return test_error("Need equal lists, k>=2");
    }
    calc_t chi2 = 0;
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        observed.read_range(at, m, g_buf_a);
        expected.read_range(at, m, g_buf_b);
        for (int i = 0; i < m; ++i) {
            if (!(g_buf_b[i] > 0)) {
                return test_error("Expected counts must be >0");
            }
            const calc_t d = g_buf_a[i] - g_buf_b[i];
            chi2 += d * d / g_buf_b[i];
        }
    }
    TestResult r;
    r.statistic = chi2;
    r.df = n - 1;
    r.df2 = nan_v();
    r.p_value = dist::chisq_sf(chi2, r.df);
    r.estimate = nan_v();
    r.se = nan_v();
    r.n1 = n;
    r.ok = true;
    return r;
}

TestResult chisq_test_2way(const Array* const cols[], int ncols) {
    if (ncols < 2) {
        return test_error("Need >=2 columns");
    }
    const int rows = cols[0]->size();
    if (rows < 2) {
        return test_error("Need >=2 rows");
    }
    for (int c = 1; c < ncols; ++c) {
        if (cols[c]->size() != rows) {
            return test_error("Column length mismatch");
        }
    }
    // Pass 1: column sums + total (row sums recomputed per chunk in
    // pass 2 — no row-sum storage, tables can be list-sized).
    calc_t col_sum[6] = {};  // ncols <= list count
    if (ncols > 6) {
        return test_error("Max 6 columns");
    }
    calc_t total = 0;
    for (int c = 0; c < ncols; ++c) {
        const Array& a = *cols[c];
        for (int at = 0; at < rows; at += kChunk) {
            const int m = rows - at < kChunk ? rows - at : kChunk;
            a.read_range(at, m, g_buf_a);
            for (int i = 0; i < m; ++i) {
                if (g_buf_a[i] < 0) {
                    return test_error("Counts must be >=0");
                }
                col_sum[c] += g_buf_a[i];
            }
        }
        total += col_sum[c];
        if (!(col_sum[c] > 0)) {
            return test_error("Empty column");
        }
    }
    // Pass 2: chunk rows; per row compute the row sum, then the cell
    // contributions. Reads each column twice per chunk (row sum +
    // contribution) via a per-column staging walk kept in g_buf_b.
    calc_t chi2 = 0;
    for (int at = 0; at < rows; at += kChunk) {
        const int m = rows - at < kChunk ? rows - at : kChunk;
        for (int i = 0; i < m; ++i) {
            g_row_sum[i] = 0;
        }
        for (int c = 0; c < ncols; ++c) {
            cols[c]->read_range(at, m, g_buf_a);
            for (int i = 0; i < m; ++i) {
                g_row_sum[i] += g_buf_a[i];
            }
        }
        for (int i = 0; i < m; ++i) {
            if (!(g_row_sum[i] > 0)) {
                return test_error("Empty row");
            }
        }
        for (int c = 0; c < ncols; ++c) {
            cols[c]->read_range(at, m, g_buf_a);
            for (int i = 0; i < m; ++i) {
                const calc_t e = g_row_sum[i] * col_sum[c] / total;
                const calc_t d = g_buf_a[i] - e;
                chi2 += d * d / e;
            }
        }
    }
    TestResult r;
    r.statistic = chi2;
    r.df = static_cast<calc_t>(rows - 1) * (ncols - 1);
    r.df2 = nan_v();
    r.p_value = dist::chisq_sf(chi2, r.df);
    r.estimate = nan_v();
    r.se = nan_v();
    r.n1 = rows;
    r.n2 = ncols;
    r.ok = true;
    return r;
}

// ---- one-way ANOVA ----

TestResult anova_oneway(const Array* const groups[], int group_count) {
    if (group_count < 2 || group_count > 6) {
        return test_error("Need 2-6 groups");
    }
    calc_t means[6];
    calc_t sds[6];
    int ns[6];
    int total_n = 0;
    calc_t grand_sum = 0;
    for (int g = 0; g < group_count; ++g) {
        if (!mean_sd(*groups[g], &means[g], &sds[g], &ns[g])) {
            return test_error("Need n>=2 per group");
        }
        total_n += ns[g];
        grand_sum += means[g] * ns[g];
    }
    const calc_t grand_mean = grand_sum / total_n;
    calc_t ssb = 0;
    calc_t ssw = 0;
    for (int g = 0; g < group_count; ++g) {
        const calc_t d = means[g] - grand_mean;
        ssb += ns[g] * d * d;
        ssw += (ns[g] - 1) * sds[g] * sds[g];
    }
    const calc_t df1 = group_count - 1;
    const calc_t df2 = total_n - group_count;
    if (!(ssw > 0)) {
        return test_error("Zero within-group variance");
    }
    TestResult r;
    r.statistic = (ssb / df1) / (ssw / df2);
    r.df = df1;
    r.df2 = df2;
    r.p_value = dist::f_sf(r.statistic, df1, df2);
    r.estimate = grand_mean;
    r.se = nan_v();
    r.n1 = total_n;
    r.n2 = group_count;
    r.ok = true;
    return r;
}

// ---- linear regression t-test ----

TestResult linreg_test(const Array& x, const Array& y, Alt alt) {
    const int n = x.size();
    if (n < 3 || y.size() != n) {
        return test_error("Need equal lists, n>=3");
    }
    // Two-pass centered sums for conditioning.
    calc_t sx = 0;
    calc_t sy = 0;
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        x.read_range(at, m, g_buf_a);
        y.read_range(at, m, g_buf_b);
        for (int i = 0; i < m; ++i) {
            sx += g_buf_a[i];
            sy += g_buf_b[i];
        }
    }
    const calc_t mx = sx / n;
    const calc_t my = sy / n;
    calc_t sxx = 0;
    calc_t syy = 0;
    calc_t sxy = 0;
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        x.read_range(at, m, g_buf_a);
        y.read_range(at, m, g_buf_b);
        for (int i = 0; i < m; ++i) {
            const calc_t dx = g_buf_a[i] - mx;
            const calc_t dy = g_buf_b[i] - my;
            sxx += dx * dx;
            syy += dy * dy;
            sxy += dx * dy;
        }
    }
    if (!(sxx > 0)) {
        return test_error("x values are constant");
    }
    const calc_t b = sxy / sxx;
    const calc_t sse = syy - b * sxy;
    const calc_t df = n - 2;
    const calc_t s2 = sse > 0 ? sse / df : 0;
    const calc_t se_b = std::sqrt(s2 / sxx);
    if (!(se_b > 0)) {
        return test_error("Perfect fit (se=0)");
    }
    TestResult r;
    r.estimate = b;
    r.se = se_b;
    r.statistic = b / se_b;
    r.df = df;
    r.df2 = nan_v();
    r.p_value = p_from_alt_t(r.statistic, df, alt);
    r.n1 = n;
    r.ok = true;
    return r;
}

// ---- confidence intervals ----

namespace {

Interval interval_from(calc_t center, calc_t moe, calc_t conf) {
    Interval r;
    r.point_estimate = center;
    r.margin_of_error = moe;
    r.low = center - moe;
    r.high = center + moe;
    r.confidence = conf;
    r.ok = true;
    return r;
}

}  // namespace

Interval ci_mean_z(calc_t x_bar, calc_t sigma, int n, calc_t conf) {
    if (!conf_ok(conf) || !(sigma > 0) || n < 1) {
        return interval_error("Need sigma>0, n>=1, 0<conf<1");
    }
    const calc_t z = dist::normal_inv((1 + conf) / 2, 0, 1);
    return interval_from(x_bar, z * sigma / std::sqrt(static_cast<calc_t>(n)), conf);
}

Interval ci_mean_t_summary(calc_t x_bar, calc_t s, int n, calc_t conf) {
    if (!conf_ok(conf) || n < 2 || !(s > 0)) {
        return interval_error("Need n>=2, s>0, 0<conf<1");
    }
    const calc_t t = dist::t_inv((1 + conf) / 2, n - 1);
    return interval_from(x_bar, t * s / std::sqrt(static_cast<calc_t>(n)), conf);
}

Interval ci_mean_t(const Array& data, calc_t conf) {
    calc_t m = 0;
    calc_t s = 0;
    int n = 0;
    if (!mean_sd(data, &m, &s, &n)) {
        return interval_error("Need n>=2");
    }
    return ci_mean_t_summary(m, s, n, conf);
}

Interval ci_diff_means_summary(calc_t x1, calc_t s1, int n1, calc_t x2, calc_t s2, int n2,
                               calc_t conf, bool pooled) {
    if (!conf_ok(conf)) {
        return interval_error("Need 0<conf<1");
    }
    calc_t se = 0;
    calc_t df = 0;
    if (!two_samp_t_se_df(s1, n1, s2, n2, pooled, &se, &df)) {
        return interval_error("Need n>=2 per sample, s>0");
    }
    const calc_t t = dist::t_inv((1 + conf) / 2, df);
    return interval_from(x1 - x2, t * se, conf);
}

Interval ci_diff_means(const Array& d1, const Array& d2, calc_t conf, bool pooled) {
    calc_t m1 = 0;
    calc_t s1 = 0;
    int n1 = 0;
    calc_t m2 = 0;
    calc_t s2 = 0;
    int n2 = 0;
    if (!mean_sd(d1, &m1, &s1, &n1) || !mean_sd(d2, &m2, &s2, &n2)) {
        return interval_error("Need n>=2 per sample");
    }
    return ci_diff_means_summary(m1, s1, n1, m2, s2, n2, conf, pooled);
}

Interval ci_proportion(int successes, int n, calc_t conf) {
    if (!conf_ok(conf) || n < 1 || successes < 0 || successes > n) {
        return interval_error("Need 0<=x<=n, 0<conf<1");
    }
    const calc_t p = static_cast<calc_t>(successes) / n;
    const calc_t z = dist::normal_inv((1 + conf) / 2, 0, 1);
    return interval_from(p, z * std::sqrt(p * (1 - p) / n), conf);
}

Interval ci_diff_proportions(int x1, int n1, int x2, int n2, calc_t conf) {
    if (!conf_ok(conf) || n1 < 1 || n2 < 1 || x1 < 0 || x1 > n1 || x2 < 0 || x2 > n2) {
        return interval_error("Need 0<=x<=n, 0<conf<1");
    }
    const calc_t p1 = static_cast<calc_t>(x1) / n1;
    const calc_t p2 = static_cast<calc_t>(x2) / n2;
    const calc_t z = dist::normal_inv((1 + conf) / 2, 0, 1);
    const calc_t se = std::sqrt(p1 * (1 - p1) / n1 + p2 * (1 - p2) / n2);
    return interval_from(p1 - p2, z * se, conf);
}

}  // namespace math::stats
