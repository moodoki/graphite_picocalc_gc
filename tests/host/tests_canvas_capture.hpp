#pragma once

#include <cstdio>

// What the host canvas stub recorded, so a test can assert on geometry
// without a panel. See tests/host/host_canvas_stub.cpp.
namespace scripting::canvas {

struct CanvasCapture {
    int clears = 0;
    int pixels = 0;
    int lines = 0;
    int rects = 0;
    int texts = 0;
    int last_x = 0;
    int last_y = 0;
    int last_w = 0;
    int last_h = 0;
    unsigned last_color = 0;
    unsigned last_bg = 0;
    bool last_fill = false;
    char last_text[64] = {};
};

extern CanvasCapture g_capture;

}  // namespace scripting::canvas
