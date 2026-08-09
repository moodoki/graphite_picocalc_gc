// Differential harness for the Phase 5.2 unified evaluator (task 5.2.9).
//
// Runs every corpus expression through BOTH pipelines from the same starting
// state and asserts they agree — on the result AND on everything they wrote.
// phase5.2-spec.md §6.1: "run every expression they already contain through
// *both* evaluators and assert identical results. That turns 1,200 hand-written
// checks into a conformance harness for free, and it fails loudly at exactly
// the inputs that matter instead of wherever someone thought to look."
//
// The corpus is harvested from the suites that pin the three retired evaluators
// (scripts/gen-differential-corpus.py). A differential check needs no expected
// value, only agreement, so a bare expression string is the whole input — which
// is why the harvest works at all, and why it picks up the error cases and
// stores a hand-written corpus would under-represent.
//
// **The allow-list is docs/notes/unified-evaluator-changes.md.** Every expected
// divergence carries its register id. The check runs both ways:
//
//   * a divergence with no entry FAILS — that is the whole point;
//   * an entry that does NOT diverge fails too, as a stale row. A register that
//     accumulates rows and never drops them is a register nobody can trust.
//
// What this harness does not compare: display strings. Formatting belongs to
// the dispatcher and lands with 5.2.10; the on-device replay in §9 compares the
// rendered `inject:` echoes. Here the comparison is values and committed state,
// which is what an evaluator is responsible for.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/array.hpp"
#include "math/complex_expr.hpp"
#include "math/engine.hpp"
#include "math/list_expr.hpp"
#include "math/lists.hpp"
#include "math/mat_expr.hpp"
#include "math/matrix.hpp"
#include "math/named_lists.hpp"
#include "math/types.hpp"
#include "math/format.hpp"
#include "math/frac.hpp"
#include "math/unified_eval.hpp"
#include "math/unified_home.hpp"

