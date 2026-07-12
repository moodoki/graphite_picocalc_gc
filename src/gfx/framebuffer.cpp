#include "gfx/framebuffer.hpp"

#include <algorithm>
#include <cstring>

#include "pico/stdlib.h"

namespace gfx {

namespace {

// Render buffers. Full mode (Pico 2): one whole frame. Strip mode
// (Pico 1): one strip at a time, pushed synchronously.
#if PICOCALC_PICO2
uint16_t frame_buf[platform::kScreenW * platform::kScreenH];
#endif
uint16_t strip_buf[platform::kScreenW * config::kStripHeight];

}  // namespace

// Reserved for a future dual-core display service. The core-1 handshake
// was found to hang on hardware (2026-07-10); rendering is synchronous on
// core 0 for now (matches the known-good vendored path). See D10 / worklog.
void display_service_main() {
    while (true) {
        tight_loop_contents();
    }
}

void Framebuffer::render_frame(RenderFn render, void* ctx, int dirty_y0, int dirty_y1) {
    dirty_y0 = std::max(dirty_y0, 0);
    dirty_y1 = std::min(dirty_y1, platform::kScreenH);
    if (dirty_y0 >= dirty_y1) {
        return;
    }

    if (config::kUseFullFramebuffer) {
#if PICOCALC_PICO2
        // One buffer-sized "strip": the band renders at the top of
        // frame_buf (row() offsets by clip_y0_), so the buffer is
        // scratch, not a persistent frame image.
        buf_ = frame_buf;
        clip_y0_ = dirty_y0;
        clip_y1_ = dirty_y1;
        render(*this, ctx);
        platform::display().push_rect(0, dirty_y0, platform::kScreenW, dirty_y1 - dirty_y0,
                                      frame_buf);
#endif
        return;
    }

    // Strip mode: render each strip of the band and push it
    // synchronously (core 0).
    for (int y0 = dirty_y0; y0 < dirty_y1; y0 += config::kStripHeight) {
        buf_ = strip_buf;
        clip_y0_ = y0;
        const int h =
            (y0 + config::kStripHeight <= dirty_y1) ? config::kStripHeight : dirty_y1 - y0;
        clip_y1_ = y0 + h;
        render(*this, ctx);
        platform::display().push_rect(0, y0, platform::kScreenW, h, strip_buf);
    }
}

void Framebuffer::clear(Color c) {
    fill_rect(0, clip_y0_, platform::kScreenW, clip_y1_ - clip_y0_, c);
}

uint16_t* Framebuffer::row(int y) {
    return buf_ + static_cast<ptrdiff_t>(y - clip_y0_) * platform::kScreenW;
}

void Framebuffer::set_pixel(int x, int y, Color c) {
    if (x < pane_x0_ || x >= pane_x1_ || y < pane_y0_ || y >= pane_y1_ || y < clip_y0_ ||
        y >= clip_y1_) {
        return;
    }
    row(y)[x] = c.rgb565;
}

void Framebuffer::fill_rect(int x, int y, int w, int h, Color c) {
    int const x0 = x < pane_x0_ ? pane_x0_ : x;
    int const x1 = x + w > pane_x1_ ? pane_x1_ : x + w;
    const int cy0 = clip_y0_ > pane_y0_ ? clip_y0_ : pane_y0_;
    const int cy1 = clip_y1_ < pane_y1_ ? clip_y1_ : pane_y1_;
    int const y0 = y < cy0 ? cy0 : y;
    int const y1 = y + h > cy1 ? cy1 : y + h;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    for (int yy = y0; yy < y1; ++yy) {
        uint16_t* p = row(yy) + x0;
        for (int xx = x0; xx < x1; ++xx) {
            *p++ = c.rgb565;
        }
    }
}

void Framebuffer::draw_hline(int x, int y, int w, Color c) {
    fill_rect(x, y, w, 1, c);
}

void Framebuffer::draw_vline(int x, int y, int h, Color c) {
    fill_rect(x, y, 1, h, c);
}

void Framebuffer::draw_rect(int x, int y, int w, int h, Color c) {
    draw_hline(x, y, w, c);
    draw_hline(x, y + h - 1, w, c);
    draw_vline(x, y, h, c);
    draw_vline(x + w - 1, y, h, c);
}

void Framebuffer::draw_line(int x0, int y0, int x1, int y1, Color c) {
    // Bresenham; set_pixel clips per-pixel.
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (true) {
        set_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

Framebuffer& framebuffer() {
    static Framebuffer instance;
    return instance;
}

}  // namespace gfx
