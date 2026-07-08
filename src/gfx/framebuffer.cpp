#include "gfx/framebuffer.hpp"

#include <cstring>

#include "pico/multicore.h"

namespace gfx {

namespace {

struct PushJob {
    const uint16_t* px;
    int x, y, w, h;
};

// Strip mode: two ping-pong strip buffers. Full mode: one whole frame.
#if PICOCALC_PICO2
uint16_t frame_buf[platform::kScreenW * platform::kScreenH];
#endif
uint16_t strip_buf[2][platform::kScreenW * config::kStripHeight];
PushJob jobs[2];

// Number of jobs handed to core 1 that have not been acknowledged.
int outstanding = 0;

void submit(PushJob* job) {
    multicore_fifo_push_blocking(reinterpret_cast<uintptr_t>(job));
    ++outstanding;
}

void wait_one_ack() {
    (void)multicore_fifo_pop_blocking();
    --outstanding;
}

void drain_acks() {
    while (outstanding > 0) {
        wait_one_ack();
    }
}

}  // namespace

void display_service_main() {
    while (true) {
        auto* job = reinterpret_cast<PushJob*>(multicore_fifo_pop_blocking());
        platform::display().push_rect_dma(job->x, job->y, job->w, job->h, job->px);
        multicore_fifo_push_blocking(1);  // Ack
    }
}

void Framebuffer::render_frame(RenderFn render, void* ctx) {
    if (config::kUseFullFramebuffer) {
#if PICOCALC_PICO2
        drain_acks();  // Single buffer: previous frame must be on the wire
        buf_ = frame_buf;
        clip_y0_ = 0;
        clip_y1_ = platform::kScreenH;
        render(*this, ctx);
        jobs[0] = {frame_buf, 0, 0, platform::kScreenW, platform::kScreenH};
        submit(&jobs[0]);
#endif
        return;
    }

    // Strip mode
    int slot = 0;
    for (int y0 = 0; y0 < platform::kScreenH; y0 += config::kStripHeight) {
        // Reuse of a strip buffer requires its previous push to be done.
        if (outstanding >= 2) {
            wait_one_ack();
        }
        buf_ = strip_buf[slot];
        clip_y0_ = y0;
        const int h = (y0 + config::kStripHeight <= platform::kScreenH) ? config::kStripHeight
                                                                        : platform::kScreenH - y0;
        clip_y1_ = y0 + h;
        render(*this, ctx);
        jobs[slot] = {strip_buf[slot], 0, y0, platform::kScreenW, h};
        submit(&jobs[slot]);
        slot ^= 1;
    }
    drain_acks();
}

void Framebuffer::clear(Color c) {
    fill_rect(0, clip_y0_, platform::kScreenW, clip_y1_ - clip_y0_, c);
}

uint16_t* Framebuffer::row(int y) {
    return buf_ + static_cast<ptrdiff_t>(y - clip_y0_) * platform::kScreenW;
}

void Framebuffer::set_pixel(int x, int y, Color c) {
    if (x < 0 || x >= platform::kScreenW || y < clip_y0_ || y >= clip_y1_) {
        return;
    }
    row(y)[x] = c.rgb565;
}

void Framebuffer::fill_rect(int x, int y, int w, int h, Color c) {
    int x0 = x < 0 ? 0 : x;
    int x1 = x + w > platform::kScreenW ? platform::kScreenW : x + w;
    int y0 = y < clip_y0_ ? clip_y0_ : y;
    int y1 = y + h > clip_y1_ ? clip_y1_ : y + h;
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
