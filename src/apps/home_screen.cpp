#include "apps/home_screen.hpp"
#include "platform/system.hpp"  // §9 probe

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/cas/cas_eval.hpp"
#include "math/cas/exact.hpp"
#include "math/cas/serialize.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/frac.hpp"
#include "math/lists.hpp"
#include "math/matrix.hpp"
#include "math/named_lists.hpp"
#include "math/solve_expr.hpp"
#include "math/unified_home.hpp"
#include "math/units.hpp"
#include "render/layout_builder.hpp"
#include "render/layout_render.hpp"
#include "apps/calc_menu.hpp"
#include "apps/cas_menu.hpp"
#include "apps/const_screen.hpp"
#include "apps/dist_screen.hpp"
#include "apps/files_screen.hpp"
#include "apps/graph_model.hpp"  // save_graph_state() for the `mode` command
#include "apps/graph_screen.hpp"
#include "apps/help_screen.hpp"
#include "apps/infer_screen.hpp"
#include "apps/list_editor.hpp"
#include "apps/matrix_editor.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/plot_screen.hpp"
#include "apps/settings_screen.hpp"
#include "apps/solver_screen.hpp"
#include "apps/stats_screen.hpp"
#include "apps/window_screen.hpp"
#include "graph/graph_state.hpp"

