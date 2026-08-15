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

    // ---- Pane clip rect (task 2.19) ----
    // Composes with the strip window: primitives draw only inside both.
    // Used by SplitScreen to confine each sub-view to its pane; cleared
    // state is the whole screen. Screen coordinates, x1/y1 exclusive.
    void set_pane_clip(int x0, int y0, int x1, int y1) {
        pane_x0_ = x0 > 0 ? x0 : 0;
        pane_y0_ = y0 > 0 ? y0 : 0;
        pane_x1_ = x1 < platform::kScreenW ? x1 : platform::kScreenW;
        pane_y1_ = y1 < platform::kScreenH ? y1 : platform::kScreenH;
    }
    void clear_pane_clip() { set_pane_clip(0, 0, platform::kScreenW, platform::kScreenH); }

    // Current pane rect — lets a renderer tighten the clip temporarily
    // (e.g. GraphScreen confining curves to the plot rows) and restore
    // the enclosing pane afterwards.
    int pane_x0() const { return pane_x0_; }
    int pane_y0() const { return pane_y0_; }
    int pane_x1() const { return pane_x1_; }
    int pane_y1() const { return pane_y1_; }

    // Raw row access for text/blit code. Row must be within the clip
    // window; returns pointer to pixel (0, y).
    uint16_t* row(int y);

    // Point this Framebuffer at a caller-owned buffer covering rows
    // [y0, y1), so the primitives above can compose into something other
    // than the render loop's own strips.
    //
    // Added for the Python script canvas (6B.8, D80), which draws outside
    // the render path and would otherwise have to duplicate glyph decoding
    // and line stepping. `buf` must hold (y1 - y0) * kScreenW pixels.
    //
    // INVARIANT: only valid while render_frame() is NOT running. It owns
    // buf_/clip_ for the duration of a frame, and a binding that rebound
    // them mid-render would corrupt the frame. Safe from a key handler,
    // because render_frame drains its pushes before returning.
    void bind(uint16_t* buf, int y0, int y1) {
        buf_ = buf;
        clip_y0_ = y0;
        clip_y1_ = y1;
    }

private:
    uint16_t* buf_ = nullptr;
    int clip_y0_ = 0;
    int clip_y1_ = 0;
    int pane_x0_ = 0;
    int pane_y0_ = 0;
    int pane_x1_ = platform::kScreenW;
    int pane_y1_ = platform::kScreenH;
};

// Core 1 entry point: services display push jobs forever.
void display_service_main();

// Launch the core-1 display service (strip mode only). Call once at boot
// after platform::init(). No-op in full-framebuffer mode (Pico 2), where
// the push stays synchronous on core 0. Idempotent.
void start_display_service();

// Framebuffer singleton used by the render loop.
Framebuffer& framebuffer();

// The render loop's strip buffers, lent out as scratch.
//
// They are idle whenever render_frame() is not running — which includes the
// whole time a key handler is on the stack, since render_frame drains its
// pushes before returning. The script canvas (6B.8) composes here rather than
// adding its own multi-KB buffer, on the same "one owner at a time" terms
// platform::io_scratch() carries.
//
// INVARIANT: nothing may hold this across a call that could reach the render
// loop.
// Both strip buffers, contiguous — 16 full-width rows, enough for one line of
// any font this project ships.
uint16_t* scratch_pixels();
constexpr int kScratchPixels = platform::kScreenW * config::kStripHeight * 2;
constexpr int kScratchRows = config::kStripHeight * 2;

}  // namespace gfx
