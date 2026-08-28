// platform::Display over a plain RGB565 image in memory (Phase 6.4.0, D93).
//
// The whole seam is push_rect: the shared tree composites into a
// framebuffer and hands rectangles to a panel. Here the "panel" is a
// 320x320 array we can write out as a PPM, which is what makes every
// screen in this calculator inspectable for the first time.
//
// No external dependency of any kind, deliberately. This is what CI runs
// (D93 ordering), so it must build on a runner with nothing installed.

#include "host/display_headless.hpp"

#include <cstdio>

#include "platform/display.hpp"

namespace {

// The composited screen. Not a scratch buffer: push_rect paints into it
// and it holds whatever the last frame left, exactly as a panel does.
uint16_t g_screen[platform::kScreenW * platform::kScreenH] = {};

}  // namespace

namespace platform {

void Display::init() {
    initialized_ = true;
}

void Display::push_rect(int x, int y, int w, int h, const uint16_t* px) {
    if (w <= 0 || h <= 0 || px == nullptr) {
        return;  // Same early-out as the panel driver
    }
    // `px` is tightly packed row-major, exactly w*h pixels, no stride --
    // callers pass full-width strips or a whole frame. Clip anyway: a
    // caller that goes off-screen should produce a wrong image, not a
    // corrupt heap.
    for (int row = 0; row < h; ++row) {
        const int dy = y + row;
        if (dy < 0 || dy >= kScreenH) {
            continue;
        }
        for (int col = 0; col < w; ++col) {
            const int dx = x + col;
            if (dx < 0 || dx >= kScreenW) {
                continue;
            }
            g_screen[dy * kScreenW + dx] = px[row * w + col];
        }
    }
}

void Display::push_rect_dma(int x, int y, int w, int h, const uint16_t* px) {
    // There is no DMA and no second core. The firmware's two paths differ
    // only in how the bytes reach the wire, never in what they are.
    push_rect(x, y, w, h, px);
}

void Display::set_backlight(uint8_t /*level*/) {}

Display& display() {
    static Display instance;
    return instance;
}

}  // namespace platform

namespace host {

bool write_ppm(const char* path) {
    std::FILE* f = std::fopen(path, "wb");
    if (f == nullptr) {
        return false;
    }
    // P6 binary, 8 bits per channel. RGB565 -> RGB888 replicates the high
    // bits into the low ones so full-scale stays full-scale (0x1F -> 0xFF),
    // which a plain left-shift would not do.
    std::fprintf(f, "P6\n%d %d\n255\n", platform::kScreenW, platform::kScreenH);
    for (int i = 0; i < platform::kScreenW * platform::kScreenH; ++i) {
        const uint16_t c = g_screen[i];
        const unsigned r5 = (c >> 11) & 0x1F;
        const unsigned g6 = (c >> 5) & 0x3F;
        const unsigned b5 = c & 0x1F;
        const unsigned char rgb[3] = {
            static_cast<unsigned char>((r5 << 3) | (r5 >> 2)),
            static_cast<unsigned char>((g6 << 2) | (g6 >> 4)),
            static_cast<unsigned char>((b5 << 3) | (b5 >> 2)),
        };
        std::fwrite(rgb, 1, 3, f);
    }
    const bool ok = std::ferror(f) == 0;
    return std::fclose(f) == 0 && ok;
}

}  // namespace host
