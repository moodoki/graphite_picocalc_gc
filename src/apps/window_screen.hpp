#pragma once

#include "ui/input_line.hpp"
#include "ui/screen.hpp"

namespace apps {

// Graph window settings form (task 4.8, mode-aware since task 2.6):
// edit Xmin/Xmax/Ymin/Ymax/Xscl/Yscl, plus Tmin/Tmax/Tstep in
// parametric mode (theta rows follow with task 2.10). UP/DOWN navigate;
// ENTER edits/commits a field. Changes are saved to SD and force a
// graph replot on exit.
class WindowScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    static constexpr int kMaxFields = 10;

    struct FieldRef {
        const char* name;
        double* value;
    };

    int selected_ = 0;
    bool editing_ = false;
    ui::InputLine input_;

    // Fill out[] with the active mode's fields; returns the count.
    static int fields(FieldRef* out);
    static int field_count();
    static double* field_ptr(int i);
    static const char* field_name(int i);
    void begin_edit();
    void commit_edit();
};

WindowScreen& window_screen();

}  // namespace apps
