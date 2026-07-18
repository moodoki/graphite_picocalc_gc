#pragma once

#include <cstdint>

#include "ui/input_line.hpp"
#include "ui/screen.hpp"

namespace apps {

// Calculator home screen (task 2.5): expression input at the bottom,
// scrollable history above, results via math::Engine.
class HomeScreen : public ui::Screen {
public:
    HomeScreen() { track_dirty(); }

    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

    // Load history + variables from SD (called once at boot).
    void load_state();

    // The diag screen lives in main.cpp (it owns the self-test state);
    // main registers it here so the typed `diag` command can push it.
    void set_diag_screen(ui::Screen* s) { diag_screen_ = s; }

private:
    static constexpr int kMaxHistory = 50;

    struct Entry {
        char expr[96];
        char result[48];  // Wide enough for a short list "{...}>l1"
        bool error;
    };

    Entry history_[kMaxHistory] = {};
    int history_count_ = 0;  // Total entries (capped at kMaxHistory)
    int history_head_ = 0;   // Ring buffer next-write index
    int scroll_ = 0;         // View scroll, 0 = pinned to newest

    // `cls` display watermark: entries pushed before the mark are
    // hidden from the rendered scrollback but stay in the UP/DOWN
    // recall walk (and in history.txt — cls is session-level).
    uint32_t entries_total_ = 0;
    uint32_t cls_mark_ = 0;

    ui::Screen* diag_screen_ = nullptr;

    // Shell-style input recall (UP/DOWN walk past inputs; Shift+UP/DOWN
    // scroll the view). -1 = not browsing; otherwise entry_from_newest
    // index currently shown in the input line.
    int hist_nav_ = -1;
    char pending_[ui::InputLine::kCapacity] = {};  // Stashed unsent input

    ui::InputLine input_;

    void invalidate_input();
    void invalidate_history();

    void evaluate_input();
    bool handle_command(const char* cmd);
    int visible_count() const;
    void push_entry(const char* expr, const char* result, bool error);
    void persist_history_line(const char* expr, const char* result);
    void save_variables();
    void load_variables();

    const Entry* entry_from_newest(int n) const;
};

HomeScreen& home_screen();

}  // namespace apps
