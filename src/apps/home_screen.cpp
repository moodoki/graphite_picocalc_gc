#include "apps/home_screen.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/cas/cas_eval.hpp"
#include "math/cas/serialize.hpp"
#include "math/complex_expr.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/frac.hpp"
#include "math/list_expr.hpp"
#include "math/lists.hpp"
#include "math/mat_expr.hpp"
#include "math/matrix.hpp"
#include "math/named_lists.hpp"
#include "math/solve_expr.hpp"
#include "math/units.hpp"
#include "render/layout_builder.hpp"
#include "render/layout_render.hpp"
#include "apps/calc_menu.hpp"
#include "apps/cas_menu.hpp"
#include "apps/const_screen.hpp"
#include "apps/dist_screen.hpp"
#include "apps/files_screen.hpp"
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

// Screen layout (spec section 4.4, sized for the interim 8x12 font).
constexpr int kStatusH = 16;
constexpr int kInputY = 268;
constexpr int kSoftkeyY = 296;
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

void HomeScreen::push_entry(const char* expr, const char* result, ResultKind kind) {
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

void HomeScreen::evaluate_input() {
    if (input_.empty()) {
        return;
    }

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
            char result[128];
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
    char expr[160];
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
    {
        const size_t elen = std::strlen(expr);
        if (elen > 5 && std::strcmp(expr + elen - 5, ">frac") == 0) {
            expr[elen - 5] = 0;
            to_frac = true;
        } else if (elen > 4 && std::strcmp(expr + elen - 4, ">dec") == 0) {
            expr[elen - 4] = 0;  // Decimal is the default display
        }
    }

    // Matrix expressions (Phase 4A) get next crack — [X] tokens are
    // unambiguous. Kind::kNone means "not matrix syntax".
    const auto mres = math::matexpr::evaluate(expr);
    if (mres.kind != math::matexpr::Kind::kNone) {
        char result[128];
        bool error = false;
        if (mres.kind == math::matexpr::Kind::kError) {
            std::snprintf(result, sizeof(result), "%s", mres.error);
            error = true;
        } else if (mres.kind == math::matexpr::Kind::kScalar && mres.scalar_complex) {
            // Complex scalar from a matrix expression (4D.25: det /
            // element access); matexpr already committed Ans/store.
            char num[64];
            math::format_complex(mres.cvalue, math::number_mode(), num, sizeof(num));
            if (mres.scalar.stored_var >= 0) {
                const char name = mres.scalar.stored_var < 26
                                      ? static_cast<char>('a' + mres.scalar.stored_var)
                                      : 't';  // theta
                std::snprintf(result, sizeof(result), "%s%c%c", num, gfx::kGlyphStore, name);
            } else {
                std::snprintf(result, sizeof(result), "%s", num);
            }
        } else if (mres.kind == math::matexpr::Kind::kScalar) {
            format_scalar_result(mres.scalar, result, sizeof(result));
            error = !mres.scalar.ok;
        } else if (mres.kind == math::matexpr::Kind::kList) {
            char text[120];
            math::listexpr::format_list(*mres.list, text, sizeof(text));
            if (mres.stored_list >= 0) {
                std::snprintf(result, sizeof(result), "%s%cl%c", text, gfx::kGlyphStore,
                              static_cast<char>('1' + mres.stored_list));
            } else {
                std::snprintf(result, sizeof(result), "%s", text);
            }
        } else if (mres.kind == math::matexpr::Kind::kText) {
            std::snprintf(result, sizeof(result), "%s", mres.text);
        } else {
            char text[120];
            if (to_frac) {
                math::matexpr::format_matrix_frac(*mres.matrix, text, sizeof(text));
            } else {
                math::matexpr::format_matrix(*mres.matrix, text, sizeof(text));
            }
            if (mres.stored_matrix >= 0) {
                std::snprintf(result, sizeof(result), "%s%c[%c]", text, gfx::kGlyphStore,
                              static_cast<char>('A' + mres.stored_matrix));
            } else {
                std::snprintf(result, sizeof(result), "%s", text);
            }
        }
        push_entry(input_.text(), result, error ? ResultKind::kError : ResultKind::kPlain);
        if (!error) {
            persist_history_line(input_.text(), result, ResultKind::kPlain);
            save_variables();
            if (mres.kind == math::matexpr::Kind::kMatrix) {
                // MatAns changed — persist it so it survives a reboot
                // like the named matrices do.
                math::matexpr::save_ans(platform::storage());
            }
            if (mres.matrices_modified) {
                math::matrices().save(platform::storage(), mres.stored_matrix);
            }
            if (mres.lists_modified) {
                math::lists().save(platform::storage(), mres.stored_list);
            }
            for (int i = 0; i < 6; ++i) {  // mat2list wrote several (4D.12)
                if ((mres.lists_mask & (1U << i)) != 0) {
                    math::lists().save(platform::storage(), i);
                }
            }
        }
        input_.clear();
        hist_nav_ = -1;
        pending_[0] = 0;
        return;
    }

    // List expressions (Phase 3A) next; Kind::kNone means
    // "not list syntax" and falls through to the scalar engine.
    const auto lres = math::listexpr::evaluate(expr);
    if (lres.kind != math::listexpr::Kind::kNone) {
        char result[128];
        bool error = false;
        if (lres.kind == math::listexpr::Kind::kError) {
            std::snprintf(result, sizeof(result), "%s", lres.error);
            error = true;
        } else if (lres.kind == math::listexpr::Kind::kScalar && lres.scalar_complex) {
            // Standalone sum/mean of a complex list (4D.24): commit Ans
            // here, mirroring the complexexpr contract.
            math::engine().vars().set_complex(math::Variables::kAns, lres.cvalue.re,
                                              lres.cvalue.im);
            math::format_complex(lres.cvalue, math::number_mode(), result, sizeof(result));
        } else if (lres.kind == math::listexpr::Kind::kScalar) {
            format_scalar_result(lres.scalar, result, sizeof(result));
            error = !lres.scalar.ok;
        } else {
            char text[120];
            math::listexpr::format_list(*lres.list, text, sizeof(text));
            if (lres.stored_list >= 0) {
                char lname[8];
                math::list_ref_name(lres.stored_list, lname, sizeof(lname));
                std::snprintf(result, sizeof(result), "%s%c%s", text, gfx::kGlyphStore, lname);
            } else {
                std::snprintf(result, sizeof(result), "%s", text);
            }
        }
        push_entry(input_.text(), result, error ? ResultKind::kError : ResultKind::kPlain);
        if (!error) {
            persist_history_line(input_.text(), result, ResultKind::kPlain);
            save_variables();
            if (lres.names_modified) {  // A named list was created (4D.13)
                math::named_lists().save_index(platform::storage());
            }
            // Persist every ref this evaluation wrote (stores AND
            // in-place sorts — the latter silently skipped pre-4D.13).
            for (int r = 0; r < math::kNamedRefBase + math::NamedLists::kMax; ++r) {
                if ((lres.lists_mask & (1U << r)) == 0) {
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

    // Scalar >frac (4D.2): reached only when the expr wasn't matrix or
    // list syntax. Evaluate on the real engine and show the result as
    // p/q, falling back to decimal when no tight fraction (den <= 10000)
    // exists.
    if (to_frac) {
        char result[128];
        const auto res = math::engine().evaluate(expr);
        const bool error = !res.ok;
        if (error) {
            std::snprintf(result, sizeof(result), "%s", res.error);
        } else if (!math::frac::format_fraction(res.value, 10000, result, sizeof(result))) {
            math::format_number(res.value, result, sizeof(result));
        }
        push_entry(input_.text(), result, error ? ResultKind::kError : ResultKind::kPlain);
        if (!error) {
            persist_history_line(input_.text(), result, ResultKind::kPlain);
            save_variables();
        }
        input_.clear();
        hist_nav_ = -1;
        pending_[0] = 0;
        return;
    }

    // Scalar path (Phase 4C, D30): complex-aware whenever the number
    // mode isn't REAL or the input names `i`. In plain REAL mode, the
    // complex evaluator still runs once as a side-effect-free probe —
    // only when the real engine would otherwise show a bare NaN — so
    // sqrt(-4) etc. get "Non-real result" instead, without committing
    // Ans/a store twice (math::complexexpr::evaluate never mutates
    // engine state; only this dispatch does, exactly once).
    char result[128];
    bool error = false;
    // Complex-valued variables force the complex path too (4D.15): in
    // REAL mode that yields the pointed "Non-real result" error below;
    // in RECT/POLAR the reference resolves normally.
    const bool force_complex = math::number_mode() != math::NumberMode::kReal ||
                               math::complexexpr::mentions_i(expr) || math::refs_complex_var(expr);

    if (force_complex) {
        const auto cres = math::complexexpr::evaluate(expr);
        if (!cres.ok) {
            std::snprintf(result, sizeof(result), "%s", cres.error);
            error = true;
        } else if (math::number_mode() == math::NumberMode::kReal && !cres.value.is_real()) {
            std::snprintf(result, sizeof(result), "Non-real result");
            error = true;
        } else if (cres.value.is_real()) {
            math::engine().vars().set_real(math::Variables::kAns, cres.value.re);
            if (cres.stored_var >= 0) {
                math::engine().vars().set_real(cres.stored_var, cres.value.re);
            }
            math::EvalResult r;
            r.ok = true;
            r.value = cres.value.re;
            r.stored_var = cres.stored_var;
            format_scalar_result(r, result, sizeof(result));
        } else {
            // Complex commit (4D.15): Ans and the store target hold the
            // full value; real-only readers error on them (D37).
            math::engine().vars().set_complex(math::Variables::kAns, cres.value.re, cres.value.im);
            if (cres.stored_var >= 0) {
                math::engine().vars().set_complex(cres.stored_var, cres.value.re, cres.value.im);
            }
            char num[64];
            math::format_complex(cres.value, math::number_mode(), num, sizeof(num));
            if (cres.stored_var >= 0) {
                const char name =
                    cres.stored_var < 26 ? static_cast<char>('a' + cres.stored_var) : 't';  // theta
                std::snprintf(result, sizeof(result), "%s%c%c", num, gfx::kGlyphStore, name);
            } else {
                std::snprintf(result, sizeof(result), "%s", num);
            }
        }
    } else {
        const auto probe = math::complexexpr::evaluate(expr);
        if (probe.ok && !probe.value.is_real()) {
            std::snprintf(result, sizeof(result), "Non-real result");
            error = true;
        } else {
            const auto res = math::engine().evaluate(expr);
            format_scalar_result(res, result, sizeof(result));
            error = !res.ok;
        }
    }

    push_entry(input_.text(), result, error ? ResultKind::kError : ResultKind::kPlain);
    if (!error) {
        persist_history_line(input_.text(), result, ResultKind::kPlain);
        save_variables();
    }
    input_.clear();
    hist_nav_ = -1;
    pending_[0] = 0;
}

// Typed commands (2026-07-18): lowercase-only (input is
// case-sensitive), matched against the trimmed line before math
// evaluation. Commands don't enter history.
bool HomeScreen::handle_command(const char* cmd) {
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
            if (!input_.empty()) {
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
                        return true;
                    }
                }
                evaluate_input();
                invalidate(0, kSoftkeyY);  // History + input + status bar
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
