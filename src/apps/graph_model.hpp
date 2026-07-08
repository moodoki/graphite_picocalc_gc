#pragma once

#include "platform/display.hpp"

namespace apps {

constexpr int kNumFuncs = 7;  // Y1..Y7

struct GraphWindow {
    double x_min = -10.0;
    double x_max = 10.0;
    double y_min = -10.0;
    double y_max = 10.0;
    double x_scl = 1.0;
    double y_scl = 1.0;
};

struct YFunctions {
    char expr[kNumFuncs][96] = {};
    bool enabled[kNumFuncs] = {};

    bool any_enabled() const {
        for (int i = 0; i < kNumFuncs; ++i) {
            if (enabled[i] && expr[i][0] != 0) {
                return true;
            }
        }
        return false;
    }
};

GraphWindow& graph_window();
YFunctions& y_functions();

// Distinct plot color per function slot (spec 7.3 palette).
platform::Color function_color(int index);

// SD persistence (/picocalc/yfuncs.txt, /picocalc/window.dat).
void load_graph_state();
void save_functions();
void save_window();

// Zoom presets / operations on the shared window.
void zoom_standard();
void zoom_trig();
void zoom_in();
void zoom_out();

}  // namespace apps
