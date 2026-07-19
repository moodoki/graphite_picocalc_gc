#include "apps/plot_screen.hpp"

#include <cstdint>
#include <cstdio>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/lists.hpp"
#include "apps/graph_model.hpp"
#include "apps/graph_screen.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/window_screen.hpp"
#include "graph/graph_state.hpp"

namespace apps {

namespace {

constexpr int kTopY = 44;
constexpr int kRowH = 28;
constexpr int kSoftkeyY = 300;

const char* const kTypeNames[graph::kStatPlotTypeCount] = {"Scatter", "XY-Line", "Histogram", "Box",
                                                           "NormProb"};
const char* const kMarkNames[3] = {"dot", "plus", "cross"};

// Row layout depends on the slot's type:
// 0 Plot, 1 On, 2 Type, 3 X list, then per-type extras.
constexpr int kRowPlot = 0;
constexpr int kRowOn = 1;
constexpr int kRowType = 2;
constexpr int kRowXList = 3;

graph::StatPlotConfig& cfg(int slot) {
    return graph::state().stat_plots[slot];
}

bool type_has_ylist(graph::StatPlotType t) {
    return t == graph::StatPlotType::kScatter || t == graph::StatPlotType::kXyLine;
}

bool type_has_mark(graph::StatPlotType t) {
    return t == graph::StatPlotType::kScatter || t == graph::StatPlotType::kXyLine ||
           t == graph::StatPlotType::kNormalProb;
}

}  // namespace

int PlotScreen::row_count() const {
    const auto& p = cfg(slot_);
    int n = 4;  // Plot, On, Type, X list
    if (type_has_ylist(p.type)) {
        ++n;
    }
    if (type_has_mark(p.type)) {
        ++n;
    }
    if (p.type == graph::StatPlotType::kHistogram) {
        ++n;
    }
    return n;
}

void PlotScreen::on_activate() {
    editing_ = false;
    invalidate_all();
}

void PlotScreen::adjust(int row, int dir) {
    auto& p = cfg(slot_);
    switch (row) {
        case kRowPlot:
            slot_ = (slot_ + dir + graph::kStatPlotSlots) % graph::kStatPlotSlots;
            break;
        case kRowOn:
            p.enabled = !p.enabled;
            break;
        case kRowType: {
            int t = static_cast<int>(p.type);
            t = (t + dir + graph::kStatPlotTypeCount) % graph::kStatPlotTypeCount;
            p.type = static_cast<graph::StatPlotType>(t);
            break;
        }
        case kRowXList:
            p.x_list = static_cast<uint8_t>((p.x_list + dir + math::ListStore::kCount) %
                                            math::ListStore::kCount);
            break;
        default: {
            // Extra rows in display order: Y list, mark, bin width.
            const int extra = row - kRowXList;  // 1-based extra index
            if (type_has_ylist(p.type) && extra == 1) {
                p.y_list = static_cast<uint8_t>((p.y_list + dir + math::ListStore::kCount) %
                                                math::ListStore::kCount);
            } else if (type_has_mark(p.type)) {
                p.mark = static_cast<uint8_t>((p.mark + dir + 3) % 3);
            }
            break;
        }
    }
    if (row_ >= row_count()) {
        row_ = row_count() - 1;
    }
    save_graph_state();
    graph_screen().invalidate();
}

bool PlotScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    auto& p = cfg(slot_);
    const int rows = row_count();
    const bool bin_row = p.type == graph::StatPlotType::kHistogram && row_ == rows - 1;

