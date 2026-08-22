#include "scripting/calc_canvas.hpp"

#include <cstring>

#include "platform/display.hpp"
#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"

namespace scripting::canvas {

namespace {

bool g_owns_display = false;

struct NamedColor {
    const char* name;
    platform::Color color;
};

// The names §4.2 uses, over the palette that already exists. Deliberately
// short: a script wanting anything else passes an (r, g, b) tuple, which is
// why from_rgb is exposed alongside.
constexpr NamedColor kNamed[] = {
    {"black", platform::colors::kBlack},
    {"white", platform::colors::kWhite},
    {"blue", platform::colors::kBlue},
    {"red", platform::colors::kRed},
    {"green", platform::colors::kGreen},
    {"yellow", platform::Color::from_rgb(250, 220, 40)},
    {"cyan", platform::Color::from_rgb(0, 200, 220)},
    {"magenta", platform::Color::from_rgb(220, 40, 220)},
    {"orange", platform::Color::from_rgb(255, 150, 0)},
    {"gray", platform::Color::from_rgb(128, 128, 128)},
    {"grey", platform::Color::from_rgb(128, 128, 128)},
};

// Core 1 may still be pushing the last frame when a binding runs — see
// gfx::display_wait_idle(). Every entry point below waits first, because
// every one of them either writes the shared scratch buffer or drives the
// SPI bus, and core 1 is doing both.
void ensure_idle() {
    gfx::display_wait_idle();
}

int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// One solid horizontal run, pushed exactly. Everything below reduces to this.
void push_run(int x, int y, int w, Rgb565 color) {
    if (y < 0 || y >= platform::kScreenH || w <= 0) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (x + w > platform::kScreenW) {
        w = platform::kScreenW - x;
    }
    if (w <= 0) {
        return;
    }
    std::uint16_t* buf = gfx::scratch_pixels();
    for (int i = 0; i < w; ++i) {
        buf[i] = color;
    }
    platform::display().push_rect(x, y, w, 1, buf);
}

// A solid block, in as few pushes as the scratch buffer allows. One push per
// band of gfx::kScratchPixels / w rows.
void push_block(int x, int y, int w, int h, Rgb565 color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > platform::kScreenW) {
        w = platform::kScreenW - x;
    }
    if (y + h > platform::kScreenH) {
        h = platform::kScreenH - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    std::uint16_t* buf = gfx::scratch_pixels();
    const int rows_per_push = gfx::kScratchPixels / w;
    if (rows_per_push < 1) {
        for (int row = 0; row < h; ++row) {
            push_run(x, y + row, w, color);
        }
        return;
    }
    for (int i = 0; i < w * (rows_per_push < h ? rows_per_push : h); ++i) {
        buf[i] = color;
    }
    for (int row = 0; row < h;) {
        const int take = (h - row) < rows_per_push ? (h - row) : rows_per_push;
        platform::display().push_rect(x, y + row, w, take, buf);
        row += take;
    }
}

}  // namespace

bool color_from_name(const char* name, Rgb565* out) {
    if (name == nullptr) {
        return false;
    }
    // NOLINTNEXTLINE(readability-use-anyofallof) — any_of cannot write *out.
    for (const NamedColor& c : kNamed) {
        if (std::strcmp(name, c.name) == 0) {
            *out = c.color.rgb565;
            return true;
        }
    }
    return false;
}

Rgb565 color_from_rgb(int r, int g, int b) {
    return platform::Color::from_rgb(static_cast<std::uint8_t>(clampi(r, 0, 255)),
                                     static_cast<std::uint8_t>(clampi(g, 0, 255)),
                                     static_cast<std::uint8_t>(clampi(b, 0, 255)))
        .rgb565;
}

void begin_run() {
    g_owns_display = false;
}

bool owns_display() {
    return g_owns_display;
}

void clear(Rgb565 color) {
    ensure_idle();
    // Taking the panel is what clear_screen means. Everything else assumes it
    // has already happened, but does not require it — a script that draws
    // without clearing first still gets its pixels, on top of the editor.
    g_owns_display = true;
    push_block(0, 0, platform::kScreenW, platform::kScreenH, color);
}

void pixel(int x, int y, Rgb565 color) {
    ensure_idle();
    push_run(x, y, 1, color);
}

void line(int x0, int y0, int x1, int y1, Rgb565 color) {
    ensure_idle();
    // Horizontal and vertical are the cases worth having exact: one push and
    // h pushes respectively.
    if (y0 == y1) {
        const int x = x0 < x1 ? x0 : x1;
        push_run(x, y0, (x0 < x1 ? x1 - x0 : x0 - x1) + 1, color);
        return;
    }
    if (x0 == x1) {
        const int y = y0 < y1 ? y0 : y1;
        push_block(x0, y, 1, (y0 < y1 ? y1 - y0 : y0 - y1) + 1, color);
        return;
    }
    // Bresenham, accumulating each scanline's run and pushing it whole rather
    // than a push per pixel — a shallow line is a handful of pushes, a steep
    // one is one per row either way.
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int run_x0 = x0;
    int cx = x0;
    int cy = y0;
    for (;;) {
        const int e2 = 2 * err;
        if (e2 >= dy) {
            if (cx == x1 && cy == y1) {
                break;
            }
            err += dy;
            cx += sx;
        }
        if (e2 <= dx) {
            // Row is finishing: flush the run we accumulated on it.
            const int lo = run_x0 < cx ? run_x0 : cx;
            const int hi = run_x0 < cx ? cx : run_x0;
            push_run(lo, cy, hi - lo + 1, color);
            if (cx == x1 && cy == y1) {
                break;
            }
            err += dx;
            cy += sy;
            run_x0 = cx;
        }
    }
    push_run(run_x0 < cx ? run_x0 : cx, cy, (run_x0 < cx ? cx - run_x0 : run_x0 - cx) + 1, color);
}

void rect(int x, int y, int w, int h, Rgb565 color, bool fill) {
    ensure_idle();
    if (w <= 0 || h <= 0) {
        return;
    }
    if (fill) {
        push_block(x, y, w, h, color);
        return;
    }
    push_run(x, y, w, color);
    push_run(x, y + h - 1, w, color);
    if (h > 2) {
        push_block(x, y + 1, 1, h - 2, color);
        push_block(x + w - 1, y + 1, 1, h - 2, color);
    }
}

int text(int x, int y, const char* s, Rgb565 fg, Rgb565 bg) {
    if (s == nullptr || *s == 0) {
        return 0;
    }
    ensure_idle();
    const gfx::Font& font = gfx::main_font();
    const int ch = font.height();
    if (ch > gfx::kScratchRows) {
        return 0;  // Taller than the borrowed strip band; no font we ship is.
    }

    // The whole run in one pass: bind a Framebuffer over a full-width band of
    // the scratch buffer, let Font::draw_char do the bitmap decoding it
    // already knows, then push the band's rows. Reusing draw_char is the whole
    // reason Framebuffer::bind exists — and doing the string rather than each
    // glyph turns N*ch pushes into ch. The main font is 8x16, exactly the
    // borrowed band's height.
    gfx::Framebuffer fb;
    fb.bind(gfx::scratch_pixels(), y, y + ch);
    fb.clear_pane_clip();

    const int w = font.text_width(s);
    const int x0 = x < 0 ? 0 : x;
    const int x1 = x + w > platform::kScreenW ? platform::kScreenW : x + w;
    if (x1 <= x0) {
        return 0;
    }
    // draw_char with a background fills its own cell, so there is nothing to
    // pre-clear; the bytes outside the glyphs are never pushed.
    font.draw_string(fb, x, y, s, platform::Color{fg}, platform::Color{bg});

    for (int row = 0; row < ch; ++row) {
        const int py = y + row;
        if (py < 0 || py >= platform::kScreenH) {
            continue;
        }
        platform::display().push_rect(x0, py, x1 - x0, 1,
                                      gfx::scratch_pixels() + row * platform::kScreenW + x0);
    }
    return w;
}

int text_width(const char* s) {
    return s == nullptr ? 0 : gfx::main_font().text_width(s);
}

int text_height() {
    return gfx::main_font().height();
}

}  // namespace scripting::canvas
