#include "graph/stat_plot.hpp"

#include <algorithm>
#include <cmath>

#include "math/dist.hpp"
#include "math/list_ops.hpp"
#include "math/lists.hpp"
#include "math/stats.hpp"

namespace graph {

namespace {

constexpr int kChunk = 256;
constexpr int kMaxBins = 64;

// Slot colors (distinct from the function palette's first entries).
const platform::Color kPlotColors[kStatPlotSlots] = {
    platform::Color::from_rgb(255, 160, 0),   // Plot1 orange
    platform::Color::from_rgb(0, 220, 220),   // Plot2 cyan
    platform::Color::from_rgb(230, 80, 230),  // Plot3 magenta
};

math::calc_t g_buf_x[kChunk];
math::calc_t g_buf_y[kChunk];

// Normprob scratch: sorted data + matching normal quantiles, one pair
// per slot (Array-backed so 10000-element PSRAM lists work).
math::Array g_np_sorted[kStatPlotSlots];
math::Array g_np_quant[kStatPlotSlots];

struct Cache {
    bool valid = false;
    int n = 0;
    // Union data bounds (ZoomStat).
    double x_lo = 0, x_hi = 0, y_lo = 0, y_hi = 0;
    // Histogram.
    int bins = 0;
    double bin_x0 = 0;
    double bin_w = 0;
    int counts[kMaxBins] = {};
    int max_count = 0;
    // Box plot.
    double q1 = 0, med = 0, q3 = 0;
    double whisker_lo = 0, whisker_hi = 0;  // Extremes within the fences
    double fence_lo = 0, fence_hi = 0;      // 1.5 IQR outlier fences
};
Cache g_cache[kStatPlotSlots];

const math::Array& list_of(int idx) {
    return math::lists().list(idx);
}

bool stream_min_max(const math::Array& a, double* lo, double* hi) {
    const int n = a.size();
    if (n < 1) {
        return false;
    }
    bool first = true;
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        a.read_range(at, m, g_buf_x);
        for (int i = 0; i < m; ++i) {
            const double v = g_buf_x[i];
            if (first) {
                *lo = *hi = v;
                first = false;
            } else {
                *lo = v < *lo ? v : *lo;
                *hi = v > *hi ? v : *hi;
            }
        }
    }
    return true;
}

void recompute_slot(int slot) {
    const StatPlotConfig& p = state().stat_plots[slot];
    Cache& c = g_cache[slot];
    c.valid = false;
    if (!p.enabled) {
        g_np_sorted[slot].clear();
        g_np_quant[slot].clear();
        return;
    }
    const math::Array& x = list_of(p.x_list);
    const math::Array& y = list_of(p.y_list);
    const int n = x.size();
    if (n < 1) {
        return;
    }
    c.n = n;
    switch (p.type) {
        case StatPlotType::kScatter:
        case StatPlotType::kXyLine: {
            if (y.size() != n) {
                return;  // Length mismatch — plot skipped
            }
            double ylo = 0;
            double yhi = 0;
            if (!stream_min_max(x, &c.x_lo, &c.x_hi) || !stream_min_max(y, &ylo, &yhi)) {
                return;
            }
            c.y_lo = ylo;
            c.y_hi = yhi;
            c.valid = true;
            break;
        }
        case StatPlotType::kHistogram: {
            double lo = 0;
            double hi = 0;
            if (!stream_min_max(x, &lo, &hi)) {
                return;
            }
            double w = p.bin_width > 0 ? p.bin_width : (hi - lo) / 10;
            if (!(w > 0)) {
                w = 1;  // Constant data: one unit-wide bar
            }
            const double span = hi - lo;
            const int bins = static_cast<int>(std::floor(span / w)) + 1;
            if (bins < 1 || bins > kMaxBins) {
                return;  // Bin width too fine for the cache — skipped
            }
            c.bins = bins;
            c.bin_x0 = lo;
            c.bin_w = w;
            for (int i = 0; i < bins; ++i) {
                c.counts[i] = 0;
            }
            for (int at = 0; at < n; at += kChunk) {
                const int m = n - at < kChunk ? n - at : kChunk;
                x.read_range(at, m, g_buf_x);
                for (int i = 0; i < m; ++i) {
                    int b = static_cast<int>(std::floor((g_buf_x[i] - lo) / w));
                    b = b < 0 ? 0 : b;
                    b = b >= bins ? bins - 1 : b;
                    b = b >= kMaxBins ? kMaxBins - 1 : b;  // Analyzer aid; bins<=kMaxBins
                    ++c.counts[b];
                }
            }
            c.max_count = 0;
            for (int i = 0; i < bins; ++i) {
                c.max_count = c.counts[i] > c.max_count ? c.counts[i] : c.max_count;
            }
            c.x_lo = lo;
            c.x_hi = lo + bins * w;
            c.y_lo = 0;
            c.y_hi = c.max_count;
            c.valid = true;
            break;
        }
        case StatPlotType::kBoxPlot: {
            const auto s = math::stats::one_var(x);
            if (!s.ok || s.n < 1) {
                return;
            }
            c.q1 = s.q1;
            c.med = s.median;
            c.q3 = s.q3;
            const double iqr = s.q3 - s.q1;
            c.fence_lo = s.q1 - 1.5 * iqr;
            c.fence_hi = s.q3 + 1.5 * iqr;
            // Whiskers: extremes within the fences (modified box plot).
            bool first = true;
            for (int at = 0; at < n; at += kChunk) {
                const int m = n - at < kChunk ? n - at : kChunk;
                x.read_range(at, m, g_buf_x);
                for (int i = 0; i < m; ++i) {
                    const double v = g_buf_x[i];
                    if (v < c.fence_lo || v > c.fence_hi) {
                        continue;
                    }
                    if (first) {
                        c.whisker_lo = c.whisker_hi = v;
                        first = false;
                    } else {
                        c.whisker_lo = v < c.whisker_lo ? v : c.whisker_lo;
                        c.whisker_hi = v > c.whisker_hi ? v : c.whisker_hi;
                    }
                }
            }
            c.x_lo = s.min_val;
            c.x_hi = s.max_val;
            c.y_lo = 0;  // Boxes draw in fixed bands; y bounds unused
            c.y_hi = 1;
            c.valid = true;
            break;
        }
        case StatPlotType::kNormalProb: {
            if (n < 2) {
                return;
            }
            // Sorted copy + Blom quantiles, cached as Arrays so render
            // never calls ndtri (Pico 1 softfloat).
            if (!math::listops::copy(x, g_np_sorted[slot]) ||
                !math::listops::sort_asc(g_np_sorted[slot])) {
                return;
            }
            if (!g_np_quant[slot].resize(n)) {
                return;
            }
            for (int at = 0; at < n; at += kChunk) {
                const int m = n - at < kChunk ? n - at : kChunk;
                for (int i = 0; i < m; ++i) {
                    const double a = (at + i + 1 - 0.375) / (n + 0.25);
                    g_buf_y[i] = math::dist::normal_inv(a, 0, 1);
                }
                g_np_quant[slot].write_range(at, m, g_buf_y);
            }
            c.x_lo = g_np_sorted[slot].get(0);
            c.x_hi = g_np_sorted[slot].get(n - 1);
            c.y_lo = g_np_quant[slot].get(0);
            c.y_hi = g_np_quant[slot].get(n - 1);
            c.valid = true;
            break;
        }
        default:
            break;
    }
}

void draw_mark(gfx::Framebuffer& fb, int px, int py, int mark, platform::Color color) {
    switch (mark) {
        case 1:  // plus
            fb.draw_hline(px - 2, py, 5, color);
            fb.draw_vline(px, py - 2, 5, color);
            break;
        case 2:  // cross
            for (int d = -2; d <= 2; ++d) {
                fb.set_pixel(px + d, py + d, color);
                fb.set_pixel(px + d, py - d, color);
            }
            break;
        default:  // dot (2x2 for visibility)
            fb.fill_rect(px, py, 2, 2, color);
            break;
    }
}

void draw_slot(gfx::Framebuffer& fb, const Viewport& vp, int slot) {
    const StatPlotConfig& p = state().stat_plots[slot];
    const Cache& c = g_cache[slot];
    if (!p.enabled || !c.valid) {
        return;
    }
    const platform::Color color = kPlotColors[slot];
    const math::Array& x = list_of(p.x_list);
    const math::Array& y = list_of(p.y_list);

    switch (p.type) {
        case StatPlotType::kScatter:
        case StatPlotType::kXyLine: {
            int prev_px = 0;
            int prev_py = 0;
            for (int at = 0; at < c.n; at += kChunk) {
                const int m = c.n - at < kChunk ? c.n - at : kChunk;
                x.read_range(at, m, g_buf_x);
                y.read_range(at, m, g_buf_y);
                for (int i = 0; i < m; ++i) {
                    const int px = vp.px_x(g_buf_x[i]);
                    const int py = vp.px_y(g_buf_y[i]);
                    draw_mark(fb, px, py, p.mark, color);
                    if (p.type == StatPlotType::kXyLine && (at + i) > 0) {
                        fb.draw_line(prev_px, prev_py, px, py, color);
                    }
                    prev_px = px;
                    prev_py = py;
                }
            }
            break;
        }
        case StatPlotType::kHistogram: {
            const int base_py = vp.px_y(0);
            for (int b = 0; b < c.bins; ++b) {
                if (c.counts[b] == 0) {
                    continue;
                }
                const int x0 = vp.px_x(c.bin_x0 + b * c.bin_w);
                const int x1 = vp.px_x(c.bin_x0 + (b + 1) * c.bin_w);
                const int y1 = vp.px_y(c.counts[b]);
                const int w = x1 - x0;
                const int h = base_py - y1;
                if (w > 0 && h > 0) {
                    fb.fill_rect(x0, y1, w, h, color);
                    fb.draw_rect(x0, y1, w + 1, h + 1, platform::colors::kBlack);
                }
            }
            break;
        }
        case StatPlotType::kBoxPlot: {
            // Fixed horizontal band per slot (TI-style; ignores the y
            // window): slot 0/1/2 at 1/4, 2/4, 3/4 viewport height.
            const int cy = vp.top + ((slot + 1) * vp.height) / 4;
            const int hh = 8;  // Box half-height
            const int pq1 = vp.px_x(c.q1);
            const int pq3 = vp.px_x(c.q3);
            const int pmed = vp.px_x(c.med);
            const int plo = vp.px_x(c.whisker_lo);
            const int phi = vp.px_x(c.whisker_hi);
            fb.draw_rect(pq1, cy - hh, pq3 - pq1 + 1, 2 * hh + 1, color);
            fb.draw_vline(pmed, cy - hh, 2 * hh + 1, color);
            fb.draw_hline(plo, cy, pq1 - plo, color);
            fb.draw_hline(pq3, cy, phi - pq3, color);
            fb.draw_vline(plo, cy - 4, 9, color);
            fb.draw_vline(phi, cy - 4, 9, color);
            // Outliers past the fences, streamed.
            for (int at = 0; at < c.n; at += kChunk) {
                const int m = c.n - at < kChunk ? c.n - at : kChunk;
                x.read_range(at, m, g_buf_x);
                for (int i = 0; i < m; ++i) {
                    const double v = g_buf_x[i];
                    if (v < c.fence_lo || v > c.fence_hi) {
                        draw_mark(fb, vp.px_x(v), cy, 1, color);
                    }
                }
            }
            break;
        }
        case StatPlotType::kNormalProb: {
            const math::Array& xs = g_np_sorted[slot];
            const math::Array& zs = g_np_quant[slot];
            if (xs.size() != c.n || zs.size() != c.n) {
                break;
            }
            for (int at = 0; at < c.n; at += kChunk) {
                const int m = c.n - at < kChunk ? c.n - at : kChunk;
                xs.read_range(at, m, g_buf_x);
                zs.read_range(at, m, g_buf_y);
                for (int i = 0; i < m; ++i) {
                    draw_mark(fb, vp.px_x(g_buf_x[i]), vp.px_y(g_buf_y[i]), p.mark, color);
                }
            }
            break;
        }
        default:
            break;
    }
}

}  // namespace

