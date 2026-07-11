#include "apps/graph_model.hpp"

#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"

namespace apps {

namespace {
constexpr const char* kFuncsPath = "/picocalc/yfuncs.txt";
constexpr const char* kWindowPath = "/picocalc/window.dat";

GraphWindow g_window;
YFunctions g_funcs;

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
    static const Color kPalette[kNumFuncs] = {
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

void save_functions() {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    // One line per slot: "<enabled>\t<expr>\n" (empty expr allowed).
    char out[kNumFuncs * 104];
    int off = 0;
    for (int i = 0; i < kNumFuncs; ++i) {
        off += std::snprintf(out + off, sizeof(out) - off, "%d\t%s\n", g_funcs.enabled[i] ? 1 : 0,
                             g_funcs.expr[i]);
    }
    fs.write_file(kFuncsPath, reinterpret_cast<const uint8_t*>(out), static_cast<size_t>(off));
}

void save_window() {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    fs.write_file(kWindowPath, reinterpret_cast<const uint8_t*>(&g_window), sizeof(g_window));
}

void load_graph_state() {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }

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
        char* tab = std::strchr(line, '\t');
        if (tab != nullptr) {
            g_funcs.enabled[slot] = (line[0] == '1');
            std::strncpy(g_funcs.expr[slot], tab + 1, sizeof(g_funcs.expr[slot]) - 1);
            g_funcs.expr[slot][sizeof(g_funcs.expr[slot]) - 1] = 0;
        }
        ++slot;
        line = nl != nullptr ? nl + 1 : nullptr;
    }
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

}  // namespace apps
