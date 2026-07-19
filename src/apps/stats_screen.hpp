#pragma once

#include <cstdint>

#include "ui/screen.hpp"

namespace apps {

// Statistics screen (task 3B.9): setup form + results view in one
// screen, entered via the typed `stats` command (D20 pattern, like
// `lists`). The form picks an analysis (1-Var, 2-Var, or one of the
// ten regressions), the source lists l1..l6, an optional freq list
// (1-Var) and an optional Y-slot store target (regressions, task
// 3B.8). ENTER on Calculate runs it; results render from cached text
// lines, so render() stays draw-only (strip-safe, §8).
class StatsScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    // 0 = 1-Var, 1 = 2-Var, 2 + RegressionType otherwise.
    static constexpr int kAnalysisCount = 12;
    static constexpr int kMaxLines = 22;
    static constexpr int kLineChars = 40;
    static constexpr int kResVisible = 17;

    enum RowKind : uint8_t { kRowAnalysis, kRowXList, kRowYList, kRowFreq, kRowStore, kRowCalc };

    int analysis_ = 0;
    int x_list_ = 0;       // 0-5
    int y_list_ = 1;       // 0-5
    int freq_list_ = -1;   // -1 = OFF
    int store_slot_ = -1;  // -1 = OFF, else Y1..Y7
    int row_ = 0;
    const char* msg_ = nullptr;  // Form-level error (static string)

    bool showing_results_ = false;
    bool computing_ = false;  // True only during the forced pre-Calculate frame
    int scroll_ = 0;
    int line_count_ = 0;
    char lines_[kMaxLines][kLineChars] = {};

    bool is_regression() const { return analysis_ >= 2; }
    int build_rows(RowKind* rows) const;  // Row list for the analysis
    const char* analysis_name() const;
    void adjust(RowKind kind, int dir);
    void calculate();
    void add_line(const char* text);
    void add_kv(const char* key, double v);
};

StatsScreen& stats_screen();

}  // namespace apps
