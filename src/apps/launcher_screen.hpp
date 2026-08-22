#pragma once

#include "ui/screen.hpp"

namespace apps {

// App launcher (Phase 6A.2, spec §3.2). A plain vertical list over
// platform::AppRegistry — no icon grid, no categories (Risk 10: expand
// only when a second app actually needs more).
//
// Entered from Home two ways (D58): the F6 softkey and the typed
// `apps`/`app` command.
//
// Unlike CalcMenuScreen/CasMenuScreen, which pop() and then act on the
// screen beneath, the launcher stays on the stack and pushes the app.
// That is what makes the §3.3 exit convention work: ESC in an app is
// screen_manager().pop(), which lands back here rather than on Home.
class LauncherScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    int selected_ = 0;
    int top_ = 0;  // first visible row

    void launch(int index);
    void scroll_into_view();
};

LauncherScreen& launcher_screen();

}  // namespace apps
