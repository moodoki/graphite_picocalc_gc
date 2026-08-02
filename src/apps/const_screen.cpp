#include "apps/const_screen.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/catalog.hpp"
#include "math/format.hpp"
#include "apps/home_screen.hpp"

namespace apps {

namespace {
constexpr int kTopY = 24;
constexpr int kRowH = 18;
constexpr int kVisibleRows = 15;

// Fixed, non-overlapping columns (8x16 monospace, 8px/char) so the value
// and summary can never overprint each other the way the old two-column
// layout did for long values like hbar (2026-07-27 testdrive). Symbol
// (~4ch) | engine id (~7ch, the ENTER-insert token) | compact value
// (~10ch) | summary (fills the rest, truncated with an ellipsis glyph).
constexpr int kSymX = 4;
constexpr int kNameX = 44;
constexpr int kValX = 108;
constexpr int kSummaryX = 196;

// Short, browse-friendly value for the picker: ~5 significant figures
// with a compact exponent ("2.9979e8", "1.0546e-34"). This is display
// only — ENTER inserts the engine identifier (cs[i].name), so the engine
// still uses the full-precision value. format_number_compact isn't used
// here because it keeps full precision for the scientific-notation range,
// which is exactly where the picker's longest values (hbar, planck) live.
void format_value_short(double v, char* dst, size_t cap) {
    char tmp[24];
    std::snprintf(tmp, sizeof(tmp), "%.5g", v);
    char* e = std::strchr(tmp, 'e');
    if (e == nullptr) {
        std::snprintf(dst, cap, "%s", tmp);
        return;
    }
    *e = 0;                                            // split mantissa / exponent
    const long exp = std::strtol(e + 1, nullptr, 10);  // drops '+' and leading zeros
    std::snprintf(dst, cap, "%se%ld", tmp, exp);
}

// Copy src into dst, truncating with math::kEllipsisGlyph when the drawn
// width would exceed max_px, so text never spills past its column.
void fit_text(const gfx::Font& font, const char* src, int max_px, char* dst, size_t cap) {
    if (cap == 0) {
        return;
    }
    if (font.text_width(src) <= max_px) {
        std::snprintf(dst, cap, "%s", src);
        return;
    }
    const char ell[2] = {math::kEllipsisGlyph, 0};
    const int ell_w = font.text_width(ell);
    size_t n = 0;
    int w = 0;
    while (src[n] != '\0' && n + 2 < cap) {
        const char ch[2] = {src[n], 0};
        const int cw = font.text_width(ch);
        if (w + cw + ell_w > max_px) {
            break;
        }
        w += cw;
        ++n;
    }
    for (size_t k = 0; k < n; ++k) {
        dst[k] = src[k];
    }
    dst[n] = math::kEllipsisGlyph;
    dst[n + 1] = 0;
}
}  // namespace

void ConstScreen::on_activate() {
    desc_scroll_ = 0;
    invalidate_all();
}

bool ConstScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    int count = 0;
    const auto* cs = math::constants(&count);
    switch (ev.key) {
        case Key::kUp:
            if (selected_ > 0) {
                --selected_;
                top_ = selected_ < top_ ? selected_ : top_;
                desc_scroll_ = 0;
                invalidate_all();
            }
            return true;
        case Key::kDown:
            if (selected_ + 1 < count) {
                ++selected_;
                if (selected_ - top_ >= kVisibleRows) {
                    top_ = selected_ - kVisibleRows + 1;
                }
                desc_scroll_ = 0;
                invalidate_all();
            }
            return true;
        case Key::kLeft:
            if (desc_scroll_ > 0) {
                --desc_scroll_;
                invalidate_all();
            }
            return true;
        case Key::kRight: {
            // Scroll the selected row's summary leftward, but only while its
            // tail is still truncated — stop as soon as the remainder fits
            // fully in the column (don't scroll off into empty space).
            const int col_px = platform::kScreenW - 4 - kSummaryX;
            if (gfx::main_font().text_width(cs[selected_].summary + desc_scroll_) > col_px) {
                ++desc_scroll_;
                invalidate_all();
            }
            return true;
        }
        case Key::kEnter:
            home_screen().insert_text(cs[selected_].name);
            ui::screen_manager().pop();
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void ConstScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "CONSTANTS");

    int count = 0;
    const auto* cs = math::constants(&count);
    for (int r = 0; r < kVisibleRows; ++r) {
        const int i = top_ + r;
        if (i >= count) {
            break;
        }
        const int y = kTopY + r * kRowH;
        if (i == selected_) {
            fb.fill_rect(0, y - 2, platform::kScreenW, kRowH, platform::Color::from_rgb(0, 0, 60));
        }
        font.draw_string(fb, kSymX, y, cs[i].symbol, kGreen);
        font.draw_string(fb, kNameX, y, cs[i].name, kGrayLine);
        char val[24];
        format_value_short(cs[i].value, val, sizeof(val));
        font.draw_string(fb, kValX, y, val, kWhite);
        char summary[40];
        const char* desc = cs[i].summary;
        if (i == selected_ && desc_scroll_ > 0) {
            desc += desc_scroll_;  // left/right horizontal scroll (selected row)
        }
        fit_text(font, desc, platform::kScreenW - 4 - kSummaryX, summary, sizeof(summary));
        font.draw_string(fb, kSummaryX, y, summary, kGrayLine);
    }

    const int sk = platform::kScreenH - 20;
    fb.fill_rect(0, sk, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, sk + 4, "ENTER:INSERT <>:DESC ESC:BACK", kGrayLine);
}

ConstScreen& const_screen() {
    static ConstScreen instance;
    return instance;
}

}  // namespace apps
