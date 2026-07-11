#pragma once

#include "ui/input_line.hpp"
#include "ui/screen.hpp"

namespace apps {

// Y= function editor (task 4.1): list of Y1..Y7, each a function string
// with an enable checkbox. Navigate with UP/DOWN; ENTER/F1 edits inline;
// F2 toggles enable; F3 clears; F4 jumps to the graph.
class YEditorScreen : public ui::Screen {
public:
    YEditorScreen() { track_dirty(); }

    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    int selected_ = 0;
    bool editing_ = false;
    ui::InputLine input_;

    void begin_edit();
    void commit_edit();
    void invalidate_row(int i);
};

YEditorScreen& y_editor_screen();

}  // namespace apps