namespace apps {

namespace {
constexpr const char* kHistoryPath = "/picocalc/history.txt";
constexpr const char* kVarsPath = "/picocalc/variables.dat";
// Shown as the "expression" on the history line the `mode` command pushes.
constexpr const char* kModeCommandName = "mode";

// history.txt is an append-only log. Only its last kHistoryTailBytes are
// ever loaded (the ring holds kMaxHistory=50 lines), so we (a) read from
// the tail, not the head, and (b) compact the file back down to the tail
// once it grows past kHistoryMaxBytes — otherwise it would grow without
// bound and, once past the read window, reboots would restore stale old
// lines instead of the newest. g_hist_io backs both paths (single-threaded
// UI, never reentrant) so neither carries its own multi-KB static.
constexpr size_t kHistoryTailBytes = 8192;
constexpr long kHistoryMaxBytes = 24576;  // 3x tail: compaction stays rare
char g_hist_io[kHistoryTailBytes];

// evaluate_input's per-branch string scratch, in bss rather than on its
// stack frame (D47). Each result branch declared its own buffers and
// GCC could not overlap them all, leaving an 872 B frame — which sits
// underneath the whole list-expression chain (evaluate ->
// eval_list_into, recursive), and that chain has to fit core 0's 4 KB
// before it is writing into core 1's stack. Same reentrancy argument as
// g_hist_io above: one HomeScreen, one Enter at a time.
//
// The aliases in evaluate_input are `auto&`, so they stay array
// references and every sizeof() at the use sites keeps its old value.
struct EvalScratch {
    char result[128];
    char text[120];
    char num[64];
    char expr[160];
};
EvalScratch g_eval;

// Screen layout (spec section 4.4, sized for the interim 8x12 font).
constexpr int kStatusH = 16;
constexpr int kInputY = 268;
constexpr int kSoftkeyY = 296;

// Exact-form display (Phase 5 §10.1, 4D.24): a second, side-effect-free CAS
// probe run after the numeric result is already committed, so it can only
// ever change what is *shown* — Ans, the store target and the variables all
// still come from the numeric value. Overwrites `result` and returns true
// only on a recognized closed form; the decimal stands otherwise. Shared by
// the REAL and complex dispatch branches below.
bool apply_exact_form(const char* expr, double value, int stored_var, bool suppressed, char* result,
                      size_t result_len) {
    // A store line reads "num=>a"; splicing an exact form into it would mean
    // re-implementing format_scalar_result's store branch, and the CAS parser
    // cannot parse the "->" anyway.
    if (suppressed || stored_var >= 0 || !std::isfinite(value)) {
        return false;
    }
    char exact[48];
    if (!math::cas::exact_form(expr, value, exact, sizeof(exact))) {
        return false;
    }
    std::snprintf(result, result_len, "%s", exact);
    return true;
}
}  // namespace

// Dirty bands for partial redraw (5.6 part 2). The input band is the
// typing fast path (~28 of 320 rows). Enter includes the status bar so
// the battery/mode indicators refresh at every evaluation.
void HomeScreen::invalidate_input() {
    invalidate(kInputY, kSoftkeyY);
}

void HomeScreen::invalidate_history() {
    invalidate(kStatusH, kInputY);
}

void HomeScreen::on_activate() {}

const HomeScreen::Entry* HomeScreen::entry_from_newest(int n) const {
    if (n >= history_count_) {
        return nullptr;
    }
    const int idx = (history_head_ - 1 - n + 2 * kMaxHistory) % kMaxHistory;
    return &history_[idx];
}

// Entries visible in the scrollback: everything since the last `cls`,
// capped by what the ring still holds. The recall walk ignores this.
int HomeScreen::visible_count() const {
    const uint32_t since = entries_total_ - cls_mark_;
    return since < static_cast<uint32_t>(history_count_) ? static_cast<int>(since) : history_count_;
}

// §9 evaluator probe (5.2.12). Started at the top of evaluate_input and
// stopped here, on entry to push_entry -- every evaluation branch funnels
// through this function, and the SD history write happens after it. So the
// window is evaluation + result formatting, with no I/O and no rendering.
namespace {
uint64_t g_probe_t0 = 0;
uint32_t g_probe_us = 0;
}  // namespace

uint32_t home_eval_us() {
    return g_probe_us;
}

void HomeScreen::push_entry(const char* expr, const char* result, ResultKind kind) {
    if (g_probe_t0 != 0) {
        g_probe_us = static_cast<uint32_t>(platform::uptime_us() - g_probe_t0);
        g_probe_t0 = 0;
    }
    ++entries_total_;
    Entry& e = history_[history_head_];
    std::strncpy(e.expr, expr, sizeof(e.expr) - 1);
    e.expr[sizeof(e.expr) - 1] = 0;
    std::strncpy(e.result, result, sizeof(e.result) - 1);
    e.result[sizeof(e.result) - 1] = 0;
    e.kind = kind;
    history_head_ = (history_head_ + 1) % kMaxHistory;
    if (history_count_ < kMaxHistory) {
        ++history_count_;
    }
    scroll_ = 0;
    // Keep the full (untruncated) newest result for LEFT/RIGHT panning.
    std::strncpy(result_full_, result, sizeof(result_full_) - 1);
    result_full_[sizeof(result_full_) - 1] = 0;
    result_scroll_ = 0;
}

int HomeScreen::result_max_scroll() const {
    const int win = (platform::kScreenW - 8) / gfx::main_font().width();
    const int len = static_cast<int>(std::strlen(result_full_));
    return len > win ? len - win : 0;
}

void HomeScreen::draw_result_window(gfx::Framebuffer& fb, int y, const gfx::Font& font,
                                    platform::Color color) const {
    const int win = (platform::kScreenW - 8) / font.width();
    const int maxs = result_max_scroll();
    const int off = result_scroll_ > maxs ? maxs : (result_scroll_ < 0 ? 0 : result_scroll_);
    const int len = static_cast<int>(std::strlen(result_full_));
    char window[64];
    const int w =
        win < static_cast<int>(sizeof(window)) - 1 ? win : static_cast<int>(sizeof(window)) - 1;
    std::strncpy(window, result_full_ + off, static_cast<size_t>(w));
    window[w] = 0;
    // Ellipsis markers: leading when scrolled right, trailing when the text
    // still runs past the window (so a truncated result is legibly scrollable).
    if (off > 0 && w > 0) {
        window[0] = math::kEllipsisGlyph;
    }
    if (off + w < len && w > 0) {
        window[w - 1] = math::kEllipsisGlyph;
    }
    font.draw_string(fb, 4, y, window, color);
}

void HomeScreen::persist_history_line(const char* expr, const char* result, ResultKind kind) {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    // "expr<TAB>result<TAB>kind\n". The trailing kind column (Phase 5) lets
    // symbolic CAS results reload as symbolic rather than flat plain text; a
    // legacy two-field line (no second tab) parses back as kPlain. expr and
    // result are both ASCII and tab-free (the CAS serializer emits no tabs),
    // so the tabs are unambiguous delimiters. Buffer is sized so the two
    // 127-char fields plus separators never truncate the newline away.
    const char kmark = kind == ResultKind::kSymbolic ? 'S' : 'P';
    char line[288];
    const int n = std::snprintf(line, sizeof(line), "%s\t%s\t%c\n", expr, result, kmark);
    if (n < 0) {
        return;
    }
    // Clamp to what actually landed in the buffer: snprintf returns the
    // untruncated length, so a would-be-longer line must not over-read.
    const size_t len = std::min(static_cast<size_t>(n), sizeof(line) - 1);
    fs.append_file(kHistoryPath, reinterpret_cast<const uint8_t*>(line), len);
    compact_history();
}

void HomeScreen::compact_history() {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    const long fsize = fs.file_size(kHistoryPath);
    if (fsize <= kHistoryMaxBytes) {
        return;  // still within bounds — no rewrite
    }
    // Read the last kHistoryTailBytes and rewrite the file with just that
    // (line-aligned: drop the partial leading fragment). Appends are
    // human-paced so this rare O(file) rewrite is cheap; kMaxBytes = 3x the
    // tail keeps it to roughly one rewrite per two tail-buffers of writes.
    const size_t cap = sizeof(g_hist_io) - 1;
    const size_t offset = static_cast<size_t>(fsize) - cap;
    const int n =
        fs.read_file_range(kHistoryPath, offset, reinterpret_cast<uint8_t*>(g_hist_io), cap);
    if (n <= 0) {
        return;
    }
    g_hist_io[n] = 0;
    char* start = std::strchr(g_hist_io, '\n');
    start = start != nullptr ? start + 1 : g_hist_io;
    fs.write_file(kHistoryPath, reinterpret_cast<const uint8_t*>(start), std::strlen(start));
}

namespace {

// variables.dat image (4D.15): versioned since complex storage widened
// Variables — the pre-PCV1 file was a raw 224-byte vars[] dump with no
// header, so it simply fails the magic check and is ignored (one-time
// variable reset on first boot, same precedent as PCL/PCM/PCG bumps).
struct VarsImage {
    char magic[4];
    math::calc_t vars[math::Variables::kCount];
    math::calc_t imag[math::Variables::kCount];
};
constexpr char kVarsMagic[4] = {'P', 'C', 'V', '1'};

}  // namespace

void HomeScreen::save_variables() {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    const auto& v = math::engine().vars();
    static VarsImage img;
    std::memcpy(img.magic, kVarsMagic, sizeof(img.magic));
    std::memcpy(img.vars, v.vars, sizeof(img.vars));
    std::memcpy(img.imag, v.imag, sizeof(img.imag));
    fs.write_file(kVarsPath, reinterpret_cast<const uint8_t*>(&img), sizeof(img));
}

void HomeScreen::load_variables() {
    auto& fs = platform::storage();
    auto& v = math::engine().vars();
    static VarsImage img;
    const int n = fs.read_file(kVarsPath, reinterpret_cast<uint8_t*>(&img), sizeof(img));
    if (n != static_cast<int>(sizeof(img)) ||
        std::memcmp(img.magic, kVarsMagic, sizeof(img.magic)) != 0) {
        return;  // Missing, old-format, or truncated: keep defaults
    }
    std::memcpy(v.vars, img.vars, sizeof(v.vars));
    std::memcpy(v.imag, img.imag, sizeof(v.imag));
}

void HomeScreen::load_state() {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    load_variables();

    // Load the tail of the history file (plaintext "expr\tresult[\tkind]"
    // lines, decision D4; the kind column is Phase 5). The last
    // kHistoryTailBytes hold at least 50 full-size lines — the whole ring.
    // Reading the tail (not the head) is what keeps the newest entries
    // visible once the log has grown past one buffer.
    const long fsize = fs.file_size(kHistoryPath);
    if (fsize <= 0) {
        return;
    }
    const size_t cap = sizeof(g_hist_io) - 1;
    const size_t offset = static_cast<size_t>(fsize) > cap ? static_cast<size_t>(fsize) - cap : 0;
    const int n =
        fs.read_file_range(kHistoryPath, offset, reinterpret_cast<uint8_t*>(g_hist_io), cap);
    if (n <= 0) {
        return;
    }
    g_hist_io[n] = 0;
    char* line = g_hist_io;
    // When we started mid-file, the first (partial) line is a fragment —
    // skip past the first newline so parsing begins on a whole line.
    if (offset > 0) {
        char* first_nl = std::strchr(g_hist_io, '\n');
        line = first_nl != nullptr ? first_nl + 1 : nullptr;
    }
    while (line != nullptr && *line != 0) {
        char* nl = std::strchr(line, '\n');
        if (nl != nullptr) {
            *nl = 0;
        }
        char* sep = std::strchr(line, '\t');
        if (sep != nullptr) {
            *sep = 0;
            char* result = sep + 1;
            // Optional trailing kind column (Phase 5): "result<TAB>S|P".
            // A legacy line with no second tab reloads as kPlain.
            ResultKind kind = ResultKind::kPlain;
            char* ksep = std::strchr(result, '\t');
            if (ksep != nullptr) {
                *ksep = 0;
                if (ksep[1] == 'S') {
                    kind = ResultKind::kSymbolic;
                }
            }
            push_entry(line, result, kind);
        }
        line = nl != nullptr ? nl + 1 : nullptr;
    }
    scroll_ = 0;
}

namespace {

// "num" or "num>a" for the store form (shared by the scalar and
// list-expression result paths).
void format_scalar_result(const math::EvalResult& res, char* out, size_t out_len) {
    if (!res.ok) {
        std::snprintf(out, out_len, "%s", res.error);
        return;
    }
    char num[24];
    math::format_number(res.value, num, sizeof(num));
    if (res.stored_var >= 0) {
        const char name =
            res.stored_var < 26 ? static_cast<char>('a' + res.stored_var) : 't';  // theta
        std::snprintf(out, out_len, "%s%c%c", num, gfx::kGlyphStore, name);
    } else {
        std::snprintf(out, out_len, "%s", num);
    }
}

}  // namespace

void HomeScreen::evaluate_input(bool force_decimal) {
    if (input_.empty()) {
        return;
    }
    g_probe_t0 = platform::uptime_us();
    g_probe_us = 0;

    // Inline CAS (Phase 5, 4D.21): recognize a single diff()/integ()/
    // factor()/expand()/simplify()/solve() call and route it to the symbolic
    // engine. evaluate_home returns kNone for everything else — including
    // solve() carrying numeric bounds/guess, which the numeric solver below
    // owns (P5-4 shape split) — so the existing paths are untouched. CAS is
    // display-only: it never commits Ans, a store, or variables (P5-1/P5-2).
    {
        const bool allow_complex = math::number_mode() != math::NumberMode::kReal;
        const math::cas::HomeResult cr = math::cas::evaluate_home(input_.text(), allow_complex);
        if (cr.kind != math::cas::HomeKind::kNone) {
            auto& result = g_eval.result;
            ResultKind rkind = ResultKind::kSymbolic;
            if (cr.kind == math::cas::HomeKind::kError) {
                std::snprintf(result, sizeof(result), "%s", cr.error);
                rkind = ResultKind::kError;
            } else if (cr.kind == math::cas::HomeKind::kSolutions) {
                // "x = {s1, s2, ...}"
                std::size_t w = 0;
                w += static_cast<std::size_t>(
                    std::snprintf(result + w, sizeof(result) - w, "%c = {", cr.var));
                for (int i = 0; i < cr.count && w < sizeof(result) - 1; ++i) {
                    if (i > 0 && w < sizeof(result) - 1) {
                        result[w++] = ',';
                    }
                    w += math::cas::expr_to_string(cr.solutions[i], result + w, sizeof(result) - w);
                }
                if (w < sizeof(result) - 1) {
                    result[w++] = '}';
                }
                result[w] = 0;
            } else {  // kExpr
                math::cas::expr_to_string(cr.result, result, sizeof(result));
            }
            push_entry(input_.text(), result, rkind);
            if (rkind != ResultKind::kError) {
                persist_history_line(input_.text(), result, rkind);
            }
            input_.clear();
            hist_nav_ = -1;
            pending_[0] = 0;
            return;
        }
    }

    // Inline solve() calls become numeric literals first (Phase 4A),
    // so they compose inside any downstream path. History shows the
    // original input; evaluation continues on the substituted text.
    auto& expr = g_eval.expr;
    std::snprintf(expr, sizeof(expr), "%s", input_.text());
    if (math::solveexpr::contains_solve(expr)) {
        const char* serr = nullptr;
        if (!math::solveexpr::substitute(expr, sizeof(expr), &serr)) {
            push_entry(input_.text(), serr, ResultKind::kError);
            input_.clear();
            hist_nav_ = -1;
            pending_[0] = 0;
            return;
        }
    }
    // Unit conversions (4D.18): convert(v,"mi","km") calls become
    // numeric literals the same way (string args can't ride tinyexpr).
    if (math::unitexpr::contains_convert(expr)) {
        const char* uerr = nullptr;
        if (!math::unitexpr::substitute(expr, sizeof(expr), &uerr)) {
            push_entry(input_.text(), uerr, ResultKind::kError);
            input_.clear();
            hist_nav_ = -1;
            pending_[0] = 0;
            return;
        }
    }

    // ">frac" / ">dec" display suffixes (4D.2, TI's Ans>Frac): strip the
    // suffix now and set to_frac; the result is reformatted as fractions
    // further down, per result kind (scalar in the scalar path, matrix in
    // the matrix path). >dec is the default display.
    bool to_frac = false;
    bool to_dec = force_decimal;
    {
        const size_t elen = std::strlen(expr);
        if (elen > 5 && std::strcmp(expr + elen - 5, ">frac") == 0) {
            expr[elen - 5] = 0;
            to_frac = true;
        } else if (elen > 4 && std::strcmp(expr + elen - 4, ">dec") == 0) {
            expr[elen - 4] = 0;  // Decimal is the default display
            to_dec = true;       // ... and suppresses the exact-form probe (4D.24)
        }
    }

    // One evaluator (Phase 5.2, task 5.2.10). This replaces the matexpr ->
    // listexpr -> scalar cascade that stood here, its REAL-mode probe, and its
    // four result-rendering branches. What is left is display and persistence:
    // the evaluation, the commits and the formatting all happen in one call,
    // and `commit` says exactly what changed.
    //
    // The REAL-mode probe is GONE, which is the clearest single sign the split
    // was the problem rather than a workaround. It existed because tinyexpr
    // cannot see complex values, so REAL mode had to run complexexpr first
    // purely to ask "would this be non-real?" — two evaluations, two string
    // scans (mentions_i, refs_complex_var), one answer. One evaluator answers
    // it in one run, because the gate is inside the commit (D30, 5.2.8).
    {
        auto& result = g_eval.result;
        const math::unified::HomeResult ures = math::unified::evaluate_home(expr, to_frac);
        const bool error = ures.kind == math::unified::HomeKind::kError;
        ResultKind rkind = ResultKind::kPlain;
        if (error) {
            std::snprintf(result, sizeof(result), "%s", ures.error);
        } else if (ures.store_label[0] != 0) {
            std::snprintf(result, sizeof(result), "%s%c%s", ures.text, gfx::kGlyphStore,
                          ures.store_label);
        } else {
            std::snprintf(result, sizeof(result), "%s", ures.text);
        }
        if (!error && ures.exact_form_ok &&
            apply_exact_form(expr, ures.scalar_value, -1, to_dec, result, sizeof(result))) {
            rkind = ResultKind::kSymbolic;
        }
        push_entry(input_.text(), result, error ? ResultKind::kError : rkind);
        if (!error) {
            persist_history_line(input_.text(), result, rkind);
            save_variables();
            const math::unified::Commit& c = ures.commit;
            if (c.mat_ans) {
                math::save_ans(platform::storage());
            }
            if (c.matrix >= 0) {
                math::matrices().save(platform::storage(), c.matrix);
            }
            if (c.names_modified) {
                math::named_lists().save_index(platform::storage());
            }
            // Every ref written, not just the store target — an in-place sort
            // and mat2list both write without one (the D35 gap, 4D.13).
            for (int r = 0; r < math::kNamedRefBase + math::NamedLists::kMax; ++r) {
                if ((c.lists_mask & (1U << r)) == 0) {
                    continue;
                }
                if (r < math::kNamedRefBase) {
                    math::lists().save(platform::storage(), r);
                } else {
                    math::named_lists().save(platform::storage(), r - math::kNamedRefBase);
                }
            }
        }
        input_.clear();
        hist_nav_ = -1;
        pending_[0] = 0;
        return;
    }
}

// The plain-Enter body (Phase 5.1, task 5.1.1). Extracted verbatim from
// on_key's kEnter case so that a typed Enter and an injected line run the
// identical sequence — trim, command match, else evaluate. Keeping one copy
// is the whole reason an injected result can be trusted as equivalent to a
// typed one; two copies would drift.
void HomeScreen::submit_input() {
    // Trimmed command match first (cls, help, ...).
    char cmd[16];
    const char* t = input_.text();
    while (*t == ' ') {
        ++t;
    }
    size_t len = std::strlen(t);
    while (len > 0 && t[len - 1] == ' ') {
        --len;
    }
    if (len > 0 && len < sizeof(cmd)) {
        std::memcpy(cmd, t, len);
        cmd[len] = 0;
        if (handle_command(cmd)) {
            input_.clear();
            hist_nav_ = -1;
            pending_[0] = 0;
            invalidate(0, kSoftkeyY);
            return;
        }
    }
    evaluate_input();
    invalidate(0, kSoftkeyY);  // History + input + status bar
}

bool HomeScreen::submit_line(const char* line, const char** result_out, const char** kind_out) {
    if (result_out != nullptr) {
        *result_out = nullptr;
    }
    if (kind_out != nullptr) {
        *kind_out = nullptr;
    }
    if (line == nullptr) {
        return false;
    }
    // Reject rather than truncate. set_text() is strncpy-based and would
    // silently clip at kCapacity, evaluating an expression the caller never
    // sent — the failure mode that matters most for an automated harness,
    // since it looks like a result rather than an error.
    if (std::strlen(line) >= ui::InputLine::kCapacity) {
        return false;
    }
    input_.set_text(line);
    if (input_.empty()) {
        return false;
    }

    // Commands push no history entry; compare the counter rather than
    // inspecting the newest entry, which would otherwise report the
    // *previous* line's result as if it were this one's.
    const uint32_t before = entries_total_;
    submit_input();
    if (entries_total_ == before) {
        return true;  // Dispatched as a typed command
    }

    const Entry* e = entry_from_newest(0);
    if (e == nullptr) {
        return true;
    }
    if (result_out != nullptr) {
        *result_out = e->result;
    }
    if (kind_out != nullptr) {
        switch (e->kind) {
            case ResultKind::kSymbolic:
                *kind_out = "symbolic";
                break;
            case ResultKind::kError:
                *kind_out = "error";
                break;
            case ResultKind::kPlain:
            default:
                *kind_out = "plain";
                break;
        }
    }
    return true;
}

// Typed commands (2026-07-18): lowercase-only (input is
// Current modes as a compact, parseable triple: "<angle> <number> <display>",
// e.g. "RAD REAL FLOAT" or "DEG RECT FIX3" (Phase 5.1). Deliberately ASCII —
// RECT/POLAR rather than the screen's "a+bi"/"r∠θ", which carry font glyph
// bytes that a host script would have to decode.
void HomeScreen::format_modes(char* buf, size_t buf_len) const {
    const char* angle = math::angle_mode() == math::AngleMode::kDegrees ? "DEG" : "RAD";
    const char* number = "REAL";
    if (math::number_mode() == math::NumberMode::kRectangular) {
        number = "RECT";
    } else if (math::number_mode() == math::NumberMode::kPolar) {
        number = "POLAR";
    }
    char display[8];
    switch (math::display_mode()) {
        case math::DisplayMode::kFix:
            std::snprintf(display, sizeof(display), "FIX%d", math::fix_digits());
            break;
        case math::DisplayMode::kSci:
            std::snprintf(display, sizeof(display), "SCI");
            break;
        case math::DisplayMode::kEng:
            std::snprintf(display, sizeof(display), "ENG");
            break;
        case math::DisplayMode::kFloat:
        default:
            std::snprintf(display, sizeof(display), "FLOAT");
            break;
    }
    std::snprintf(buf, buf_len, "%s %s %s", angle, number, display);
}

// `mode [keyword]` (Phase 5.1) — read or set angle/number/display mode from
// the home screen instead of the MODE screen's arrow-key navigation. Added
// because serial injection (5.1) can submit lines but not drive a screen, and
// without this the DEGREE-folding and RECT/POLAR checklists stayed hand-only
// while everything around them became scriptable.
//
// Unlike every other command this one **pushes a history entry** — the new
// mode string. A setter with no feedback is unusable over serial (commands
// report only "-> command"), and echoing it on screen is what a user would
// want from a mode switch anyway.
bool HomeScreen::handle_mode_command(const char* arg) {
    bool changed = true;
    if (arg[0] == 0) {
        changed = false;  // Bare `mode` reports without setting
    } else if (std::strcmp(arg, "rad") == 0) {
        math::set_angle_mode(math::AngleMode::kRadians);
    } else if (std::strcmp(arg, "deg") == 0) {
        math::set_angle_mode(math::AngleMode::kDegrees);
    } else if (std::strcmp(arg, "real") == 0) {
        math::set_number_mode(math::NumberMode::kReal);
    } else if (std::strcmp(arg, "rect") == 0) {
        math::set_number_mode(math::NumberMode::kRectangular);
    } else if (std::strcmp(arg, "polar") == 0) {
        math::set_number_mode(math::NumberMode::kPolar);
    } else if (std::strcmp(arg, "float") == 0) {
        math::set_display_mode(math::DisplayMode::kFloat);
    } else if (std::strcmp(arg, "sci") == 0) {
        math::set_display_mode(math::DisplayMode::kSci);
    } else if (std::strcmp(arg, "eng") == 0) {
        math::set_display_mode(math::DisplayMode::kEng);
    } else if (std::strncmp(arg, "fix", 3) == 0 && arg[3] >= '0' && arg[3] <= '9' && arg[4] == 0) {
        math::set_display_mode(math::DisplayMode::kFix);
        math::set_fix_digits(arg[3] - '0');
    } else {
        push_entry(kModeCommandName, "Unknown mode", ResultKind::kError);
        return true;
    }

    if (changed) {
        // Mirror into graph state and persist, exactly as ModeScreen::adjust
        // does — these live in two places and a set that skips the mirror
        // reverts on the next MODE-screen visit or reboot.
        graph::state().angle = math::angle_mode();
        graph::state().number = math::number_mode();
        graph::state().display = math::display_mode();
        graph::state().fix_digits = math::fix_digits();
        save_graph_state();
    }

    char modes[32];
    format_modes(modes, sizeof(modes));
    push_entry(kModeCommandName, modes, ResultKind::kPlain);
    return true;
}

// Typed commands (2026-07-18): lowercase-only (input is
// case-sensitive), matched against the trimmed line before math
// evaluation. Commands don't enter history — `mode` is the one
// exception, see handle_mode_command.
bool HomeScreen::handle_command(const char* cmd) {
    // `mode` and `mode <keyword>` — prefix match, since this is the only
    // command that takes an argument.
    if (std::strncmp(cmd, kModeCommandName, 4) == 0 && (cmd[4] == 0 || cmd[4] == ' ')) {
        const char* arg = cmd[4] == 0 ? cmd + 4 : cmd + 5;
        while (*arg == ' ') {
            ++arg;
        }
        return handle_mode_command(arg);
    }
    if (std::strcmp(cmd, "cls") == 0) {
        // Session-level clear: hide the current scrollback, keep the
        // recall walk and history.txt intact.
        cls_mark_ = entries_total_;
        scroll_ = 0;
        result_full_[0] = 0;
        result_scroll_ = 0;
        return true;
    }
    if (std::strcmp(cmd, "clrhist") == 0) {
        history_count_ = 0;
        history_head_ = 0;
        entries_total_ = 0;
        cls_mark_ = 0;
        scroll_ = 0;
        hist_nav_ = -1;
        result_full_[0] = 0;
        result_scroll_ = 0;
        auto& fs = platform::storage();
        if (fs.mounted()) {
            fs.delete_file(kHistoryPath);
        }
        return true;
    }
    // Short aliases (D24): "?" for help, singular "list"/"stat" for the
    // frequently-typed screens. No engine collisions — none parse as
    // expressions.
    if (std::strcmp(cmd, "help") == 0 || std::strcmp(cmd, "?") == 0) {
        ui::screen_manager().push(&help_screen());
        return true;
    }
    if (std::strcmp(cmd, "files") == 0) {
        ui::screen_manager().push(&files_screen());
        return true;
    }
    if (std::strcmp(cmd, "lists") == 0 || std::strcmp(cmd, "list") == 0) {
        ui::screen_manager().push(&list_editor());
        return true;
    }
    if (std::strcmp(cmd, "stats") == 0 || std::strcmp(cmd, "stat") == 0) {
        ui::screen_manager().push(&stats_screen());
        return true;
    }
    if (std::strcmp(cmd, "dist") == 0) {
        ui::screen_manager().push(&dist_screen());
        return true;
    }
    if (std::strcmp(cmd, "test") == 0 || std::strcmp(cmd, "infer") == 0) {
        ui::screen_manager().push(&infer_screen());
        return true;
    }
    if (std::strcmp(cmd, "plot") == 0 || std::strcmp(cmd, "plots") == 0) {
        ui::screen_manager().push(&plot_screen());
        return true;
    }
    if (std::strcmp(cmd, "matrix") == 0 || std::strcmp(cmd, "mat") == 0) {
        ui::screen_manager().push(&matrix_editor());
        return true;
    }
    // Bare `solve` opens the form screen; `solve(...)` with arguments
    // stays an expression (inline solve, handled in evaluate_input).
    if (std::strcmp(cmd, "solve") == 0 || std::strcmp(cmd, "solver") == 0) {
        ui::screen_manager().push(&solver_screen());
        return true;
    }
    // Graph analysis (4B): jump to the graph with the CALC menu open.
    if (std::strcmp(cmd, "calc") == 0 || std::strcmp(cmd, "analyze") == 0) {
        ui::screen_manager().push(&graph_screen());
        ui::screen_manager().push(&calc_menu());
        return true;
    }
    // Scientific-constants picker (4D.17).
    if (std::strcmp(cmd, "const") == 0 || std::strcmp(cmd, "constants") == 0) {
        ui::screen_manager().push(&const_screen());
        return true;
    }
    // CAS operations menu (Phase 5); also on the F6 softkey.
    if (std::strcmp(cmd, "cas") == 0) {
        ui::screen_manager().push(&cas_menu());
        return true;
    }
    // Device settings: brightness/backlight/auto-power-down (4D.19-20).
    if (std::strcmp(cmd, "settings") == 0 || std::strcmp(cmd, "setup") == 0) {
        ui::screen_manager().push(&settings_screen());
        return true;
    }
    if (std::strcmp(cmd, "diag") == 0 && diag_screen_ != nullptr) {
        ui::screen_manager().push(diag_screen_);
        return true;
    }
    return false;
}

void HomeScreen::insert_text(const char* s) {
    char buf[ui::InputLine::kCapacity];
    std::snprintf(buf, sizeof(buf), "%s%s", input_.text(), s);
    input_.set_text(buf);
    invalidate_input();
}

bool HomeScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kEnter:
            // Alt+Enter = "show me the decimal" (Phase 5 Stage 4). Alt, not
            // Shift: the STM32 translates Shift+Enter into its own scan code
            // (0xD1 -> kInsert) rather than reporting Enter with shift_held,
            // so a Shift binding here would never fire and would also squat
            // on a real key. Alt passes through with its flag intact, the
            // same way Alt+UP/DOWN already scrolls the history view.
            //
            // With an expression entered it evaluates with the exact-form
            // probe suppressed, exactly as a trailing `>dec` would. With the
            // input empty it re-runs the newest history entry that came back
            // as an exact form, so an amber sqrt(2) becomes 1.414213562
            // without retyping it. Commands (cls, help, ...) are unaffected:
            // they only match on the plain-Enter path below.
            if (ev.alt_held) {
                if (input_.empty()) {
                    const Entry* last = entry_from_newest(0);
                    if (last == nullptr || last->kind != ResultKind::kSymbolic) {
                        return true;
                    }
                    input_.set_text(last->expr);
                }
                evaluate_input(true);
                invalidate(0, kSoftkeyY);
                return true;
            }
            if (!input_.empty()) {
                submit_input();
            }
            return true;
        case Key::kUp:
            // Modifier+UP scrolls the history view; plain UP walks back
            // through past inputs shell-style (supersedes task 5.5's
            // single-recall). Alt/Ctrl, because the STM32 swallows
            // Shift on arrow keys (D12, HW-verified 2026-07-11); shift
            // kept in case a future keyboard firmware reports it.
            if (ev.alt_held || ev.ctrl_held || ev.shift_held) {
                // View scroll stops at the cls watermark; the recall
                // walk below intentionally does not.
                if (scroll_ < visible_count() - 1) {
                    ++scroll_;
                    invalidate_history();
                }
            } else if (hist_nav_ < history_count_ - 1) {
                if (hist_nav_ < 0) {
                    // Stash whatever was being typed for DOWN to restore.
                    std::strncpy(pending_, input_.text(), sizeof(pending_) - 1);
                    pending_[sizeof(pending_) - 1] = 0;
                }
                ++hist_nav_;
                input_.set_text(entry_from_newest(hist_nav_)->expr);
                invalidate_input();
            }
            return true;
        case Key::kDown:
            if (ev.alt_held || ev.ctrl_held || ev.shift_held) {
                if (scroll_ > 0) {
                    --scroll_;
                    invalidate_history();
                }
            } else if (hist_nav_ > 0) {
                --hist_nav_;
                input_.set_text(entry_from_newest(hist_nav_)->expr);
                invalidate_input();
            } else if (hist_nav_ == 0) {
                hist_nav_ = -1;
                input_.set_text(pending_);
                invalidate_input();
            }
            return true;
        case Key::kLeft:
        case Key::kRight:
            // With the input empty and the view pinned to newest, arrows
            // pan the (possibly truncated) newest result instead of moving
            // a cursor: RIGHT reveals later elements, LEFT walks back
            // toward the start (testdrive 2026-07-20). Otherwise they fall
            // through to the input line's cursor movement.
            if (input_.empty() && scroll_ == 0) {
                const int maxs = result_max_scroll();
                if (maxs > 0) {
                    if (ev.key == Key::kRight && result_scroll_ < maxs) {
                        ++result_scroll_;
                        invalidate_history();
                    } else if (ev.key == Key::kLeft && result_scroll_ > 0) {
                        --result_scroll_;
                        invalidate_history();
                    }
                    return true;
                }
            }
            if (input_.on_key(ev)) {
                invalidate_input();
                return true;
            }
            return false;
        case Key::kEscape:
            input_.clear();
            hist_nav_ = -1;
            invalidate_input();
            return true;
        // Global F-key scheme (2026-07-18 remap, TI-84-shaped):
        // F1 editor, F2 window, F3 mode, F4 trace, F5 graph.
        case Key::kF1:
            push_mode_editor();
            return true;
        case Key::kF2:
            ui::screen_manager().push(&window_screen());
            return true;
        case Key::kF3:
            ui::screen_manager().push(&mode_screen());
            return true;
        case Key::kF4:
            goto_graph_trace();
            return true;
        case Key::kF5:
            ui::screen_manager().push(&graph_screen());
            return true;
        case Key::kF6:  // CAS menu (Phase 5; F6 = Shift+F1 on the unit)
            ui::screen_manager().push(&cas_menu());
            return true;
        default:
            if (input_.on_key(ev)) {
                invalidate_input();
                return true;
            }
            return false;
    }
}

void HomeScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();
    const int lh = font.height() + 2;

    fb.clear(kBlack);

    ui::draw_status_bar(fb, "HOME");

    // History: newest at the bottom. Expressions render as 2D typeset
    // math (task 3.6); numeric/error results stay as plain text, while CAS
    // symbolic results are typeset in the accent color (Phase 5, 4D.21).
    const render::Metrics metrics{font.width(), font.height()};
    const int visible = visible_count();  // cls hides older entries
    int y = kInputY - 4;
    for (int n = scroll_; y > kStatusH + lh && n < visible; ++n) {
        const Entry* e = entry_from_newest(n);
        if (e == nullptr) {
            break;
        }
        const bool symbolic = e->kind == ResultKind::kSymbolic;
        const platform::Color rcolor = e->kind == ResultKind::kError ? kRed
                                       : symbolic                    ? kSymbolic
                                                                     : kWhite;

        // Expression layout first: the entry's full height must be known
        // *before* drawing, or a tall pretty-printed entry ends up rendered
        // across the status bar (HW 2026-07-18).
        render::LayoutNode const* root = render::build_layout(e->expr, metrics);
        const int eh = root != nullptr ? root->height : lh;

        // A long newest result (plain or symbolic) is shown as a one-line
        // pannable window with ellipses (LEFT/RIGHT scroll) rather than a
        // clipped/overflowing typeset form. Otherwise a symbolic result is
        // typeset 2D; plain results are single text lines.
        const bool pan = n == 0 && result_max_scroll() > 0;

        // Result height. A typeset symbolic result has its own height; the pan
        // window and plain text are one line. Building the result layout resets
        // the shared pool, so the expression tree is rebuilt before rendering.
        int rh = lh;
        if (symbolic && !pan) {
            const char* rtext = n == 0 ? result_full_ : e->result;
            render::LayoutNode const* rroot = render::build_layout(rtext, metrics);
            rh = rroot != nullptr ? rroot->height : lh;
            root = nullptr;  // invalidated by the reset in the build above
        }
        if (y - rh - (eh + 2) < kStatusH) {
            break;
        }

        // Result line at the bottom of the entry block.
        y -= rh;
        if (pan) {
            draw_result_window(fb, y, font, rcolor);
        } else if (symbolic) {
            const char* rtext = n == 0 ? result_full_ : e->result;
            render::LayoutNode const* rr = render::build_layout(rtext, metrics);
            const int rw = rr != nullptr ? rr->width : 0;
            // Right-align like numeric results so a symbolic answer reads as a
            // result, not another input line; left-anchor if it's too wide.
            const int rx = std::max(platform::kScreenW - rw - 4, 4);
            render::render_node(rr, fb, rx, y, font, rcolor);
        } else {
            const int rx = platform::kScreenW - font.text_width(e->result) - 4;
            font.draw_string(fb, rx, y, e->result, rcolor);
        }

        // Expression line(s) above the result. A symbolic entry's result
        // build reset the pool, so rebuild; otherwise `root` is still valid.
        y -= eh + 2;
        if (root == nullptr) {
            root = render::build_layout(e->expr, metrics);
        }
        render::render_node(root, fb, 4, y, font, kGrayLine);
        y -= 2;
    }

    // Input area
    fb.draw_hline(0, kInputY, platform::kScreenW, kGrayLine);
    font.draw_char(fb, 2, kInputY + 8, '>', kGreen);
    input_.render(fb, 2 + font.width() + 2, kInputY + 8, platform::kScreenW - font.width() - 8,
                  font, true);

    // Help discoverability (typed-command era): dim hint on the empty
    // input line pointing at `help`. kGridLine, not kGrayLine — the
    // latter reads near-white on the panel and was jarring here (HW
    // 2026-07-18).
    if (input_.empty()) {
        const char* hint = "type help for docs";
        font.draw_string(fb, platform::kScreenW - font.text_width(hint) - 4, kInputY + 8, hint,
                         kGridLine);
    }

    const char* f1 = "Y=";
    switch (graph::state().mode) {
        case graph::Mode::kParametric:
            f1 = "PAR";
            break;
        case graph::Mode::kPolar:
            f1 = "R=";
            break;
        default:
            break;
    }
    const char* const keys[6] = {f1, "WIN", "MODE", "TRC", "GRPH", "CAS"};
    ui::draw_softkeys(fb, keys);
}

HomeScreen& home_screen() {
    static HomeScreen instance;
    return instance;
}

}  // namespace apps
