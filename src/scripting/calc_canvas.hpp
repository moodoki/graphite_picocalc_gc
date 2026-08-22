#pragma once

#include <cstdint>

// The Python script canvas (Phase 6B.8, D80).
//
// A script that draws OWNS THE PANEL until it ends. That is not a style
// choice: pixels normally reach the screen by being pulled — the render loop
// calls the current screen's render() once per strip — and while a script runs
// that never happens, because the screen is blocked in on_key underneath the
// VM. So a drawing binding pushes to the panel itself, and nothing may repaint
// over it afterwards. ProgramScreen stops marking rows dirty (which makes
// ScreenManager::render_frame skip the frame outright) and reports
// owns_display() so the main loop leaves the status bar alone.
//
// SPAN-EXACT, NOT COMPOSITED. There is no framebuffer to read back into on the
// Pico 1 — a full one is 200 KB against 17 KB free — so each primitive pushes
// only the pixels it owns. Two consequences that are API, not implementation:
//
//   * draw_text takes a BACKGROUND colour. Transparent text would mean one
//     push per lit pixel.
//   * A diagonal line goes out as horizontal runs rather than one blit.
//
// The vendored driver does expose read_buffer_spi() (drivers/lcdspi/lcdspi.c),
// so real read-modify-write compositing is available if a script ever needs
// it. It is deliberately not built — see D85.
//
// Composition borrows gfx::scratch_pixels() (the render loop's strip buffer,
// idle whenever a key handler is on the stack) rather than adding SRAM.
namespace scripting::canvas {

// RGB565, matching platform::Color's representation. Kept as a plain integer
// so calc_api.cpp can pass one across without including platform headers.
using Rgb565 = std::uint16_t;

// Named colours from platform::colors, or nullptr. Returns false when the
// name is not one we know — the caller turns that into a Python exception
// rather than silently drawing in black.
bool color_from_name(const char* name, Rgb565* out);
Rgb565 color_from_rgb(int r, int g, int b);

// Has a script taken the panel this run? Set by clear(), cleared by
// begin_run(). ProgramScreen reads it after exec() returns.
void begin_run();
bool owns_display();

void clear(Rgb565 color);
void pixel(int x, int y, Rgb565 color);
void line(int x0, int y0, int x1, int y1, Rgb565 color);
void rect(int x, int y, int w, int h, Rgb565 color, bool fill);

// Draws `s` at (x, y) in `fg` over `bg`. Returns the width drawn, so a caller
// laying out several runs does not have to know the font.
int text(int x, int y, const char* s, Rgb565 fg, Rgb565 bg);

// Font metrics, for a script placing text by rows and columns.
int text_width(const char* s);
int text_height();

}  // namespace scripting::canvas
