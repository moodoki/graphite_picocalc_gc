#pragma once

#include "ui/input_line.hpp"
#include "ui/screen.hpp"

namespace apps {

// Graph window settings form (task 4.8): edit Xmin/Xmax/Ymin/Ymax/
// Xscl/Yscl. UP/DOWN navigate; ENTER edits/commits a field. Changes are
// saved to SD and force a graph replot on exit.
class WindowScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    static constexpr int kNumFields = 6;

    int selected_ = 0;
    bool editing_ = false;
    ui::InputLine input_;

    double* field_ptr(int i) const;
    static const char* field_name(int i);
    void begin_edit();
    void commit_edit();
};

WindowScreen& window_screen();

}  // namespace apps
