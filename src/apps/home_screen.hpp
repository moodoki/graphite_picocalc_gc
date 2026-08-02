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

    // Append text at the input line's end (the `const` picker inserts
    // the selected constant's identifier, 4D.17).
    void insert_text(const char* s);

private:
    static constexpr int kMaxHistory = 50;

    // Result-line rendering kind (Phase 5): plain numeric text, an error
    // (red), or a CAS symbolic result (typeset in the accent color).
    enum class ResultKind : uint8_t { kPlain, kError, kSymbolic };

    struct Entry {
        char expr[96];
        char result[48];  // Wide enough for a short list "{...}>l1"
        ResultKind kind;
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

    // Horizontal scroll of the newest result (testdrive 2026-07-20): long
    // list/matrix results are truncated on the history line; when the
    // input is empty and the view is pinned to newest, LEFT/RIGHT pan a
    // window across the full untruncated text kept here. Reset on every
    // new result and on cls.
    char result_full_[128] = {};
    int result_scroll_ = 0;  // Char offset of the visible window's left edge

    ui::InputLine input_;

    void invalidate_input();
    void invalidate_history();

    void evaluate_input();
    bool handle_command(const char* cmd);
    int visible_count() const;
    int result_max_scroll() const;  // Max LEFT/RIGHT pan offset for result_full_
    // Draw the newest result_full_ as a horizontally-pannable one-line window
    // with leading/trailing ellipses when clipped (LEFT/RIGHT pan it).
    void draw_result_window(gfx::Framebuffer& fb, int y, const gfx::Font& font,
                            platform::Color color) const;
    void push_entry(const char* expr, const char* result, ResultKind kind);
    void persist_history_line(const char* expr, const char* result);
    void save_variables();
    void load_variables();

    const Entry* entry_from_newest(int n) const;
};

HomeScreen& home_screen();

}  // namespace apps
