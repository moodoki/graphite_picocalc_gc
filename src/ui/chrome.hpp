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
// it, and nothing was ever going to: **2nd and ALPHA are a TI keyboard
// convention this calculator has no need for** (D100). A TI-84 has ~50
// keys and reaches several hundred functions by prefixing them; the
// PicoCalc has a full QWERTY keyboard, so a letter is the letter key and
// a function is typed by name. The modifier that buys TI its key count
// buys us nothing.
//
// So this was never a missing feature, and #63 was never a defect --
// only an indicator for a mode that does not exist, taking up budget in
// a bar that turned out to be short of it (#61). If some future feature
// does want a modal prefix, it comes back here with a caller and with a
// reason of its own.
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
