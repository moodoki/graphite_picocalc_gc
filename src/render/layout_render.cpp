#include "render/layout_render.hpp"

#include "render/layout_builder.hpp"

namespace render {

namespace {

// Draw a parenthesis that scales to `h` pixels tall at column `x`,
// top at `y`. `left` selects '(' vs ')'. For single-line height, the
// font glyph is used; taller spans are drawn with strokes.
void draw_paren(gfx::Framebuffer& fb, int x, int y, int h, bool left, const gfx::Font& font,
                platform::Color color) {
    if (h <= font.height()) {
        font.draw_char(fb, x, y, left ? '(' : ')', color);
        return;
    }
    const int cw = font.width();
    const int bx = left ? x + cw - 2 : x + 1;  // Vertical stroke column
    const int nub = left ? 2 : -2;             // Top/bottom nub dir
    fb.draw_vline(bx, y + 2, h - 4, color);
    fb.set_pixel(bx + nub, y + 1, color);
    fb.set_pixel(bx + 2 * nub, y, color);
    fb.set_pixel(bx + nub, y + h - 2, color);
    fb.set_pixel(bx + 2 * nub, y + h - 1, color);
}

}  // namespace

void render_node(const LayoutNode* node, gfx::Framebuffer& fb, int x, int y, const gfx::Font& font,
                 platform::Color color) {
    if (node == nullptr) {
        return;
    }
    // Skip subtrees entirely outside the active strip.
    if (y >= fb.clip_y1() || y + node->height <= fb.clip_y0()) {
        return;
    }

    switch (node->type) {
        case NodeType::kText:
            font.draw_char(fb, x, y, ' ', color);  // no-op guard for empty
            font.draw_string(fb, x, y, node->t.text, color);
            break;

        case NodeType::kHBox: {
            int cx = x;
            for (int i = 0; i < node->h.count; ++i) {
                const LayoutNode* c = node->h.items[i];
                const int cy = y + (node->baseline - c->baseline);
                render_node(c, fb, cx, cy, font, color);
                cx += c->width;
            }
            break;
        }

        case NodeType::kFraction: {
            const LayoutNode* num = node->bin.a;
            const LayoutNode* den = node->bin.b;
            const int nx = x + (node->width - num->width) / 2;
            render_node(num, fb, nx, y, font, color);
            const int bar_y = y + num->height + 1;
            fb.draw_hline(x + 1, bar_y, node->width - 2, color);
            const int dx = x + (node->width - den->width) / 2;
            render_node(den, fb, dx, y + num->height + 3, font, color);
            break;
        }

        case NodeType::kSuperscript: {
            const LayoutNode* base = node->bin.a;
            const LayoutNode* exp = node->bin.b;
            const int base_y = y + (node->baseline - base->baseline);
            render_node(base, fb, x, base_y, font, color);
            render_node(exp, fb, x + base->width, y, font, color);
            break;
        }

        case NodeType::kParen: {
            const LayoutNode* child = node->paren.child;
            const int cw = font.width();
            draw_paren(fb, x, y, node->height, true, font, color);
            render_node(child, fb, x + cw, y, font, color);
            draw_paren(fb, x + cw + child->width, y, node->height, false, font, color);
            break;
        }
    }
}

int draw_expression(gfx::Framebuffer& fb, int x, int y, const char* expr, const gfx::Font& font,
                    platform::Color color) {
    const Metrics m{font.width(), font.height()};
    LayoutNode* root = build_layout(expr, m);
    if (root == nullptr) {
        return 0;
    }
    render_node(root, fb, x, y, font, color);
    return root->width;
}

}  // namespace render
