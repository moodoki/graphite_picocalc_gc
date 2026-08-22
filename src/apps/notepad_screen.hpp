#pragma once

#include "ui/screen.hpp"

namespace apps {

// Notepad (Phase 6C.1, spec §3.6, D54). A thin wrapper over the shared
// ui::TextEditorWidget: .txt files under /picocalc/notes/, no RUN key,
// no auto-indent. Registered with AppRegistry as "Notepad".
//
// Deliberately the first real app (D64): it needs no interpreter and
// nothing from Phase 5, so it proves the widget's whole edit → save →
// power-cycle → reload loop before 6B builds a second consumer of it.
class NotepadScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;
};

NotepadScreen& notepad_screen();

}  // namespace apps
