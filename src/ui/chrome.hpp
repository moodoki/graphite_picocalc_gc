#pragma once

#include "gfx/framebuffer.hpp"

namespace ui {

// Shared screen chrome (task 5.1 / 5.2) so every screen has a
// consistent 16px status bar and 20px softkey bar.

constexpr int kStatusBarH = 16;
constexpr int kSoftkeyBarH = 20;

// Modifier flags shown in the status bar.
struct StatusFlags {
    bool second = false;
    bool alpha = false;
};

// Draw the top status bar: title (left) and, right-aligned, the 2nd /
// ALPHA indicators, the angle mode (RAD/DEG), and the display mode.
void draw_status_bar(gfx::Framebuffer& fb, const char* title, StatusFlags flags = {});

// Draw the bottom softkey bar. `labels` is 6 entries (F1..F6); a null or
// empty entry leaves that slot blank.
void draw_softkeys(gfx::Framebuffer& fb, const char* const labels[6]);

}  // namespace ui
