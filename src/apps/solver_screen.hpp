#pragma once

#include "ui/input_line.hpp"
#include "ui/screen.hpp"
#include "math/types.hpp"

namespace apps {

// Numeric equation solver screen (task 4A.9, phase4-spec §3.4).
// Entered via the typed `solve` command (bare word; `solve(...)` with
// arguments is the inline expression form). Form rows: Equation (text,
// "x^3-2x-5" or "sin(x)=0.5"), Variable (L/R cycle), Lower, Upper,
// Guess (optional — when set, Newton from the guess instead of the
// bracket), Solve. Results (root, f(root), iterations) are cached
// strings; render() is draw-only (strip-safe §8).
class SolverScreen : public ui::Screen {
public:
    SolverScreen() { track_dirty(); }

    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;
    void on_activate() override;

private:
    static constexpr int kRowEquation = 0;
    static constexpr int kRowVariable = 1;
    static constexpr int kRowLower = 2;
    static constexpr int kRowUpper = 3;
    static constexpr int kRowGuess = 4;
    static constexpr int kRowSolve = 5;
    static constexpr int kRowCount = 6;

    int row_ = 0;
    int var_ = 0;  // Index into the variable cycle (x first)
    bool editing_ = false;
    ui::InputLine input_;

    char equation_[96] = "x^2-2";
    math::calc_t lower_ = -10;
    math::calc_t upper_ = 10;
    math::calc_t guess_ = 0;
    bool has_guess_ = false;

    // Cached result lines (render stays draw-only).
    char result_[3][40] = {};
    const char* msg_ = nullptr;

    void clear_result();
    void begin_edit(bool from_empty);
    void commit_edit();
    void solve();
};

SolverScreen& solver_screen();

}  // namespace apps
