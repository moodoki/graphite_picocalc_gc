#include "apps/window_screen.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gfx/font.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "apps/graph_model.hpp"
#include "apps/graph_screen.hpp"
#include "graph/graph_state.hpp"

namespace apps {

namespace {
constexpr int kTopY = 32;

// 6 fields (function mode) get roomy rows; 9 (parametric) still must
// end above the softkey bar at y=300: 32 + 9*28 = 284.
int row_height(int count) {
    return count > 6 ? 28 : 32;
}
}  // namespace

int WindowScreen::fields(FieldRef* out) {
    auto& st = graph::state();
    auto& w = st.window;
    int n = 0;
    if (st.mode == graph::Mode::kParametric) {
        out[n++] = {"Tmin", &st.t_min};
        out[n++] = {"Tmax", &st.t_max};
        out[n++] = {"Tstep", &st.t_step};
    } else if (st.mode == graph::Mode::kPolar) {
        out[n++] = {"THmin", &st.theta_min};
        out[n++] = {"THmax", &st.theta_max};
        out[n++] = {"THstep", &st.theta_step};
    }
    out[n++] = {"Xmin", &w.x_min};
    out[n++] = {"Xmax", &w.x_max};
    out[n++] = {"Ymin", &w.y_min};
    out[n++] = {"Ymax", &w.y_max};
    out[n++] = {"Xscl", &w.x_scl};
    out[n++] = {"Yscl", &w.y_scl};
    return n;
}

int WindowScreen::field_count() {
    FieldRef refs[kMaxFields];
    return fields(refs);
}

double* WindowScreen::field_ptr(int i) {
    FieldRef refs[kMaxFields];
    const int n = fields(refs);
    return refs[i < n ? i : n - 1].value;
}

const char* WindowScreen::field_name(int i) {
    FieldRef refs[kMaxFields];
    const int n = fields(refs);
    return refs[i < n ? i : n - 1].name;
}

void WindowScreen::on_activate() {
    editing_ = false;
    // The field list depends on the graph mode; re-clamp the selection.
    if (selected_ >= field_count()) {
        selected_ = field_count() - 1;
    }
}

void WindowScreen::begin_edit() {
    char buf[24];
    math::format_number(*field_ptr(selected_), buf, sizeof(buf));
    input_.set_text(buf);
    editing_ = true;
}

void WindowScreen::commit_edit() {
    // Full expression entry (2*pi, pi/180, ...); a bad expression
    // keeps the field's old value instead of committing a junk prefix.
    math::calc_t v = 0;
    if (math::eval_field(input_.text(), &v)) {
        *field_ptr(selected_) = v;
    }
    editing_ = false;
    save_window();
}

bool WindowScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }

    if (editing_) {
        if (ev.key == Key::kEnter) {
            commit_edit();
            return true;
        }
        if (ev.key == Key::kEscape) {
            editing_ = false;
            return true;
        }
        return input_.on_key(ev);
    }

    switch (ev.key) {
        case Key::kUp:
            if (selected_ > 0) {
                --selected_;
            }
            return true;
        case Key::kDown:
            if (selected_ < field_count() - 1) {
                ++selected_;
            }
            return true;
        case Key::kEnter:
        case Key::kF1:
            begin_edit();
            return true;
        case Key::kEscape:
            graph_screen().invalidate();
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void WindowScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    fb.fill_rect(0, 0, platform::kScreenW, 16, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 4, 2, "WINDOW SETTINGS", kGrayLine);

    FieldRef refs[kMaxFields];
    const int count = fields(refs);
    const int row_h = row_height(count);
    // Name column fits the longest field name ("THstep" in polar mode).
    int name_chars = 5;
    for (int i = 0; i < count; ++i) {
        const auto len = static_cast<int>(std::strlen(refs[i].name));
        name_chars = len > name_chars ? len : name_chars;
    }
    for (int i = 0; i < count; ++i) {
        const int y = kTopY + i * row_h;
        const bool sel = (i == selected_);
        if (sel) {
            fb.fill_rect(0, y - 4, platform::kScreenW, row_h, platform::Color::from_rgb(0, 0, 60));
        }
        font.draw_string(fb, 8, y, refs[i].name, kGreen);
        font.draw_char(fb, 8 + name_chars * font.width(), y, '=', kWhite);

        const int vx = 8 + (name_chars + 2) * font.width();
        if (sel && editing_) {
            input_.render(fb, vx, y, platform::kScreenW - vx - 8, font, true);
        } else {
            char buf[24];
            math::format_number(*refs[i].value, buf, sizeof(buf));
            font.draw_string(fb, vx, y, buf, kWhite);
        }
    }

    const int sk = platform::kScreenH - 20;
    fb.fill_rect(0, sk, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, sk + 4, "F1/ENTER:EDIT  ESC:BACK", kGrayLine);
}

WindowScreen& window_screen() {
    static WindowScreen instance;
    return instance;
}

}  // namespace apps
