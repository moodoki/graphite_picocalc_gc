#pragma once

#include <cstdint>

#include "ui/input_line.hpp"
#include "ui/screen.hpp"
#include "math/types.hpp"

namespace apps {

// Distribution helper screen (task 3C.8, D25): guided entry for the
// math::dist catalog functions. Entered via the typed `dist` command
// (D20 pattern). Form rows: Distribution (L/R cycle), Function (L/R
// cycle: pdf|pmf / cdf / inv), the parameters of that combination
// (numeric InputLine fields, WINDOW-style full-expression entry), and
// Calculate. The result is computed through the engine from the same
// call the user could type (shown alongside, and Ans updates —
// TI DISTR behavior). render() is draw-only (strip-safe §8): compute
// happens in on_key, results are cached strings.
class DistScreen : public ui::Screen {
public:
    // Parameter slots shared across distributions — switching the
    // distribution/function keeps values whose label persists
    // (pdf -> cdf keeps mu/sd).
    static constexpr int kParamSlots = 13;

    DistScreen();

    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;
    void on_activate() override;

private:
    int dist_ = 0;
    int fn_ = 0;
    int row_ = 0;  // 0 = dist, 1 = fn, 2.. = params, last = Calculate
    bool editing_ = false;
    ui::InputLine input_;

    math::calc_t vals_[kParamSlots] = {};

    // Cached result lines (render stays draw-only).
    char result_[40] = {};
    char expr_[64] = {};
    const char* msg_ = nullptr;

    int param_count() const;
    int row_count() const;  // 2 + params + 1
    void clear_result();
    void begin_edit(bool from_empty);
    void commit_edit();
    void calculate();
};

DistScreen& dist_screen();

}  // namespace apps