namespace {

using namespace math::unified;

#include "differential_corpus.inc"

constexpr int kCorpusCount = static_cast<int>(sizeof(kCorpus) / sizeof(kCorpus[0]));

int g_failures = 0;
int g_checks = 0;

// ---- the world -----------------------------------------------------------
//
// Snapshot/restore is this task's stated acceptance criterion, and the corpus
// is why: it contains stores (`5->l1`, `2->A`, `{1,2,3}+1->l3`). Without a
// restore between the two runs, the second pipeline would evaluate against
// whatever the first one wrote and every store in the corpus would report a
// false divergence.
//
// Captured by value rather than by copying Arrays: the seeded world is small by
// construction, so a fixed element buffer is both the snapshot and the
// comparison key, and it keeps the harness off the ArrayStore's 28 slabs.

constexpr int kSnapElems = 64;
constexpr int kMatSlots = 11;  // [A]-[J] plus MatAns
constexpr int kListRefs = math::kNamedRefBase + math::NamedLists::kMax;

struct ArraySnap {
    bool present = false;
    bool complex = false;
    bool overflow = false;  // > kSnapElems: the world outgrew the harness
    int rows = 0;
    int cols = 0;
    int size = 0;
    math::Complex data[kSnapElems];
};

struct World {
    math::calc_t re[math::Variables::kCount] = {};
    math::calc_t im[math::Variables::kCount] = {};
    bool cx[math::Variables::kCount] = {};
    ArraySnap lists[kListRefs];
    bool named_used[math::NamedLists::kMax] = {};
    char named_name[math::NamedLists::kMax][math::NamedLists::kMaxName + 1] = {};
    ArraySnap mats[kMatSlots];
};

World g_before;
World g_after_legacy;
World g_after_unified;

void snap_array(const math::Array& a, ArraySnap* s, bool matrix) {
    s->present = a.size() > 0;
    s->complex = a.dtype() == math::Dtype::kComplex;
    s->size = a.size();
    s->rows = matrix ? a.dim(0) : a.size();
    s->cols = matrix ? a.dim(1) : 1;
    s->overflow = a.size() > kSnapElems;
    const int n = s->overflow ? kSnapElems : a.size();
    for (int i = 0; i < n; ++i) {
        if (matrix) {
            const int r = s->cols > 0 ? i / s->cols : 0;
            const int c = s->cols > 0 ? i % s->cols : 0;
            s->data[i] = s->complex ? a.cget(r, c) : math::Complex(a.get(r, c), 0);
        } else {
            s->data[i] = s->complex ? a.cget(i) : math::Complex(a.get(i), 0);
        }
    }
}

void capture(World* w) {
    const auto& vars = math::engine().vars();
    for (int i = 0; i < math::Variables::kCount; ++i) {
        w->re[i] = vars.vars[i];
        w->im[i] = vars.imag[i];
        w->cx[i] = vars.is_complex(i);
    }
    for (int r = 0; r < kListRefs; ++r) {
        const bool live =
            r < math::kNamedRefBase || math::named_lists().used(r - math::kNamedRefBase);
        if (live) {
            snap_array(math::list_by_ref(r), &w->lists[r], false);
        } else {
            w->lists[r] = ArraySnap{};
        }
    }
    for (int i = 0; i < math::NamedLists::kMax; ++i) {
        w->named_used[i] = math::named_lists().used(i);
        std::snprintf(w->named_name[i], sizeof(w->named_name[i]), "%s", math::named_lists().name(i));
    }
    for (int i = 0; i < kMatSlots; ++i) {
        const math::Array& m =
            i == 10 ? math::matexpr::mat_ans() : math::matrices().matrix(i);
        snap_array(m, &w->mats[i], true);
    }
}

void restore_array(const ArraySnap& s, math::Array& a, bool matrix) {
    a.clear();
    if (!s.present) {
        return;
    }
    a.set_dtype(s.complex ? math::Dtype::kComplex : math::Dtype::kDouble);
    if (matrix) {
        a.resize(s.rows, s.cols);
    } else {
        a.resize(s.size);
    }
    const int n = s.overflow ? kSnapElems : s.size;
    for (int i = 0; i < n; ++i) {
        if (matrix) {
            const int r = s.cols > 0 ? i / s.cols : 0;
            const int c = s.cols > 0 ? i % s.cols : 0;
            if (s.complex) {
                a.cset(r, c, s.data[i]);
            } else {
                a.set(r, c, s.data[i].re);
            }
        } else if (s.complex) {
            a.cset(i, s.data[i]);
        } else {
            a.set(i, s.data[i].re);
        }
    }
}

// Named-list slots are restored by clearing the registry and re-creating in
// ascending order, which reproduces the snapshot's indices only if its used
// slots start at 0 and are contiguous. The seeded world satisfies that; the
// assertion below is what keeps it true if the seed ever changes.
void restore(const World& w) {
    auto& vars = math::engine().vars();
    for (int i = 0; i < math::Variables::kCount; ++i) {
        if (w.cx[i]) {
            vars.set_complex(i, w.re[i], w.im[i]);
        } else {
            vars.set_real(i, w.re[i]);
        }
    }
    for (int i = 0; i < math::NamedLists::kMax; ++i) {
        if (math::named_lists().used(i)) {
            math::named_lists().remove(i);
        }
    }
    for (int i = 0; i < math::NamedLists::kMax; ++i) {
        if (!w.named_used[i]) {
            continue;
        }
        const int made = math::named_lists().create(w.named_name[i]);
        if (made != i) {
            std::printf("FAIL: named-list restore landed at %d, expected %d — the seeded "
                        "world must use contiguous slots from 0\n",
                        made, i);
            ++g_failures;
        }
    }
    for (int r = 0; r < kListRefs; ++r) {
        const bool live =
            r < math::kNamedRefBase || math::named_lists().used(r - math::kNamedRefBase);
        if (live) {
            restore_array(w.lists[r], math::list_by_ref(r), false);
        }
    }
    for (int i = 0; i < kMatSlots; ++i) {
        math::Array& m =
            i == 10 ? math::matexpr::mat_ans_mutable() : math::matrices().matrix(i);
        restore_array(w.mats[i], m, true);
    }
}

bool near(math::calc_t a, math::calc_t b) {
    if (std::isnan(a) && std::isnan(b)) {
        return true;  // NaN is a value both pipelines can legitimately produce
    }
    if (std::isinf(a) || std::isinf(b)) {
        return a == b;
    }
    const math::calc_t scale = std::fmax(1.0, std::fmax(std::fabs(a), std::fabs(b)));
    return std::fabs(a - b) <= 1e-9 * scale;
}

bool same_array(const ArraySnap& a, const ArraySnap& b) {
    if (a.present != b.present || a.complex != b.complex || a.size != b.size ||
        a.rows != b.rows || a.cols != b.cols) {
        return false;
    }
    const int n = a.overflow ? kSnapElems : a.size;
    for (int i = 0; i < n; ++i) {
        if (!near(a.data[i].re, b.data[i].re) || !near(a.data[i].im, b.data[i].im)) {
            return false;
        }
    }
    return true;
}

// Returns nullptr when the worlds match, else a static description of the first
// difference — which is what makes a failure actionable rather than a boolean.
const char* world_diff(const World& a, const World& b) {
    static char buf[128];
    for (int i = 0; i < math::Variables::kCount; ++i) {
        if (a.cx[i] != b.cx[i] || !near(a.re[i], b.re[i]) || !near(a.im[i], b.im[i])) {
            const char* name = i < 26 ? "a-z" : (i == math::Variables::kTheta ? "theta" : "ans");
            std::snprintf(buf, sizeof(buf), "var %s[%d]: (%g,%g) vs (%g,%g)", name, i, a.re[i],
                          a.im[i], b.re[i], b.im[i]);
            return buf;
        }
    }
    for (int r = 0; r < kListRefs; ++r) {
        if (!same_array(a.lists[r], b.lists[r])) {
            std::snprintf(buf, sizeof(buf), "list ref %d: size %d vs %d", r, a.lists[r].size,
                          b.lists[r].size);
            return buf;
        }
    }
    for (int i = 0; i < math::NamedLists::kMax; ++i) {
        if (a.named_used[i] != b.named_used[i] ||
            std::strcmp(a.named_name[i], b.named_name[i]) != 0) {
            std::snprintf(buf, sizeof(buf), "named slot %d: \"%s\" vs \"%s\"", i, a.named_name[i],
                          b.named_name[i]);
            return buf;
        }
    }
    for (int i = 0; i < kMatSlots; ++i) {
        if (!same_array(a.mats[i], b.mats[i])) {
            std::snprintf(buf, sizeof(buf), "matrix %d: %dx%d vs %dx%d", i, a.mats[i].rows,
                          a.mats[i].cols, b.mats[i].rows, b.mats[i].cols);
            return buf;
        }
    }
    return nullptr;
}

// ---- outcomes ------------------------------------------------------------

enum class OKind : uint8_t { kError, kReal, kComplex, kList, kMatrix, kText };

struct Outcome {
    OKind kind = OKind::kError;
    char msg[96] = {};  // error text, or the kText display payload
    math::Complex scalar;
    ArraySnap arr;
    // What the screen would print, minus the store glyph — added in 5.2.10,
    // when the formatting moved into a translation unit the host can link.
    // Until then this harness could compare values but not what a user sees,
    // and "the value is right" is not the same claim as "the line is right".
    char disp[192] = {};
    char store[8] = {};
};

const char* kind_name(OKind k) {
    switch (k) {
        case OKind::kError: return "error";
        case OKind::kReal: return "real";
        case OKind::kComplex: return "complex";
        case OKind::kList: return "list";
        case OKind::kMatrix: return "matrix";
        default: return "text";
    }
}

void set_error(Outcome* o, const char* msg) {
    o->kind = OKind::kError;
    std::snprintf(o->msg, sizeof(o->msg), "%s", msg != nullptr ? msg : "(null)");
}

void set_scalar(Outcome* o, const math::Complex& z) {
    o->kind = z.im == 0.0 ? OKind::kReal : OKind::kComplex;
    o->scalar = z;
}

// The old pipeline's display rules, replicated from
// HomeScreen::evaluate_input's four rendering branches (home_screen.cpp:456-556
// as it stood before 5.2.10). Only the glue is replicated — format_number,
// format_complex, format_list and format_matrix are the real ones, called
// directly, so this compares formatting rather than reimplementing it.
void legacy_scalar_disp(Outcome* o, const math::Complex& z, bool complex_form) {
    if (complex_form) {
        math::format_complex(z, math::number_mode(), o->disp, sizeof(o->disp));
    } else {
        math::format_number(z.re, o->disp, sizeof(o->disp));
    }
}

void var_label(char* buf, size_t cap, int slot) {
    if (slot < 0) {
        buf[0] = 0;
    } else if (slot == math::Variables::kTheta) {
        std::snprintf(buf, cap, "theta");
    } else {
        std::snprintf(buf, cap, "%c", static_cast<char>('a' + slot));
    }
}

// ---- the old pipeline ----------------------------------------------------
//
// A faithful replica of HomeScreen::evaluate_input's dispatch
// (home_screen.cpp:454-682) with display and persistence removed: matrix
// expressions first, then lists, then the scalar path — and, critically, the
// Ans/store commits the DISPATCHER does for complexexpr, which never writes
// engine state itself. Getting that wrong would make the differential compare
// against a strawman, so the order and the commits are copied rather than
// reconstructed.
//
// Not replicated, because none of it is evaluation: the CAS probe (display
// only, never commits), `>frac`/`>dec` suffix stripping, and history.
void run_legacy(const char* expr, Outcome* out) {
    const auto mres = math::matexpr::evaluate(expr);
    if (mres.kind != math::matexpr::Kind::kNone) {
        switch (mres.kind) {
            case math::matexpr::Kind::kError:
                set_error(out, mres.error);
                return;
            case math::matexpr::Kind::kScalar:
                if (!mres.scalar.ok) {
                    set_error(out, mres.scalar.error);
                    return;
                }
                if (mres.scalar_complex) {
                    set_scalar(out, mres.cvalue);
                    legacy_scalar_disp(out, mres.cvalue, true);
                } else {
                    set_scalar(out, math::Complex(mres.scalar.value, 0));
                    legacy_scalar_disp(out, math::Complex(mres.scalar.value, 0), false);
                }
                var_label(out->store, sizeof(out->store), mres.scalar.stored_var);
                return;
            case math::matexpr::Kind::kList:
                out->kind = OKind::kList;
                snap_array(*mres.list, &out->arr, false);
                math::listexpr::format_list(*mres.list, out->disp, sizeof(out->disp));
                if (mres.stored_list >= 0) {
                    std::snprintf(out->store, sizeof(out->store), "l%c",
                                  static_cast<char>('1' + mres.stored_list));
                }
                return;
            case math::matexpr::Kind::kText:
                out->kind = OKind::kText;
                std::snprintf(out->msg, sizeof(out->msg), "%s", mres.text);
                std::snprintf(out->disp, sizeof(out->disp), "%s", mres.text);
                return;
            default:
                out->kind = OKind::kMatrix;
                snap_array(*mres.matrix, &out->arr, true);
                math::matexpr::format_matrix(*mres.matrix, out->disp, sizeof(out->disp));
                if (mres.stored_matrix >= 0) {
                    std::snprintf(out->store, sizeof(out->store), "[%c]",
                                  static_cast<char>('A' + mres.stored_matrix));
                }
                return;
        }
    }

    const auto lres = math::listexpr::evaluate(expr);
    if (lres.kind != math::listexpr::Kind::kNone) {
        switch (lres.kind) {
            case math::listexpr::Kind::kError:
                set_error(out, lres.error);
                return;
            case math::listexpr::Kind::kScalar:
                if (lres.scalar_complex) {
                    // The dispatcher commits Ans here (home_screen.cpp:541).
                    math::engine().vars().set_complex(math::Variables::kAns, lres.cvalue.re,
                                                      lres.cvalue.im);
                    set_scalar(out, lres.cvalue);
                    legacy_scalar_disp(out, lres.cvalue, true);
                } else if (!lres.scalar.ok) {
                    set_error(out, lres.scalar.error);
                } else {
                    set_scalar(out, math::Complex(lres.scalar.value, 0));
                    legacy_scalar_disp(out, math::Complex(lres.scalar.value, 0), false);
                    var_label(out->store, sizeof(out->store), lres.scalar.stored_var);
                }
                return;
            default:
                out->kind = OKind::kList;
                snap_array(*lres.list, &out->arr, false);
                math::listexpr::format_list(*lres.list, out->disp, sizeof(out->disp));
                if (lres.stored_list >= 0) {
                    math::list_ref_name(lres.stored_list, out->store,
                                        static_cast<int>(sizeof(out->store)));
                }
                return;
        }
    }

    auto& vars = math::engine().vars();
    const bool force_complex = math::number_mode() != math::NumberMode::kReal ||
                               math::complexexpr::mentions_i(expr) ||
                               math::refs_complex_var(expr);
    if (force_complex) {
        const auto cres = math::complexexpr::evaluate(expr);
        if (!cres.ok) {
            set_error(out, cres.error);
            return;
        }
        if (math::number_mode() == math::NumberMode::kReal && !cres.value.is_real()) {
            set_error(out, "Non-real result");
            return;
        }
        if (cres.value.is_real()) {
            vars.set_real(math::Variables::kAns, cres.value.re);
            if (cres.stored_var >= 0) {
                vars.set_real(cres.stored_var, cres.value.re);
            }
        } else {
            vars.set_complex(math::Variables::kAns, cres.value.re, cres.value.im);
            if (cres.stored_var >= 0) {
                vars.set_complex(cres.stored_var, cres.value.re, cres.value.im);
            }
        }
        set_scalar(out, cres.value);
        legacy_scalar_disp(out, cres.value, !cres.value.is_real());
        var_label(out->store, sizeof(out->store), cres.stored_var);
        return;
    }

    const auto probe = math::complexexpr::evaluate(expr);
    if (probe.ok && !probe.value.is_real()) {
        set_error(out, "Non-real result");
        return;
    }
    const auto res = math::engine().evaluate(expr);  // commits Ans and the store
    if (!res.ok) {
        set_error(out, res.error);
        return;
    }
    set_scalar(out, math::Complex(res.value, 0));
    legacy_scalar_disp(out, math::Complex(res.value, 0), false);
    var_label(out->store, sizeof(out->store), res.stored_var);
}

// ---- the new pipeline ----------------------------------------------------

Program g_prog;  // 2 KB; file scope for the same reason the evaluator's are

// The whole dispatcher, as the screen calls it (5.2.10) — not just run().
// Comparing evaluate_home against the old cascade is what makes the flip
// checkable: it covers the formatting and the store echo, which are the parts
// no host test could reach while they lived in home_screen.cpp.
void run_unified(const char* expr, Outcome* out) {
    // The PROBE runs first, and the order matters: evaluate_home commits, so a
    // probe taken afterwards would read the state the commit just wrote and
    // report `ans+1` as 44. That is what kProbe is for — a second look at the
    // same input from the same state, which is exactly 5.2.8's contract.
    Value v;
    const char* perr = nullptr;
    const bool probed = compile(expr, g_prog, &perr) && run(g_prog, &v, &perr, Mode::kProbe);
    // Snapshot the probe's value NOW. A list or matrix Value names an evaluator
    // temporary, and the next run() clears the whole pool — that is the
    // documented lifetime (valid until the next run), and evaluate_home below
    // is the next run.
    Outcome probe_val;
    if (probed) {
        switch (v.kind) {
            case Kind::kList:
                probe_val.kind = OKind::kList;
                snap_array(*v.a, &probe_val.arr, false);
                break;
            case Kind::kMatrix:
                probe_val.kind = OKind::kMatrix;
                snap_array(*v.a, &probe_val.arr, true);
                break;
            default:
                set_scalar(&probe_val, v.as_complex());
                break;
        }
    }

    const HomeResult r = evaluate_home(expr, /*to_frac=*/false);
    std::snprintf(out->store, sizeof(out->store), "%s", r.store_label);
    if (r.kind == HomeKind::kError) {
        set_error(out, r.error);
        return;
    }
    std::snprintf(out->disp, sizeof(out->disp), "%s", r.text);
    if (r.kind == HomeKind::kText) {
        out->kind = OKind::kText;
        std::snprintf(out->msg, sizeof(out->msg), "%s", r.text);
        return;
    }
    if (!probed) {
        set_error(out, perr);
        return;
    }
    out->kind = probe_val.kind;
    out->scalar = probe_val.scalar;
    out->arr = probe_val.arr;
}

// ---- the allow-list ------------------------------------------------------
//
// Every row is a row of docs/notes/unified-evaluator-changes.md. The `id` is
// the register id, so a reader of a failure can go straight to the rationale
// rather than rediscovering it.

struct Allow {
    const char* expr;
    const char* id;
};

const Allow kAllow[] = {
#include "differential_allow.inc"
};
constexpr int kAllowCount = static_cast<int>(sizeof(kAllow) / sizeof(kAllow[0]));
bool g_allow_hit[kAllowCount] = {};

int allow_index(const char* expr) {
    for (int i = 0; i < kAllowCount; ++i) {
        if (std::strcmp(kAllow[i].expr, expr) == 0) {
            return i;
        }
    }
    return -1;
}

// ---- the seeded world ----------------------------------------------------
//
// Small by construction (see kSnapElems) and deterministic. The corpus names
// l1-l6, [A]-[J], `costs` and a handful of variables; anything it names must
// exist here or both pipelines agree trivially on "empty", which tests nothing.

// Both number modes, because REAL alone would barely reach the complex tier:
// in REAL mode most of test_complex_expr's corpus is "Non-real result" from
// both pipelines, which agrees for the wrong reason. RECT is where complexexpr
// and the complex tier actually get compared.
//
// DEGREES throughout on purpose. That is D46's territory — the real and complex
// evaluators silently disagreed about DEGREE-mode trig for a whole phase, and
// this harness is the thing that would have caught it.
math::NumberMode g_mode = math::NumberMode::kReal;

void seed() {
    math::set_number_mode(g_mode);
    math::set_angle_mode(math::AngleMode::kDegrees);

    auto& vars = math::engine().vars();
    for (int i = 0; i < math::Variables::kCount; ++i) {
        vars.set_real(i, 0);
    }
    vars.set_real(0, 3);                          // a
    vars.set_real(23, 5);                         // x
    vars.set_real(math::Variables::kTheta, 30);   // theta
    vars.set_real(math::Variables::kAns, 42);     // ans

    const double l1v[3] = {1, 2, 3};
    const double l2v[3] = {4, 5, 6};
    const double l3v[3] = {3, 1, 2};
    for (int i = 0; i < 6; ++i) {
        math::Array& a = math::lists().list(i);
        a.clear();
        a.set_dtype(math::Dtype::kDouble);
        a.resize(3);
        const double* src = i == 0 ? l1v : (i == 1 ? l2v : l3v);
        a.write_range(0, 3, src);
    }
    for (int i = 0; i < math::NamedLists::kMax; ++i) {
        if (math::named_lists().used(i)) {
            math::named_lists().remove(i);
        }
    }
    const int costs = math::named_lists().create("costs");
    if (costs >= 0) {
        math::Array& a = math::named_lists().list(costs);
        a.clear();
        a.set_dtype(math::Dtype::kDouble);
        a.resize(3);
        const double v[3] = {10, 20, 30};
        a.write_range(0, 3, v);
    }

    const double va[4] = {1, 2, 3, 4};
    const double vb[4] = {5, 6, 7, 8};
    const double vi[9] = {2, 0, 0, 0, 3, 0, 0, 0, 4};
    for (int s = 0; s < 10; ++s) {
        math::Array& m = math::matrices().matrix(s);
        m.clear();
        m.set_dtype(math::Dtype::kDouble);
        if (s == 8) {  // [I], the 3x3 the eigenvector cases use
            m.resize(3, 3);
            for (int i = 0; i < 9; ++i) {
                m.set(i / 3, i % 3, vi[i]);
            }
            continue;
        }
        m.resize(2, 2);
        const double* src = (s % 2 == 0) ? va : vb;
        for (int i = 0; i < 4; ++i) {
            m.set(i / 2, i % 2, src[i]);
        }
    }
    math::matexpr::mat_ans_mutable().clear();
    math::Array& ans = math::matexpr::mat_ans_mutable();
    ans.set_dtype(math::Dtype::kDouble);
    ans.resize(2, 2);
    for (int i = 0; i < 4; ++i) {
        ans.set(i / 2, i % 2, va[i]);
    }
}

// ---- the run -------------------------------------------------------------

int g_diverged = 0;
int g_expected = 0;

void compare_one(const char* expr) {
    ++g_checks;
    seed();
    capture(&g_before);

    Outcome legacy;
    run_legacy(expr, &legacy);
    capture(&g_after_legacy);

    restore(g_before);

    Outcome unified;
    run_unified(expr, &unified);
    capture(&g_after_unified);

    const bool same_kind = legacy.kind == unified.kind;
    bool same_value = same_kind;
    if (same_kind) {
        switch (legacy.kind) {
            case OKind::kError:
            case OKind::kText:
                same_value = std::strcmp(legacy.msg, unified.msg) == 0;
                break;
            case OKind::kList:
            case OKind::kMatrix:
                same_value = same_array(legacy.arr, unified.arr);
                break;
            default:
                same_value = near(legacy.scalar.re, unified.scalar.re) &&
                             near(legacy.scalar.im, unified.scalar.im);
                break;
        }
    }
    const bool same_disp = std::strcmp(legacy.disp, unified.disp) == 0 &&
                           std::strcmp(legacy.store, unified.store) == 0;
    const char* state = world_diff(g_after_legacy, g_after_unified);
    const bool diverged = !same_kind || !same_value || !same_disp || state != nullptr;

    const int allow = allow_index(expr);
    if (diverged) {
        ++g_diverged;
        if (allow >= 0) {
            g_allow_hit[allow] = true;
            ++g_expected;
            return;
        }
        ++g_failures;
        std::printf("DIVERGENCE (no register row): \"%s\" [%s]\n", expr,
                    g_mode == math::NumberMode::kReal ? "REAL" : "RECT");
        std::printf("    old: %s", kind_name(legacy.kind));
        if (legacy.kind == OKind::kError || legacy.kind == OKind::kText) {
            std::printf(" \"%s\"", legacy.msg);
        } else if (legacy.kind == OKind::kReal || legacy.kind == OKind::kComplex) {
            std::printf(" (%.12g,%.12g)", legacy.scalar.re, legacy.scalar.im);
        } else {
            std::printf(" %dx%d", legacy.arr.rows, legacy.arr.cols);
        }
        std::printf("\n    new: %s", kind_name(unified.kind));
        if (unified.kind == OKind::kError || unified.kind == OKind::kText) {
            std::printf(" \"%s\"", unified.msg);
        } else if (unified.kind == OKind::kReal || unified.kind == OKind::kComplex) {
            std::printf(" (%.12g,%.12g)", unified.scalar.re, unified.scalar.im);
        } else {
            std::printf(" %dx%d", unified.arr.rows, unified.arr.cols);
        }
        std::printf("\n");
        if (!same_disp) {
            std::printf("    shown: \"%s\"%s%s vs \"%s\"%s%s\n", legacy.disp,
                        legacy.store[0] != 0 ? "=>" : "", legacy.store, unified.disp,
                        unified.store[0] != 0 ? "=>" : "", unified.store);
        }
        if (state != nullptr) {
            std::printf("    state: %s\n", state);
        }
        return;
    }
    // A row that stops diverging is only stale if it diverges in NEITHER mode,
    // so agreement here is not itself a failure — main() checks the rows that
    // were never hit at all.
    (void)allow;
}

}  // namespace

int main() {
    std::printf("test_differential: %d corpus expressions x 2 number modes, %d allow-list rows\n",
                kCorpusCount, kAllowCount);
    const math::NumberMode modes[] = {math::NumberMode::kReal, math::NumberMode::kRectangular};
    for (math::NumberMode m : modes) {
        g_mode = m;
        for (int i = 0; i < kCorpusCount; ++i) {
            compare_one(kCorpus[i]);
        }
    }
    // A row nothing hit is either stale or aimed at an expression the corpus
    // does not contain. Both mean the register and the harness have drifted
    // apart, which is the failure this whole file exists to prevent.
    for (int i = 0; i < kAllowCount; ++i) {
        if (!g_allow_hit[i]) {
            ++g_failures;
            std::printf("UNUSED register row %s: \"%s\" never diverged\n", kAllow[i].id,
                        kAllow[i].expr);
        }
    }
    std::printf("test_differential: %d checks, %d agreed, %d diverged as recorded, %d failures\n",
                g_checks, g_checks - g_diverged, g_expected, g_failures);
    return g_failures == 0 ? 0 : 1;
}
