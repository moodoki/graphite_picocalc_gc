// Host stand-in for scripting::canvas (6B.8).
//
// The real one pushes to the panel through gfx and platform, which the host
// build has neither of. This records what was asked for instead, which is
// what makes the geometry testable at all — the plan's "point the push at a
// capture buffer rather than the display".

#include <cstring>

#include "scripting/calc_canvas.hpp"
#include "tests_canvas_capture.hpp"

namespace scripting::canvas {

namespace {
bool g_owns = false;
}

CanvasCapture g_capture;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool color_from_name(const char* name, Rgb565* out) {
    // The same handful the real one knows, so name rejection is testable.
    struct N {
        const char* name;
        Rgb565 c;
    };
    static const N kNames[] = {{"black", 0x0000}, {"white", 0xFFFF}, {"blue", 0x001F},
                               {"red", 0xF800},   {"green", 0x0640}};
    for (const N& n : kNames) {
        if (name != nullptr && std::strcmp(name, n.name) == 0) {
            *out = n.c;
            return true;
        }
    }
    return false;
}

Rgb565 color_from_rgb(int r, int g, int b) {
    const auto clamp = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
    return static_cast<Rgb565>(((clamp(r) & 0xF8) << 8) | ((clamp(g) & 0xFC) << 3) |
                               (clamp(b) >> 3));
}

void begin_run() {
    g_owns = false;
    g_capture = CanvasCapture{};
}

bool owns_display() {
    return g_owns;
}

void clear(Rgb565 color) {
    g_owns = true;
    ++g_capture.clears;
    g_capture.last_color = color;
}

void pixel(int x, int y, Rgb565 color) {
    ++g_capture.pixels;
    g_capture.last_x = x;
    g_capture.last_y = y;
    g_capture.last_color = color;
}

void line(int x0, int y0, int x1, int y1, Rgb565 color) {
    ++g_capture.lines;
    g_capture.last_x = x1;
    g_capture.last_y = y1;
    g_capture.last_w = x1 - x0;
    g_capture.last_h = y1 - y0;
    g_capture.last_color = color;
}

void rect(int x, int y, int w, int h, Rgb565 color, bool fill) {
    ++g_capture.rects;
    g_capture.last_x = x;
    g_capture.last_y = y;
    g_capture.last_w = w;
    g_capture.last_h = h;
    g_capture.last_color = color;
    g_capture.last_fill = fill;
}

int text(int x, int y, const char* s, Rgb565 fg, Rgb565 bg) {
    ++g_capture.texts;
    g_capture.last_x = x;
    g_capture.last_y = y;
    g_capture.last_color = fg;
    g_capture.last_bg = bg;
    std::snprintf(g_capture.last_text, sizeof(g_capture.last_text), "%s", s == nullptr ? "" : s);
    return text_width(s);
}

int text_width(const char* s) {
    return s == nullptr ? 0 : static_cast<int>(std::strlen(s)) * 8;
}

int text_height() {
    return 16;
}

}  // namespace scripting::canvas
