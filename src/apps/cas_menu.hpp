#pragma once

#include "ui/screen.hpp"

namespace apps {

// CAS operations menu — Phase 5, phase5-spec.md §10 (task 4D.20). Pushed from
// the home screen (F6) or the typed `cas` command. Picking an operation pops
// back to the home screen and inserts the matching call opener (e.g.
// "factor(") into the input line, so the user fills in the argument — the
// insert-back-into-input pattern from const_screen. Inline CAS syntax
// (4D.21) does the actual evaluation once the line is entered.
class CasMenuScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    int sel_ = 0;

    void select(int i);
};

CasMenuScreen& cas_menu();

}  // namespace apps
