#pragma once

#include <cstdint>

#include "ui/input_line.hpp"
#include "ui/screen.hpp"

namespace apps {

// §9 evaluator probe (5.2.12): microseconds for the last evaluation.
uint32_t home_eval_us();

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

    // Submit `line` as if it had been typed at the input line and Enter
    // pressed — the *same* code path, not a parallel one (Phase 5.1,
    // task 5.1.1). Serial injection uses this; sharing submit_input() with
    // the Enter key is what makes an injected result trustworthy as
    // equivalent to a typed one.
    //
    // Returns false, having done nothing, when `line` is null, empty once
    // trimmed, or longer than the input line holds — set_text() truncates
    // silently (strncpy, ui::InputLine::kCapacity), and a truncated
    // expression would evaluate to something the caller never sent.
    //
    // When non-null, *result_out and *kind_out receive the newest history
    // entry's result text and a static kind name ("plain" | "symbolic" |
    // "error"). Both are set to nullptr if the line dispatched as a typed
    // command (cls, diag, ...), which pushes no history entry.
    bool submit_line(const char* line, const char** result_out = nullptr,
                     const char** kind_out = nullptr);

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

    // force_decimal suppresses the exact-form probe for this evaluation, the
    // same way a trailing `>dec` does — Alt+Enter's "show me the decimal".
    void evaluate_input(bool force_decimal = false);
    // The plain-Enter body, factored out so the key handler and
    // submit_line() run the identical sequence (trim -> command match ->
    // else evaluate) and cannot drift apart. Assumes input_ is non-empty.
    void submit_input();
    bool handle_command(const char* cmd);
    // `mode [rad|deg|real|rect|polar|float|sci|eng|fixN]` — read or set the
    // angle/number/display modes without the MODE screen, so serial injection
    // (Phase 5.1) can reach the DEGREE and RECT/POLAR checklists.
    bool handle_mode_command(const char* arg);
    void format_modes(char* buf, size_t buf_len) const;
    int visible_count() const;
    int result_max_scroll() const;  // Max LEFT/RIGHT pan offset for result_full_
    // Draw the newest result_full_ as a horizontally-pannable one-line window
    // with leading/trailing ellipses when clipped (LEFT/RIGHT pan it).
    void draw_result_window(gfx::Framebuffer& fb, int y, const gfx::Font& font,
                            platform::Color color) const;
    void push_entry(const char* expr, const char* result, ResultKind kind);
    void persist_history_line(const char* expr, const char* result, ResultKind kind);
    // Trim history.txt back to its tail once it grows past the cap, so the
    // append-only log stays bounded and reboots keep restoring newest lines.
    void compact_history();
    void save_variables();
    void load_variables();

    const Entry* entry_from_newest(int n) const;
};

HomeScreen& home_screen();

}  // namespace apps
