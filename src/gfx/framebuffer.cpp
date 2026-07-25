#include "gfx/framebuffer.hpp"

#include <algorithm>
#include <cstring>

#include "pico/multicore.h"
#include "pico/stdlib.h"

namespace gfx {

namespace {

// Render buffers. Full mode (Pico 2): one whole frame, pushed
// synchronously on core 0. Strip mode (Pico 1): two ping-pong strip
// buffers so core 0 can render strip N+1 while core 1 DMAs strip N.
#if PICOCALC_PICO2
uint16_t frame_buf[platform::kScreenW * platform::kScreenH];
#endif
uint16_t strip_buf[2][platform::kScreenW * config::kStripHeight];

// A strip handed to core 1 to push. Its buffer must not be reused until
// core 1 acks (see outstanding/wait_one_ack).
struct PushJob {
    const uint16_t* px;
    int x, y, w, h;
};
PushJob jobs[2];

// Jobs submitted to core 1 that have not yet been acked.
int outstanding = 0;

// True once the core-1 service is running. Until then (or if it was
// never started), render_frame pushes synchronously on core 0 rather
// than submitting jobs to a service that can't drain them.
bool service_running = false;

void submit(PushJob* job) {
    multicore_fifo_push_blocking(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(job)));
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

// Core-1 display service (D10, revived 2026-07-25). Pops a strip job,
// DMAs it to the panel, acks. push_rect_dma and its whole call path are
// RAM-resident (__not_in_flash_func) — executing it from flash (XIP)
// contends with core 0's USB stack and hard-faults. That XIP contention,
// not the FIFO handshake, was the 2026-07-10 "stall on frame 1"; see the
// D10 decision addendum + the 2026-07-25 worklog.
void display_service_main() {
    while (true) {
        // The inter-core FIFO is a 32-bit mailbox; passing the job
        // pointer through it is the design (see D10 addendum).
        const auto raw = static_cast<uintptr_t>(multicore_fifo_pop_blocking());
        auto* job = reinterpret_cast<PushJob*>(raw);  // NOLINT(performance-no-int-to-ptr)
        platform::display().push_rect_dma(job->x, job->y, job->w, job->h, job->px);
        multicore_fifo_push_blocking(1);  // ack
    }
}

void start_display_service() {
    if constexpr (!config::kUseFullFramebuffer) {
        if (!service_running) {
            multicore_launch_core1(display_service_main);
            service_running = true;
        }
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

    // Strip mode. Synchronous fallback if the core-1 service isn't
    // running (defensive — no render should occur before boot launches
    // it, but never submit to a service that can't drain the FIFO).
    if (!service_running) {
        for (int y0 = dirty_y0; y0 < dirty_y1; y0 += config::kStripHeight) {
            const int h =
                (y0 + config::kStripHeight <= dirty_y1) ? config::kStripHeight : dirty_y1 - y0;
            buf_ = strip_buf[0];
            clip_y0_ = y0;
            clip_y1_ = y0 + h;
            render(*this, ctx);
            platform::display().push_rect(0, y0, platform::kScreenW, h, strip_buf[0]);
        }
        return;
    }

    // Pipeline core 0 render with core 1 DMA push. Core 0 renders strip
    // N+1 into one ping-pong buffer while core 1 pushes strip N from the
    // other; a buffer is only reused once its push has been acked
    // (outstanding < 2).
    int slot = 0;
    for (int y0 = dirty_y0; y0 < dirty_y1; y0 += config::kStripHeight) {
        const int h =
            (y0 + config::kStripHeight <= dirty_y1) ? config::kStripHeight : dirty_y1 - y0;
        if (outstanding >= 2) {
            wait_one_ack();
        }
        buf_ = strip_buf[slot];
        clip_y0_ = y0;
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
