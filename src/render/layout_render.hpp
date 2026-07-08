#pragma once

#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"
#include "render/layout_node.hpp"

namespace render {

// Draw a layout tree with its top-left corner at (x, y).
void render_node(const LayoutNode* node, gfx::Framebuffer& fb, int x, int y, const gfx::Font& font,
                 platform::Color color);

// Convenience: build `expr` at the font's metrics and draw it at (x, y).
// Returns the rendered width in pixels (0 if nothing drawn).
int draw_expression(gfx::Framebuffer& fb, int x, int y, const char* expr, const gfx::Font& font,
                    platform::Color color);

}  // namespace render
