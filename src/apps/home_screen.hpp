#pragma once

#include <cstdint>

#include "ui/input_line.hpp"
#include "ui/screen.hpp"

namespace apps {

// Calculator home screen (task 2.5): expression input at the bottom,
// scrollable history above, results via math::Engine.
class HomeScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

    // Load history + variables from SD (called once at boot).
    void load_state();

private:
    static constexpr int kMaxHistory = 50;

    struct Entry {
        char expr[96];
        char result[24];
        bool error;
    };

    Entry history_[kMaxHistory] = {};
    int history_count_ = 0;  // Total entries (capped at kMaxHistory)
    int history_head_ = 0;   // Ring buffer next-write index
    int scroll_ = 0;         // 0 = pinned to newest

    ui::InputLine input_;

    void evaluate_input();
    void push_entry(const char* expr, const char* result, bool error);
    void persist_history_line(const char* expr, const char* result);
    void save_variables();
    void load_variables();

    const Entry* entry_from_newest(int n) const;
};

HomeScreen& home_screen();

}  // namespace apps
