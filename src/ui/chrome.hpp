#pragma once

#include "gfx/framebuffer.hpp"

namespace ui {

// Shared screen chrome (task 5.1 / 5.2) so every screen has a
// consistent 16px status bar and 20px softkey bar.

constexpr int kStatusBarH = 16;
constexpr int kSoftkeyBarH = 20;

// Draw the top status bar: title (left, truncated with an ellipsis if it
// would reach the right-hand block) and, right-aligned, the angle mode
// (RAD/DEG) and the display mode.
//
// There was a StatusFlags parameter here carrying `second` and `alpha`,
// and a `[2nd] [A]` prefix on the right-hand block. Nothing ever passed
// it: `Key::kSecond` and `Key::kAlpha` are declared but referenced
// nowhere, so the calculator has no 2nd mode and no alpha mode, and the
// indicators could not appear (#63). Removed rather than documented,
// because dead code that reads as live code is what made #61's budget
// look 6 characters wider than it is. If 2nd/alpha is ever built, this
// comes back with a caller.
void draw_status_bar(gfx::Framebuffer& fb, const char* title);

// Storage-health indicators (D26): while SD or PSRAM is down, the
// status bar shows a red "SD" / "PSRAM" after the title; they clear
// when the main loop's retry heartbeat recovers the subsystem. The
// main loop calls this on change and invalidates the status band.
void set_health_flags(bool sd_ok, bool psram_ok);

// Draw the bottom softkey bar. `labels` is 6 entries (F1..F6); a null or
// empty entry leaves that slot blank.
void draw_softkeys(gfx::Framebuffer& fb, const char* const labels[6]);

}  // namespace ui
