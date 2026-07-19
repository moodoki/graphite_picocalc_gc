#pragma once

#include <cstdint>

#include "ui/input_line.hpp"
#include "ui/screen.hpp"
#include "math/types.hpp"

namespace apps {

// Inference screen (task 3D.8, D27): hypothesis tests + confidence
// intervals over math::stats inference. Entered via the typed `test`
// command (alias `infer`). Form phase: a Kind row (L/R cycles the 15
// tests/intervals), then kind-specific rows — Data/Stats source
// toggle, list pickers, numeric InputLine fields, H1 alternative,
// pooled flag, group/column count — then Calculate. Results phase:
// cached text lines (strip-safe §8), ESC back to the form.
class InferScreen : public ui::Screen {
public:
    InferScreen();

    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    static constexpr int kNumSlots = 13;
    static constexpr int kMaxRows = 12;
    static constexpr int kMaxLines = 12;
    static constexpr int kLineChars = 40;

    enum RowType : uint8_t {
        kRowKind,
        kRowSource,
        kRowListA,
        kRowListB,
        kRowAlt,
        kRowPooled,
        kRowCount,
        kRowNum,
        kRowCalc,
    };
    struct Row {
        RowType type = kRowKind;
        uint8_t slot = 0;        // kRowNum: index into vals_
        const char* label = "";  // Display label (num rows + list rows)
    };

    int kind_ = 0;
    int source_ = 0;  // 0 = Data (lists), 1 = Stats (summary fields)
    int list_a_ = 0;
    int list_b_ = 1;
    int alt_ = 0;     // 0 = "!=", 1 = "<", 2 = ">"
    int pooled_ = 0;  // 0 = No, 1 = Yes
    int count_ = 2;   // Chi2 2-way columns / ANOVA groups (l1..lk)
    int row_ = 0;
    bool editing_ = false;
    ui::InputLine input_;
    math::calc_t vals_[kNumSlots] = {};
    const char* msg_ = nullptr;

    bool showing_results_ = false;
    int line_count_ = 0;
    char lines_[kMaxLines][kLineChars] = {};

    int build_rows(Row* rows) const;
    void adjust(const Row& row, int dir);
    void begin_edit(const Row& row, bool from_empty);
    void commit_edit(const Row& row);
    void calculate();
    void add_line(const char* text);
    void add_kv(const char* key, math::calc_t v);
};

InferScreen& infer_screen();

}  // namespace apps
