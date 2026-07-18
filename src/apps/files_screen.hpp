#pragma once

#include "platform/storage.hpp"
#include "ui/screen.hpp"

namespace apps {

// On-device SD listing (test-drive request 2026-07-18): shows the
// /picocalc directory — name, size, [DIR] — so persistence artifacts
// (graphstate.dat, migrated legacy files) can be checked without
// pulling the card. Read-only; refreshed on every activate.
class FilesScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    static constexpr int kMaxEntries = 32;

    platform::Storage::DirEntry entries_[kMaxEntries] = {};
    int count_ = 0;   // -1 = list failed / no card
    int scroll_ = 0;  // First visible entry
    int selected_ = 0;
};

FilesScreen& files_screen();

}  // namespace apps