bool any_stat_plot_enabled() {
    const StatPlotConfig* plots = state().stat_plots;
    return std::any_of(plots, plots + kStatPlotSlots,
                       [](const StatPlotConfig& p) { return p.enabled; });
}

void recompute_stat_plots() {
    for (int i = 0; i < kStatPlotSlots; ++i) {
        recompute_slot(i);
    }
}

void draw_stat_plots(gfx::Framebuffer& fb, const Viewport& vp) {
    for (int i = 0; i < kStatPlotSlots; ++i) {
        draw_slot(fb, vp, i);
    }
}

bool stat_plots_bounds(double* x_lo, double* x_hi, double* y_lo, double* y_hi) {
    bool any = false;
    bool any_y = false;
    for (int i = 0; i < kStatPlotSlots; ++i) {
        const StatPlotConfig& p = state().stat_plots[i];
        const Cache& c = g_cache[i];
        if (!p.enabled || !c.valid) {
            continue;
        }
        if (!any) {
            *x_lo = c.x_lo;
            *x_hi = c.x_hi;
            any = true;
        } else {
            *x_lo = c.x_lo < *x_lo ? c.x_lo : *x_lo;
            *x_hi = c.x_hi > *x_hi ? c.x_hi : *x_hi;
        }
        if (p.type != StatPlotType::kBoxPlot) {  // Boxes ignore the y window
            if (!any_y) {
                *y_lo = c.y_lo;
                *y_hi = c.y_hi;
                any_y = true;
            } else {
                *y_lo = c.y_lo < *y_lo ? c.y_lo : *y_lo;
                *y_hi = c.y_hi > *y_hi ? c.y_hi : *y_hi;
            }
        }
    }
    if (any && !any_y) {
        // Only box plots: keep a sane y span so the window stays valid.
        *y_lo = 0;
        *y_hi = 1;
    }
    return any;
}

}  // namespace graph
