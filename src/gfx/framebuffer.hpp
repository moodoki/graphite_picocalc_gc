#pragma once

#include <cstdint>

#include "config.hpp"
#include "platform/display.hpp"

namespace gfx {

using platform::Color;

// RGB565 render target with clipped drawing primitives.
//
// Two modes (config::kUseFullFramebuffer):
//  - Full framebuffer (Pico 2): one 320x320 buffer, rendered once per
//    frame and pushed to the LCD by core 1.
//  - Line-buffer strips (Pico 1): the frame is rendered in horizontal
//    strips of config::kStripHeight lines into two ping-pong buffers;
//    core 0 renders strip N+1 while core 1 pushes strip N.
//
// All drawing coordinates are absolute screen coordinates; primitives
// clip against the active strip automatically. Renderers must draw the
// full frame each pass (they are invoked once per strip in strip mode).
class Framebuffer {
public:
    using RenderFn = void (*)(Framebuffer& fb, void* ctx);

    // Render rows [dirty_y0, dirty_y1) by invoking `render` (once per
    // strip in strip mode) and push them to the LCD. Rows outside the
    // band keep their current panel contents. Defaults to the full
    // frame. Blocks until the pixels have been pushed.
    void render_frame(RenderFn render, void* ctx, int dirty_y0 = 0,
                      int dirty_y1 = platform::kScreenH);

    // ---- Primitives (clipped) ----
    void clear(Color c);
    void set_pixel(int x, int y, Color c);
    void fill_rect(int x, int y, int w, int h, Color c);
    void draw_hline(int x, int y, int w, Color c);
    void draw_vline(int x, int y, int h, Color c);
    void draw_line(int x0, int y0, int x1, int y1, Color c);
    void draw_rect(int x, int y, int w, int h, Color c);

    // Active vertical window (strip) — renderers can use this to skip
    // work that falls entirely outside the current strip.
    int clip_y0() const { return clip_y0_; }
    int clip_y1() const { return clip_y1_; }  // Exclusive

    // Raw row access for text/blit code. Row must be within the clip
    // window; returns pointer to pixel (0, y).
    uint16_t* row(int y);

private:
    uint16_t* buf_ = nullptr;
    int clip_y0_ = 0;
    int clip_y1_ = 0;
};

// Core 1 entry point: services display push jobs forever.
void display_service_main();

// Framebuffer singleton used by the render loop.
Framebuffer& framebuffer();

}  // namespace gfx
