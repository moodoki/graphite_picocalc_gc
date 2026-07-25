#include "apps/graph_model.hpp"

#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"
#include "math/format.hpp"
#include "math/types.hpp"

namespace apps {

namespace {
constexpr const char* kFuncsPath = "/picocalc/yfuncs.txt";
constexpr const char* kWindowPath = "/picocalc/window.dat";

// Function-mode slots and the shared window live in graph::state()
// since task 2.2; these are just short names for them.
GraphWindow& g_window = graph::state().window;
YFunctions& g_funcs = graph::state().y;

constexpr double kPi = 3.14159265358979323846;
}  // namespace

GraphWindow& graph_window() {
    return g_window;
}
YFunctions& y_functions() {
    return g_funcs;
}

platform::Color function_color(int index) {
    using platform::Color;
    static constexpr Color kPalette[kNumFuncs] = {
        Color::from_rgb(40, 100, 255),  // blue
        Color::from_rgb(230, 40, 40),   // red
        Color::from_rgb(0, 190, 0),     // green
        Color::from_rgb(220, 40, 220),  // magenta
        Color::from_rgb(255, 150, 0),   // orange
        Color::from_rgb(0, 200, 220),   // cyan
        Color::from_rgb(250, 220, 40),  // yellow (dark green was too dim on black)
    };
    return kPalette[index % kNumFuncs];
}

platform::Color function_color_dim(int index) {
    using platform::Color;
    // The palette above at ~40% brightness: visibly "the same hue" but
    // dark enough that the curve and grid stay readable on top (4D.11).
    static constexpr Color kDim[kNumFuncs] = {
        Color::from_rgb(16, 40, 100),  // blue
        Color::from_rgb(92, 16, 16),   // red
        Color::from_rgb(0, 76, 0),     // green
        Color::from_rgb(88, 16, 88),   // magenta
        Color::from_rgb(102, 60, 0),   // orange
        Color::from_rgb(0, 80, 88),    // cyan
        Color::from_rgb(100, 88, 16),  // yellow
    };
    return kDim[index % kNumFuncs];
}

// Unified persistence (task 2.23): every change writes the whole
// GraphState image. The old per-file writers are gone; their names
// stay as thin wrappers so Phase 1 call sites don't churn.
void save_graph_state() {
    graph::state().save(platform::storage());
}

void save_functions() {
    save_graph_state();
}

void save_window() {
    save_graph_state();
}

namespace {

// Pre-2.23 formats: window.dat (raw GraphWindow) + yfuncs.txt
// ("<enabled>\t<expr>\n" per slot). Read once, then superseded by
// graphstate.dat; the old files are left in place but ignored.
void migrate_legacy_files(platform::Storage& fs) {
    GraphWindow w;
    if (fs.read_file(kWindowPath, reinterpret_cast<uint8_t*>(&w), sizeof(w)) ==
        static_cast<int>(sizeof(w))) {
        g_window = w;
    }

    char buf[kNumFuncs * 104];
    const int n = fs.read_file(kFuncsPath, reinterpret_cast<uint8_t*>(buf), sizeof(buf) - 1);
    if (n <= 0) {
        return;
    }
    buf[n] = 0;
    char* line = buf;
    int slot = 0;
    while (line != nullptr && *line != 0 && slot < kNumFuncs) {
        char* nl = std::strchr(line, '\n');
        if (nl != nullptr) {
            *nl = 0;
        }
        char const* tab = std::strchr(line, '\t');
        if (tab != nullptr) {
            g_funcs.enabled[slot] = (line[0] == '1');
            std::strncpy(g_funcs.expr[slot], tab + 1, sizeof(g_funcs.expr[slot]) - 1);
            g_funcs.expr[slot][sizeof(g_funcs.expr[slot]) - 1] = 0;
        }
        ++slot;
        line = nl != nullptr ? nl + 1 : nullptr;
    }
}

}  // namespace

void load_graph_state() {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    if (graph::state().load(fs)) {
        // MODE-row math settings ride in the image; push them back
        // into the live math:: state.
        math::set_angle_mode(graph::state().angle);
        math::set_display_mode(graph::state().display);
        math::set_fix_digits(graph::state().fix_digits);
        math::set_number_mode(graph::state().number);
        return;
    }
    // No (or stale) unified image: migrate Phase 1 files and write the
    // unified format going forward.
    migrate_legacy_files(fs);
    graph::state().save(fs);
}

void zoom_standard() {
    g_window = GraphWindow{};  // Defaults are ZStandard (-10..10)
    save_window();
}

void zoom_trig() {
    g_window.x_min = -2.0 * kPi;
    g_window.x_max = 2.0 * kPi;
    g_window.y_min = -4.0;
    g_window.y_max = 4.0;
    g_window.x_scl = kPi / 2.0;
    g_window.y_scl = 1.0;
    save_window();
}

void zoom_in() {
    const double cx = (g_window.x_min + g_window.x_max) / 2.0;
    const double cy = (g_window.y_min + g_window.y_max) / 2.0;
    const double hx = (g_window.x_max - g_window.x_min) / 4.0;
    const double hy = (g_window.y_max - g_window.y_min) / 4.0;
    g_window.x_min = cx - hx;
    g_window.x_max = cx + hx;
    g_window.y_min = cy - hy;
    g_window.y_max = cy + hy;
    save_window();
}

void zoom_out() {
    const double cx = (g_window.x_min + g_window.x_max) / 2.0;
    const double cy = (g_window.y_min + g_window.y_max) / 2.0;
    const double hx = g_window.x_max - g_window.x_min;
    const double hy = g_window.y_max - g_window.y_min;
    g_window.x_min = cx - hx;
    g_window.x_max = cx + hx;
    g_window.y_min = cy - hy;
    g_window.y_max = cy + hy;
    save_window();
}

void zoom_decimal(int width, int height) {
    // 0.1 units per pixel, centered on the origin, so trace x values
    // land on clean tenths (4D.10). The x axis spreads over width-1
    // steps (Viewport::data_x), the y axis over `height` rows.
    g_window.x_min = -0.05 * (width - 1);
    g_window.x_max = 0.05 * (width - 1);
    g_window.y_min = -0.05 * height;
    g_window.y_max = 0.05 * height;
    g_window.x_scl = 1.0;
    g_window.y_scl = 1.0;
    save_window();
}

void zoom_square(int width, int height) {
    // Keep the x range; refit y about its center so one unit spans the
    // same pixel distance on both axes (4D.10; panel pixels are square).
    const double cy = (g_window.y_min + g_window.y_max) / 2.0;
    const double hy = (g_window.x_max - g_window.x_min) * height / width / 2.0;
    g_window.y_min = cy - hy;
    g_window.y_max = cy + hy;
    save_window();
}

}  // namespace apps