    if (editing_) {
        if (ev.key == Key::kEnter) {
            math::calc_t v = 0;
            if (math::eval_field(input_.text(), &v) && v >= 0) {
                p.bin_width = v;
                save_graph_state();
                graph_screen().invalidate();
            }
            editing_ = false;
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
            if (row_ > 0) {
                --row_;
            }
            return true;
        case Key::kDown:
            if (row_ < rows - 1) {
                ++row_;
            }
            return true;
        case Key::kLeft:
            if (!bin_row) {
                adjust(row_, -1);
            }
            return true;
        case Key::kRight:
            if (!bin_row) {
                adjust(row_, +1);
            }
            return true;
        case Key::kEnter:
            if (bin_row) {
                char buf[24];
                math::format_number(p.bin_width, buf, sizeof(buf));
                input_.set_text(buf);
                editing_ = true;
            } else {
                adjust(row_, +1);
            }
            return true;
        case Key::kDel:
            if (bin_row) {
                input_.set_text("");
                editing_ = true;
            }
            return true;
        // Global F-key scheme (D20).
        case Key::kF1:
            push_mode_editor();
            return true;
        case Key::kF2:
            ui::screen_manager().push(&window_screen());
            return true;
        case Key::kF3:
            ui::screen_manager().push(&mode_screen());
            return true;
        case Key::kF4:
            goto_graph_trace();
            return true;
        case Key::kF5:
            graph_screen().invalidate();
            ui::screen_manager().switch_to(&graph_screen());
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void PlotScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "STAT PLOTS");

    const auto& p = cfg(slot_);
    const int rows = row_count();
    for (int i = 0; i < rows; ++i) {
        const int y = kTopY + i * kRowH;
        if (i == row_) {
            fb.fill_rect(0, y - 4, platform::kScreenW, kRowH - 4,
                         platform::Color::from_rgb(0, 0, 60));
        }
        const char* label = "";
        char value[24] = {};
        const bool bin_row = p.type == graph::StatPlotType::kHistogram && i == rows - 1;
        const bool mark_row =
            type_has_mark(p.type) && i == kRowXList + 1 + (type_has_ylist(p.type) ? 1 : 0);
        if (i == kRowPlot) {
            label = "Plot";
            std::snprintf(value, sizeof(value), "Plot%d", slot_ + 1);
        } else if (i == kRowOn) {
            label = "Enabled";
            std::snprintf(value, sizeof(value), "%s", p.enabled ? "On" : "Off");
        } else if (i == kRowType) {
            label = "Type";
            std::snprintf(value, sizeof(value), "%s", kTypeNames[static_cast<int>(p.type)]);
        } else if (i == kRowXList) {
            label = "X list";
            std::snprintf(value, sizeof(value), "l%d", p.x_list + 1);
        } else if (type_has_ylist(p.type) && i == kRowXList + 1) {
            label = "Y list";
            std::snprintf(value, sizeof(value), "l%d", p.y_list + 1);
        } else if (bin_row) {
            label = "Bin width";
            if (p.bin_width > 0) {
                math::format_number(p.bin_width, value, sizeof(value));
            } else {
                std::snprintf(value, sizeof(value), "auto");
            }
        } else if (mark_row) {
            label = "Mark";
            std::snprintf(value, sizeof(value), "%s", kMarkNames[p.mark % 3]);
        }
        font.draw_string(fb, 12, y, label, kWhite);
        if (editing_ && i == row_ && bin_row) {
            const int vx = platform::kScreenW / 2;
            input_.render(fb, vx, y, platform::kScreenW - vx - 12, font, true);
        } else {
            font.draw_string(fb, platform::kScreenW - 12 - font.text_width(value), y, value,
                             kGreen);
        }
    }

    const int hy = kTopY + rows * kRowH + 8;
    font.draw_string(fb, 12, hy, "Graph: plots draw with funcs;", kGridLine);
    font.draw_string(fb, 12, hy + 18, "Z on graph = ZoomStat", kGridLine);

    fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, kSoftkeyY + 4, "L/R:CHANGE ENTER:EDIT F5:GRPH ESC:BACK", kGrayLine);
}

PlotScreen& plot_screen() {
    static PlotScreen instance;
    return instance;
}

}  // namespace apps
