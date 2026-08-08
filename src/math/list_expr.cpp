#include "math/list_expr.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "math/complex_expr.hpp"
#include "math/format.hpp"
#include "math/list_ops.hpp"
#include "math/lists.hpp"
#include "math/named_lists.hpp"
#include "math/scratch.hpp"
#include "math/stats.hpp"

namespace math::listexpr {

namespace {

constexpr size_t kMaxLen = 256;
constexpr int kChunk = 256;
constexpr int kMaxLiteral = 64;  // Input line is 128 chars — plenty
constexpr int kMaxDepth = 2;     // Wrapper nesting (cumsum(sort_asc(..)))
// Hard cap on eval_list_into recursion, enforced in the function itself
// (see RecGuard). Sized against core 0's stack, not against taste: the
// worst chain is evaluate_input -> evaluate -> kMaxRec x eval_list_into
// -> a leaf evaluator, and it has to stay inside 4 KB (D47).
constexpr int kMaxRec = 3;

Array g_result;
Array g_temp[kMaxDepth];

// Vector-lift state: l1..l6 bind to g_elem slots in the compiled
// expression; chunks stream through g_lift (see below).
calc_t g_elem[ListStore::kCount];
const char* const kSlotNames[ListStore::kCount] = {"l1", "l2", "l3", "l4", "l5", "l6"};

// Lift operands (D24): brace literals and wrapper calls inside a lifted
// expression ("{1,2,3}+2", "range(1,9)*l1") are evaluated into these
// side arrays and bound like extra list slots. Slots are handed out
// monotonically per evaluate() (Ctx::ops) and released when the lift
// that extracted them finishes, so nested lifts never alias.
constexpr int kMaxOperands = 4;
Array g_op[kMaxOperands];
calc_t g_op_elem[kMaxOperands];
const char* const kOpNames[kMaxOperands] = {"lopa", "lopb", "lopc", "lopd"};

// The three chunk-lift buffers overlay the shared compute region
// (scratch.hpp): list_expr owns the whole region during an evaluate(), so
// its private 22.5 KB became shared bss. Layout, back to back:
//   [0]      g_lift[6][256]    12288 B
//   [12288]  g_op_lift[4][256]  8192 B
//   [20480]  g_outbuf[256]      2048 B
calc_t (&g_lift)[ListStore::kCount][kChunk] =
    *reinterpret_cast<calc_t (*)[ListStore::kCount][kChunk]>(scratch::compute_region());
calc_t (&g_op_lift)[kMaxOperands][kChunk] =
    *reinterpret_cast<calc_t (*)[kMaxOperands][kChunk]>(scratch::compute_region() + sizeof(g_lift));
calc_t (&g_outbuf)[kChunk] = *reinterpret_cast<calc_t (*)[kChunk]>(scratch::compute_region() +
                                                                   sizeof(g_lift) +
                                                                   sizeof(g_op_lift));
static_assert(sizeof(g_lift) + sizeof(g_op_lift) + sizeof(g_outbuf) <= scratch::kComputeBytes,
              "list_expr scratch exceeds shared compute region");

// String scratch for the evaluators that are *not* part of the
// recursive cycle, held in bss instead of on the stack (D47).
//
// Core 0 has 4 KB before it is writing into core 1's stack, and this
// file's chain — HomeScreen::evaluate_input -> evaluate ->
// substitute_reductions -> eval_list_into (recursive) -> a leaf
// evaluator — was spending most of it on kMaxLen char arrays. Each
// struct below belongs to functions that cannot be on the stack twice
// at once (nothing they call re-enters them; see the note above
// eval_literal), so one copy each is enough.
struct ReduceScratch {
    char raw[kMaxLen];
    char arg[kMaxLen];
    char rebuilt[kMaxLen];
};
ReduceScratch g_reduce;

struct SeqScratch {
    char raw[kMaxLen];
    char arg[5][kMaxLen];
};
SeqScratch g_seq;

struct LiteralScratch {
    const char* starts[kMaxLiteral];
    size_t lens[kMaxLiteral];
    calc_t values[kMaxLiteral];
    char arg[kMaxLen];
};
LiteralScratch g_literal;

struct RangeScratch {
    char raw[kMaxLen];
    char arg[kMaxLen];
};
RangeScratch g_range;

// Top-level evaluate()'s own buffers. It is the single entry point and is
// never re-entered, and it sits *above* complexexpr::evaluate on the deepest
// path into that parser — so this frame is part of what that parser's depth
// cap has to be sized around (D47).
struct EvaluateScratch {
    char body[kMaxLen];
    char tb[kMaxLen];
    char args[kMaxLen];
};
EvaluateScratch g_top;

// eval_list_into's own wrapper-argument buffers, one set per recursion
// level. Everything else in this file is called at most once at a time;
// these are the exception, so they are indexed by depth rather than
// shared — and kept out of the recursive frame, which is the one that
// multiplies (D47).
struct StepScratch {
    char arg[kMaxLen];
    char targ[kMaxLen];
    char rw[kMaxLen];   // eval_lift's rewritten expression
    char sub[kMaxLen];  // extract_operands' current span
};
StepScratch g_step[kMaxRec];

// Complex-lift term/factor text (eval_clift).
struct CliftScratch {
    char term[kMaxLen];
    char factor[kMaxLen];
};
CliftScratch g_clift;

bool ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Slot index (0-5) when p points at a boundary-delimited l1..l6 token
// inside s, else -1. `ln(x)` and `l10` don't match.
int token_at(const char* s, const char* p) {
    if (p[0] != 'l' || p[1] < '1' || p[1] > '6') {
        return -1;
    }
    if (p > s && (ident_char(p[-1]) || p[-1] == '.')) {
        return -1;
    }
    if (ident_char(p[2]) || p[2] == '.') {
        return -1;
    }
    return p[1] - '1';
}

bool contains_list_token(const char* s) {
    for (const char* p = s; *p != 0; ++p) {
        if (token_at(s, p) >= 0) {
            return true;
        }
    }
    return false;
}

// Named-list token at p (4D.13): a boundary-delimited identifier found
// in the registry, not followed by '(' (that's a function call).
// Returns the registry slot; *tok_len gets the identifier length.
int named_token_at(const char* s, const char* p, size_t* tok_len) {
    if (std::isalpha(static_cast<unsigned char>(*p)) == 0) {
        return -1;
    }
    if (p > s && (ident_char(p[-1]) || p[-1] == '.')) {
        return -1;
    }
    size_t n = 0;
    while (ident_char(p[n])) {
        ++n;
    }
    if (n < 2 || n > NamedLists::kMaxName || p[n] == '(') {
        return -1;
    }
    char name[NamedLists::kMaxName + 1];
    std::memcpy(name, p, n);
    name[n] = 0;
    const int idx = named_lists().find(name);
    if (idx >= 0 && tok_len != nullptr) {
        *tok_len = n;
    }
    return idx;
}

bool contains_named_token(const char* s) {
    for (const char* p = s; *p != 0; ++p) {
        if (named_token_at(s, p, nullptr) >= 0) {
            return true;
        }
    }
    return false;
}

// Whole-string named-list token ("prices"), else -1.
int bare_named(const char* s) {
    size_t n = 0;
    const int idx = named_token_at(s, s, &n);
    return (idx >= 0 && s[n] == 0) ? idx : -1;
}

// Trim into buf; returns false when the input overflows it.
bool trim_into(const char* s, char* buf, size_t cap) {
    while (*s == ' ') {
        ++s;
    }
    size_t len = std::strlen(s);
    while (len > 0 && s[len - 1] == ' ') {
        --len;
    }
    if (len >= cap) {
        return false;
    }
    std::memcpy(buf, s, len);
    buf[len] = 0;
    return true;
}

// True when s is exactly `name(...)` with the paren after `name`
// matched by the final char; *inner gets the argument region.
bool wrapper_form(const char* s, const char* name, const char** inner, size_t* inner_len) {
    const size_t nl = std::strlen(name);
    if (std::strncmp(s, name, nl) != 0 || s[nl] != '(') {
        return false;
    }
    const size_t len = std::strlen(s);
    if (len < nl + 2 || s[len - 1] != ')') {
        return false;
    }
    int depth = 0;
    for (size_t i = nl; i < len; ++i) {
        if (s[i] == '(') {
            ++depth;
        } else if (s[i] == ')') {
            --depth;
            if (depth == 0) {
                if (i != len - 1) {
                    return false;  // e.g. "cumsum(l1)+1"
                }
                break;
            }
        }
    }
    *inner = s + nl + 1;
    *inner_len = len - nl - 2;
    return true;
}

const char* const kWrapperNames[] = {"sort_asc",   "sort_desc", "cumsum",
                                     "delta_list", "seq",       "range"};
// The single-list-argument subset (seq and range are handled separately).
const char* const kValueWrappers[] = {"sort_asc", "sort_desc", "cumsum", "delta_list"};

// A wrapper call anywhere in s ("range(1,9)*2" — not just as the whole
// expression) makes it a list expression (D24 lift operands).
bool contains_wrapper_call(const char* s) {
    for (const char* p = s; *p != 0; ++p) {
        if (p > s && ident_char(p[-1])) {
            continue;  // Mid-identifier, not a call start
        }
        for (const char* name : kWrapperNames) {
            const size_t nl = std::strlen(name);
            if (std::strncmp(p, name, nl) == 0 && p[nl] == '(') {
                return true;
            }
        }
    }
    return false;
}

// Split `s` (length n) on top-level commas into at most max_args
// [start, len) spans. Returns the count, or -1 on overflow.
int split_args(const char* s, size_t n, const char** starts, size_t* lens, int max_args) {
    int count = 0;
    int depth = 0;
    size_t arg_start = 0;
    for (size_t i = 0; i <= n; ++i) {
        const char c = i < n ? s[i] : ',';
        if (c == '(' || c == '{') {
            ++depth;
        } else if (c == ')' || c == '}') {
            --depth;
        } else if (c == ',' && depth == 0) {
            if (count == max_args) {
                return -1;
            }
            starts[count] = s + arg_start;
            lens[count] = i - arg_start;
            ++count;
            arg_start = i + 1;
        }
    }
    return count;
}

struct Ctx {
    const char* err = nullptr;
    int depth = 0;
    int ops = 0;  // Next free lift-operand slot (monotonic, see g_op)
    int rec = 0;  // eval_list_into recursion depth, see RecGuard
};

bool eval_list_into(const char* s, Array& out, Ctx& ctx);

// Bounds eval_list_into's recursion at the function itself rather than
// at individual call sites. `depth` above only ever covered the
// cumsum/delta path; the sort path, and the lift path through
// extract_operands, both recursed uncounted — so
// sort_asc(sort_asc(sort_asc(...))) could nest as deep as the input
// allowed. At 624 B a level against core 0's 4 KB that is a stack
// overrun, not a slow evaluation (D47), and the depth has to be capped
// by construction for the budget to mean anything.
struct RecGuard {
    Ctx& ctx;
    explicit RecGuard(Ctx& c) : ctx(c) { ++ctx.rec; }
    ~RecGuard() { --ctx.rec; }
    RecGuard(const RecGuard&) = delete;
    RecGuard& operator=(const RecGuard&) = delete;
    RecGuard(RecGuard&&) = delete;
    RecGuard& operator=(RecGuard&&) = delete;
    bool too_deep() const { return ctx.rec > kMaxRec; }
};

// Reduction names: list in, scalar out. mean/median/stdev added per
// D24 (stdev = sample Sx; "std" is an alias).
enum class Reduction : uint8_t { kSum, kProd, kLength, kMean, kMedian, kStdev, kStd };
constexpr int kReductionCount = 7;
const char* const kReductionNames[kReductionCount] = {"sum",    "prod",  "length", "mean",
                                                      "median", "stdev", "std"};

// Evaluated non-bare reduction arguments land here (released after use).
Array g_red;

bool contains_reduction_call(const char* s) {
    for (const char* p = s; *p != 0; ++p) {
        if (p > s && ident_char(p[-1])) {
            continue;
        }
        for (const char* name : kReductionNames) {
            const size_t nl = std::strlen(name);
            if (std::strncmp(p, name, nl) == 0 && p[nl] == '(') {
                return true;
            }
        }
    }
    return false;
}

// Replace list reductions — sum(l1), mean(l1), ... — with numeric
// literals so the rest of the expression can be a plain scalar or
// vector-lifted engine expression. Bare list names take the fast path
// (no copy); any other argument is evaluated as a list expression
// (D24, lifting the D22 bare-arg limitation — sum(range(1,100)),
// mean(l1*2)). Nested reductions resolve innermost-first: an argument
// still containing a reduction call is skipped until a later pass.
// Complex lists (4D.24): sum/mean are well-defined componentwise but
// can't splice into the real rewrite as a %.17g literal, so they are
// supported only when the reduction call IS the whole expression —
// *cval/*creturn deliver the complex scalar to evaluate(). Everything
// else on a complex list errors (D37: never silently truncate).
bool substitute_reductions(char* buf, size_t cap, const char** err, Complex* cval, bool* creturn) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (int ni = 0; ni < kReductionCount; ++ni) {
            const char* name = kReductionNames[ni];
            const size_t nl = std::strlen(name);
            for (char* p = buf; (p = std::strstr(p, name)) != nullptr; ++p) {
                if (p > buf && ident_char(p[-1])) {
                    continue;
                }
                char* q = p + nl;
                if (*q != '(') {
                    continue;
                }
                // Matching close paren (brace depth counts too).
                const char* close = nullptr;
                int depth = 0;
                for (const char* r = q; *r != 0; ++r) {
                    if (*r == '(' || *r == '{') {
                        ++depth;
                    } else if (*r == ')' || *r == '}') {
                        --depth;
                        if (depth == 0) {
                            close = r;
                            break;
                        }
                    }
                }
                if (close == nullptr) {
                    continue;  // Unbalanced — let the engine report it
                }
                char* const raw = g_reduce.raw;
                const auto alen = static_cast<size_t>(close - (q + 1));
                if (alen >= sizeof(g_reduce.raw)) {
                    *err = "Expression too long";
                    return false;
                }
                std::memcpy(raw, q + 1, alen);
                raw[alen] = 0;
                char* const arg = g_reduce.arg;
                if (!trim_into(raw, arg, sizeof(g_reduce.arg))) {
                    *err = "Expression too long";
                    return false;
                }

                const Array* lst = nullptr;
                const int slot = std::strlen(arg) == 2 ? token_at(arg, arg) : -1;
                if (slot >= 0) {
                    lst = &lists().list(slot);
                } else {
                    if (contains_reduction_call(arg)) {
                        continue;  // Inner reduction first
                    }
                    Ctx actx;
                    if (!eval_list_into(arg, g_red, actx)) {
                        *err = actx.err != nullptr ? actx.err : "Syntax error";
                        return false;
                    }
                    lst = &g_red;
                }
                const auto red = static_cast<Reduction>(ni);
                if (lst->dtype() == Dtype::kComplex && red != Reduction::kLength) {
                    if (red == Reduction::kSum || red == Reduction::kMean) {
                        // Standalone call only (see the function comment).
                        if (p == buf && *(close + 1) == 0) {
                            Complex cs = listops::csum(*lst);
                            const int cn = lst->size();
                            g_red.clear();
                            if (red == Reduction::kMean) {
                                if (cn == 0) {
                                    *err = "Undefined result";
                                    return false;
                                }
                                cs = cs / Complex(static_cast<calc_t>(cn));
                            }
                            *cval = cs;
                            *creturn = true;
                            return true;
                        }
                        *err = "Complex sum/mean must stand alone";
                        g_red.clear();
                        return false;
                    }
                    *err = "Non-real list";
                    g_red.clear();
                    return false;
                }
                calc_t v = 0;
                switch (red) {
                    case Reduction::kSum:
                        v = listops::sum(*lst);
                        break;
                    case Reduction::kProd:
                        v = listops::prod(*lst);
                        break;
                    case Reduction::kLength:
                        v = static_cast<calc_t>(lst->size());
                        break;
                    default: {  // mean / median / stdev / std
                        const auto st = stats::one_var(*lst);
                        if (!st.ok) {
                            *err = st.error;
                            g_red.clear();
                            return false;
                        }
                        v = red == Reduction::kMean     ? st.mean
                            : red == Reduction::kMedian ? st.median
                                                        : st.sample_stddev;
                        break;
                    }
                }
                g_red.clear();
                if (std::isnan(v)) {  // e.g. stdev of a 1-element list
                    *err = "Undefined result";
                    return false;
                }
                char num[40];
                std::snprintf(num, sizeof(num), "%.17g", static_cast<double>(v));
                char* const rebuilt = g_reduce.rebuilt;
                const int wrote = std::snprintf(rebuilt, sizeof(g_reduce.rebuilt), "%.*s(%s)%s",
                                                static_cast<int>(p - buf), buf, num, close + 1);
                if (wrote < 0 || wrote >= static_cast<int>(cap) ||
                    wrote >= static_cast<int>(sizeof(g_reduce.rebuilt))) {
                    *err = "Expression too long";
                    return false;
                }
                std::memcpy(buf, rebuilt, static_cast<size_t>(wrote) + 1);
                changed = true;
                break;
            }
        }
    }
    return true;
}

// The four leaf evaluators below are deliberately kept out of line.
//
// They are called only from eval_list_into, which *recurses* (through
// eval_lift/extract_operands, and directly on the sort/cumsum paths).
// Inlined, each one's kMaxLen locals — eval_seq alone has arg[5][256] —
// became part of the recursive frame and were paid again at every level:
// eval_list_into measured 2,248 B against core 0's 4 KB total, so a
// single home-screen list expression already overran into core 1's
// stack. Out of line they are leaves, charged once at the deepest point
// instead of once per level (D47). None of them re-enters the cycle.
__attribute__((noinline)) bool eval_literal(const char* s, Array& out, Ctx& ctx) {
    const size_t len = std::strlen(s);
    auto& starts = g_literal.starts;
    auto& lens = g_literal.lens;
    out.clear();
    if (len == 2) {  // "{}"
        out.set_dtype(Dtype::kDouble);
        return out.resize(0);
    }
    const int count = split_args(s + 1, len - 2, starts, lens, kMaxLiteral);
    if (count < 0) {
        ctx.err = "List literal too long";
        return false;
    }

    // Complex literal (4D.24): any element naming `i` or a
    // complex-valued variable makes the whole literal complex. This
    // pass also length-checks every element for the loops below.
    bool complex = false;
    char* const arg = g_literal.arg;
    for (int i = 0; i < count; ++i) {
        if (lens[i] >= sizeof(g_literal.arg)) {
            ctx.err = "Expression too long";
            return false;
        }
        std::memcpy(arg, starts[i], lens[i]);
        arg[lens[i]] = 0;
        complex = complex || complexexpr::mentions_i(arg) || refs_complex_var(arg);
    }

    if (complex) {
        out.set_dtype(Dtype::kComplex);
        if (!out.resize(count)) {
            ctx.err = "Out of list memory";
            return false;
        }
        for (int i = 0; i < count; ++i) {
            std::memcpy(arg, starts[i], lens[i]);
            arg[lens[i]] = 0;
            // Inside list evaluation, several frames deep — see
            // kMaxParseDepthNested (D47).
            const auto r = complexexpr::evaluate(arg, complexexpr::kMaxParseDepthNested);
            if (!r.ok) {
                ctx.err = "Bad list element";
                return false;
            }
            out.cset(i, r.value);
        }
        return true;
    }

    out.set_dtype(Dtype::kDouble);
    auto& values = g_literal.values;
    for (int i = 0; i < count; ++i) {
        if (lens[i] >= sizeof(g_literal.arg)) {
            ctx.err = "Expression too long";
            return false;
        }
        std::memcpy(arg, starts[i], lens[i]);
        arg[lens[i]] = 0;
        if (!eval_field(arg, &values[i])) {
            ctx.err = "Bad list element";
            return false;
        }
    }
    if (!out.resize(count)) {
        ctx.err = "Out of list memory";
        return false;
    }
    out.write_range(0, count, values);
    return true;
}

__attribute__((noinline)) bool eval_seq(const char* inner, size_t inner_len, Array& out, Ctx& ctx) {
    const char* starts[5];
    size_t lens[5];
    const int count = split_args(inner, inner_len, starts, lens, 5);
    if (count != 5) {
        ctx.err = "seq needs (expr, var, lo, hi, step)";
        return false;
    }
    auto& arg = g_seq.arg;
    for (int i = 0; i < 5; ++i) {
        char* const raw = g_seq.raw;
        if (lens[i] >= sizeof(g_seq.raw)) {
            ctx.err = "Expression too long";
            return false;
        }
        std::memcpy(raw, starts[i], lens[i]);
        raw[lens[i]] = 0;
        if (!trim_into(raw, arg[i], sizeof(arg[i]))) {
            ctx.err = "Expression too long";
            return false;
        }
    }
    int var_slot = -1;
    if (std::strcmp(arg[1], "theta") == 0) {
        var_slot = Variables::kTheta;
    } else if (arg[1][0] >= 'a' && arg[1][0] <= 'z' && arg[1][1] == 0 && arg[1][0] != 'e' &&
               arg[1][0] != 'i') {
        var_slot = arg[1][0] - 'a';
    } else {
        ctx.err = "seq var must be a-z (not e/i) or theta";
        return false;
    }
    calc_t lo = 0;
    calc_t hi = 0;
    calc_t step = 0;
    if (!eval_field(arg[2], &lo) || !eval_field(arg[3], &hi) || !eval_field(arg[4], &step)) {
        ctx.err = "Bad seq argument";
        return false;
    }
    const char* err = nullptr;
    if (!listops::seq(arg[0], var_slot, lo, hi, step, out, &err)) {
        ctx.err = err;
        return false;
    }
    return true;
}

// range(lo, hi[, step]) — inclusive endpoints, default step ±1 toward
// hi (D24). Backed by listops::seq with the identity formula.
__attribute__((noinline)) bool eval_range(const char* inner, size_t inner_len, Array& out,
                                          Ctx& ctx) {
    const char* starts[3];
    size_t lens[3];
    const int count = split_args(inner, inner_len, starts, lens, 3);
    if (count != 2 && count != 3) {
        ctx.err = "range needs (lo, hi[, step])";
        return false;
    }
    calc_t vals[3] = {0, 0, 0};
    char* const raw = g_range.raw;
    char* const arg = g_range.arg;
    for (int i = 0; i < count; ++i) {
        if (lens[i] >= sizeof(g_range.raw)) {
            ctx.err = "Expression too long";
            return false;
        }
        std::memcpy(raw, starts[i], lens[i]);
        raw[lens[i]] = 0;
        if (!trim_into(raw, arg, sizeof(g_range.arg)) || !eval_field(arg, &vals[i])) {
            ctx.err = "Bad range argument";
            return false;
        }
    }
    const calc_t lo = vals[0];
    const calc_t hi = vals[1];
    const calc_t step = count == 3 ? vals[2] : (hi >= lo ? 1 : -1);
    const char* err = nullptr;
    if (!listops::seq("x", 'x' - 'a', lo, hi, step, out, &err)) {
        ctx.err = err;
        return false;
    }
    return true;
}

// Extract top-level brace literals and wrapper calls from s into
// operand slots (g_op, evaluated via eval_list_into), rewriting each
// span to its bound name in `out` ("{1,2}+range(0,4)*l1" ->
// "lopa+lopb*l1"). Slots are claimed from ctx.ops.
bool extract_operands(const char* s, char* out, size_t cap, Ctx& ctx) {
    size_t w = 0;
    const char* p = s;
    while (*p != 0) {
        const char* span_end = nullptr;  // One past the span when found
        if (*p == '{') {
            int depth = 0;
            for (const char* q = p; *q != 0; ++q) {
                if (*q == '{') {
                    ++depth;
                } else if (*q == '}') {
                    --depth;
                    if (depth == 0) {
                        span_end = q + 1;
                        break;
                    }
                }
            }
            if (span_end == nullptr) {
                ctx.err = "Syntax error";
                return false;
            }
        } else if (p == s || !ident_char(p[-1])) {
            for (const char* name : kWrapperNames) {
                const size_t nl = std::strlen(name);
                if (std::strncmp(p, name, nl) != 0 || p[nl] != '(') {
                    continue;
                }
                int depth = 0;
                for (const char* q = p + nl; *q != 0; ++q) {
                    if (*q == '(') {
                        ++depth;
                    } else if (*q == ')') {
                        --depth;
                        if (depth == 0) {
                            span_end = q + 1;
                            break;
                        }
                    }
                }
                if (span_end == nullptr) {
                    ctx.err = "Syntax error";
                    return false;
                }
                break;
            }
            // Named-list token (4D.13): extract it as an operand span
            // so the lift machinery below sees it like a brace literal.
            if (span_end == nullptr) {
                size_t nl = 0;
                if (named_token_at(s, p, &nl) >= 0) {
                    span_end = p + nl;
                }
            }
        }

        if (span_end == nullptr) {
            if (w + 1 >= cap) {
                ctx.err = "Expression too long";
                return false;
            }
            out[w++] = *p++;
            continue;
        }

        if (ctx.ops >= kMaxOperands) {
            ctx.err = "Too many list terms";
            return false;
        }
        const auto span_len = static_cast<size_t>(span_end - p);
        char* const sub = g_step[ctx.rec - 1].sub;
        if (span_len >= sizeof(g_step[ctx.rec - 1].sub)) {
            ctx.err = "Expression too long";
            return false;
        }
        std::memcpy(sub, p, span_len);
        sub[span_len] = 0;
        const int slot = ctx.ops++;
        if (!eval_list_into(sub, g_op[slot], ctx)) {
            return false;
        }
        const size_t name_len = std::strlen(kOpNames[slot]);
        if (w + name_len + 1 >= cap) {
            ctx.err = "Expression too long";
            return false;
        }
        std::memcpy(out + w, kOpNames[slot], name_len);
        w += name_len;
        p = span_end;
    }
    out[w] = 0;
    return true;
}

// ---- Complex vector lift (4D.24, D37 v1 scope) ----
//
// When any operand list is complex (or the expression names `i` / a
// complex-valued variable), the tinyexpr lift can't run. This narrow
// evaluator covers exactly the committed v1 surface — sums and
// differences of terms, each term a product of scalar factors and at
// most one list factor, with division by scalars — and errors on
// anything else. Grammar over the operand-rewritten text (lN / lopX):
//   expr   := ['-'] term (('+'|'-') term)*
//   term   := factor (('*'|'/') factor)*
//   factor := list-token | scalar-span (evaluated via complexexpr)

constexpr int kMaxCTerms = 8;

struct CTerm {
    Complex scalar{1.0, 0.0};
    const Array* list = nullptr;
    int sign = 1;
};

// The list the factor names, when the (sign-stripped) factor is exactly
// one list token; flips *sign for each leading '-'.
const Array* clift_list_factor(const char* f, int first_op, int ops, int* sign) {
    while (*f == '+' || *f == '-' || *f == ' ') {
        if (*f == '-') {
            *sign = -*sign;
        }
        ++f;
    }
    const size_t flen = std::strlen(f);
    if (flen == 2) {
        const int slot = token_at(f, f);
        if (slot >= 0) {
            return &lists().list(slot);
        }
    }
    for (int k = first_op; k < ops; ++k) {
        if (std::strcmp(f, kOpNames[k]) == 0) {
            return &g_op[k];
        }
    }
    return nullptr;
}

bool clift_has_list_token(const char* f, int first_op, int ops) {
    for (const char* p = f; *p != 0; ++p) {
        if (token_at(f, p) >= 0) {
            return true;
        }
        for (int k = first_op; k < ops; ++k) {
            const size_t nl = std::strlen(kOpNames[k]);
            if (std::strncmp(p, kOpNames[k], nl) == 0 && (p == f || !ident_char(p[-1])) &&
                !ident_char(p[nl])) {
                return true;
            }
        }
    }
    return false;
}

__attribute__((noinline)) bool eval_clift(const char* rw, int first_op, Array& out, Ctx& ctx) {
    // Deliberately NOT static, despite this being a leaf: the entries rely
    // on CTerm's default member initializers (scalar = 1, list = nullptr)
    // running per call — only .sign is assigned below. A static kept the
    // previous call's accumulated scalar and a stale Array*, which
    // test_lists caught as a segfault on `2i*l1` (D47).
    CTerm terms[kMaxCTerms];
    int nterms = 0;

    const size_t len = std::strlen(rw);
    size_t term_start = 0;
    int next_sign = 1;
    bool expecting = true;  // At an operand position (term/factor start)
    int depth = 0;
    for (size_t i = 0; i <= len; ++i) {
        const char c = i < len ? rw[i] : '+';
        if (c == '(') {
            ++depth;
            expecting = true;
            continue;
        }
        if (c == ')') {
            --depth;
            expecting = false;
            continue;
        }
        if (depth > 0) {
            continue;
        }
        const bool exponent_sign =
            (c == '+' || c == '-') && i >= 2 && (rw[i - 1] == 'e' || rw[i - 1] == 'E') &&
            (std::isdigit(static_cast<unsigned char>(rw[i - 2])) != 0 || rw[i - 2] == '.');
        if ((c == '+' || c == '-') && i < len && (expecting || exponent_sign)) {
            continue;  // Unary sign / exponent sign, part of the factor text
        }
        if (c == '+' || c == '-') {
            if (i > term_start) {
                if (nterms == kMaxCTerms) {
                    ctx.err = "Too many list terms";
                    return false;
                }
                // Parse the term [term_start, i): factors at */ depth 0.
                char* const termbuf = g_clift.term;
                const size_t tlen = i - term_start;
                if (tlen >= sizeof(g_clift.term)) {
                    ctx.err = "Expression too long";
                    return false;
                }
                std::memcpy(termbuf, rw + term_start, tlen);
                termbuf[tlen] = 0;

                CTerm& term = terms[nterms];
                term.sign = next_sign;
                size_t fstart = 0;
                char fop = '*';
                int fdepth = 0;
                bool fexpect = true;
                for (size_t j = 0; j <= tlen; ++j) {
                    const char fc = j < tlen ? termbuf[j] : '*';
                    if (fc == '(') {
                        ++fdepth;
                        fexpect = true;
                        continue;
                    }
                    if (fc == ')') {
                        --fdepth;
                        fexpect = false;
                        continue;
                    }
                    if (fdepth > 0) {
                        continue;
                    }
                    if ((fc == '*' || fc == '/') && j < tlen && fexpect) {
                        ctx.err = "Syntax error";
                        return false;
                    }
                    if (fc != '*' && fc != '/') {
                        if (fc != ' ') {
                            fexpect = false;
                        }
                        continue;
                    }
                    char* const fbuf = g_clift.factor;
                    const size_t flen2 = j - fstart;
                    if (flen2 == 0 || flen2 >= sizeof(g_clift.factor)) {
                        ctx.err = "Syntax error";
                        return false;
                    }
                    std::memcpy(fbuf, termbuf + fstart, flen2);
                    fbuf[flen2] = 0;

                    int fsign = 1;
                    const Array* lst = clift_list_factor(fbuf, first_op, ctx.ops, &fsign);
                    if (lst != nullptr) {
                        if (fop == '/') {
                            ctx.err = "Cannot divide by a list";
                            return false;
                        }
                        if (term.list != nullptr) {
                            ctx.err = "Complex lists: one list per term";
                            return false;
                        }
                        term.list = lst;
                        if (fsign < 0) {
                            term.sign = -term.sign;
                        }
                    } else {
                        if (clift_has_list_token(fbuf, first_op, ctx.ops)) {
                            ctx.err = "Complex lists support only +, -, scalar * and /";
                            return false;
                        }
                        const auto r =
                            complexexpr::evaluate(fbuf, complexexpr::kMaxParseDepthNested);
                        if (!r.ok) {
                            ctx.err = r.error;
                            return false;
                        }
                        term.scalar = fop == '/' ? term.scalar / r.value : term.scalar * r.value;
                    }
                    fop = j < tlen ? fc : '*';
                    fstart = j + 1;
                    fexpect = true;
                }
                ++nterms;
            }
            next_sign = c == '-' ? -1 : 1;
            term_start = i + 1;
            expecting = true;
            continue;
        }
        if (c != ' ') {
            expecting = false;
        }
    }
    if (depth != 0 || nterms == 0) {
        ctx.err = "Syntax error";
        return false;
    }

    int n = -1;
    for (int t = 0; t < nterms; ++t) {
        if (terms[t].list == nullptr) {
            continue;
        }
        const int sz = terms[t].list->size();
        if (n >= 0 && sz != n) {
            ctx.err = "List length mismatch";
            return false;
        }
        n = sz;
    }
    if (n < 0) {
        ctx.err = "Expected a list";
        return false;
    }

    out.clear();
    out.set_dtype(Dtype::kComplex);
    if (!out.resize(n)) {
        ctx.err = "Out of list memory";
        return false;
    }
    for (int i = 0; i < n; ++i) {
        Complex acc(0.0, 0.0);
        for (int t = 0; t < nterms; ++t) {
            Complex v = terms[t].scalar;
            if (terms[t].list != nullptr) {
                v = v * terms[t].list->cget(i);
            }
            acc = terms[t].sign < 0 ? acc - v : acc + v;
        }
        out.cset(i, acc);
    }
    return true;
}

// Element-wise engine expression over l1..l6 plus extracted operands
// (brace literals, wrapper calls) — the vector lift.
bool eval_lift(const char* s, Array& out, Ctx& ctx) {
    const int first_op = ctx.ops;
    // Depth-indexed for the same reason as eval_list_into's pair above:
    // eval_lift sits inside the recursion cycle (extract_operands calls
    // back into eval_list_into), so a kMaxLen local here is paid per
    // level (D47).
    char* const rw = g_step[ctx.rec - 1].rw;
    if (!extract_operands(s, rw, sizeof(g_step[ctx.rec - 1].rw), ctx)) {
        return false;
    }

    bool used[ListStore::kCount] = {};
    for (const char* p = rw; *p != 0; ++p) {
        const int slot = token_at(rw, p);
        if (slot >= 0) {
            used[slot] = true;
        }
    }
    int n = -1;
    bool mismatch = false;
    for (int i = 0; i < ListStore::kCount; ++i) {
        if (!used[i]) {
            continue;
        }
        const int sz = lists().list(i).size();
        if (n >= 0 && sz != n) {
            mismatch = true;
        }
        n = sz;
    }
    for (int k = first_op; k < ctx.ops; ++k) {
        const int sz = g_op[k].size();
        if (n >= 0 && sz != n) {
            mismatch = true;
        }
        n = sz;
    }

    // Complex participation (4D.24): a complex list/operand, `i`, or a
    // complex-valued scalar variable routes to the narrow complex lift
    // — the tinyexpr lift below is real-only.
    bool any_complex = false;
    for (int i = 0; i < ListStore::kCount && !any_complex; ++i) {
        any_complex = used[i] && lists().list(i).dtype() == Dtype::kComplex;
    }
    for (int k = first_op; k < ctx.ops && !any_complex; ++k) {
        any_complex = g_op[k].dtype() == Dtype::kComplex;
    }
    if (!any_complex) {
        any_complex = complexexpr::mentions_i(rw) || refs_complex_var(rw);
    }
    if (any_complex && !mismatch) {
        const bool cok = eval_clift(rw, first_op, out, ctx);
        for (int k = first_op; k < ctx.ops; ++k) {
            g_op[k].clear();
        }
        ctx.ops = first_op;
        return cok;
    }

    bool ok = false;
    if (mismatch) {
        ctx.err = "List length mismatch";
    } else if (n < 0) {
        ctx.err = "Expected a list";
    } else {
        Engine::ExtraVar extras[ListStore::kCount + kMaxOperands];
        int nx = 0;
        for (int i = 0; i < ListStore::kCount; ++i) {
            extras[nx++] = {kSlotNames[i], &g_elem[i]};
        }
        for (int k = first_op; k < ctx.ops; ++k) {
            extras[nx++] = {kOpNames[k], &g_op_elem[k]};
        }
        void* h = engine().compile_with(rw, extras, nx);
        // The scratch array may still be complex-typed from an earlier
        // evaluation; the real lift always produces a real list.
        out.clear();
        out.set_dtype(Dtype::kDouble);
        if (h == nullptr) {
            ctx.err = "Syntax error";
        } else if (!out.resize(n)) {
            ctx.err = "Out of list memory";
        } else {
            for (int at = 0; at < n; at += kChunk) {
                const int m = n - at < kChunk ? n - at : kChunk;
                for (int i = 0; i < ListStore::kCount; ++i) {
                    if (used[i]) {
                        lists().list(i).read_range(at, m, g_lift[i]);
                    }
                }
                for (int k = first_op; k < ctx.ops; ++k) {
                    g_op[k].read_range(at, m, g_op_lift[k]);
                }
                for (int j = 0; j < m; ++j) {
                    for (int i = 0; i < ListStore::kCount; ++i) {
                        if (used[i]) {
                            g_elem[i] = g_lift[i][j];
                        }
                    }
                    for (int k = first_op; k < ctx.ops; ++k) {
                        g_op_elem[k] = g_op_lift[k][j];
                    }
                    g_outbuf[j] = engine().eval_compiled_raw(h);
                }
                out.write_range(at, m, g_outbuf);
            }
            ok = true;
        }
        engine().free_compiled(h);
    }

    // Release this lift's operand slots (see g_op).
    for (int k = first_op; k < ctx.ops; ++k) {
        g_op[k].clear();
    }
    ctx.ops = first_op;
    return ok;
}

// True when the '{' at s[0] closes at the final char — i.e. s is one
// whole brace literal. "{1,2}+{3,4}" starts and ends with braces but
// is NOT (its first '{' closes mid-string); it must go to the lift.
bool whole_literal(const char* s, size_t len) {
    int depth = 0;
    for (size_t i = 0; i < len; ++i) {
        if (s[i] == '{') {
            ++depth;
        } else if (s[i] == '}') {
            --depth;
            if (depth == 0) {
                return i == len - 1;
            }
        }
    }
    return false;
}

bool eval_list_into(const char* s, Array& out, Ctx& ctx) {
    const RecGuard guard(ctx);
    if (guard.too_deep()) {
        ctx.err = "Too deeply nested";
        return false;
    }
    const size_t len = std::strlen(s);
    if (len == 0) {
        ctx.err = "Expected a list";
        return false;
    }

    if (s[0] == '{' && s[len - 1] == '}' && whole_literal(s, len)) {
        return eval_literal(s, out, ctx);
    }

    const int bare_slot = len == 2 ? token_at(s, s) : -1;
    if (bare_slot >= 0) {
        if (!listops::copy(lists().list(bare_slot), out)) {
            ctx.err = "Out of list memory";
            return false;
        }
        return true;
    }
    const int bare_name = bare_named(s);
    if (bare_name >= 0) {
        if (!listops::copy(named_lists().list(bare_name), out)) {
            ctx.err = "Out of list memory";
            return false;
        }
        return true;
    }

    const char* inner = nullptr;
    size_t inner_len = 0;
    if (wrapper_form(s, "seq", &inner, &inner_len)) {
        return eval_seq(inner, inner_len, out, ctx);
    }
    if (wrapper_form(s, "range", &inner, &inner_len)) {
        return eval_range(inner, inner_len, out, ctx);
    }
    for (const char* name : kValueWrappers) {
        if (!wrapper_form(s, name, &inner, &inner_len)) {
            continue;
        }
        // Depth-indexed, not stack-local: this is the recursive step,
        // so a kMaxLen pair here is paid once per nesting level. ctx.rec
        // is 1-based and bounded by kMaxRec (RecGuard), which is what
        // makes the pool a fixed size.
        auto& step = g_step[ctx.rec - 1];
        char* const arg = step.arg;
        if (inner_len >= sizeof(step.arg)) {
            ctx.err = "Expression too long";
            return false;
        }
        std::memcpy(arg, inner, inner_len);
        arg[inner_len] = 0;
        char* const targ = step.targ;
        if (!trim_into(arg, targ, sizeof(step.targ))) {
            ctx.err = "Expression too long";
            return false;
        }
        const bool is_sort = name[0] == 's';
        if (is_sort) {
            // Value semantics here; the bare-arg in-place form is
            // intercepted in evaluate().
            if (!eval_list_into(targ, out, ctx)) {
                return false;
            }
            if (out.dtype() == Dtype::kComplex) {
                ctx.err = "Non-real list";  // No ordering on complex (D37)
                return false;
            }
            const bool ok = name[5] == 'a' ? listops::sort_asc(out) : listops::sort_desc(out);
            if (!ok) {
                ctx.err = "Out of list memory";
                return false;
            }
            return true;
        }
        if (ctx.depth >= kMaxDepth) {
            ctx.err = "Too deeply nested";
            return false;
        }
        Array& temp = g_temp[ctx.depth];
        ++ctx.depth;
        const bool arg_ok = eval_list_into(targ, temp, ctx);
        --ctx.depth;
        if (!arg_ok) {
            return false;
        }
        if (temp.dtype() == Dtype::kComplex) {
            temp.clear();
            ctx.err = "Non-real list";  // cumsum/delta_list are real-only in v1 (D37)
            return false;
        }
        // The scratch may be complex-typed from an earlier evaluation.
        out.clear();
        out.set_dtype(Dtype::kDouble);
        const bool ok =
            name[0] == 'c' ? listops::cumsum(temp, out) : listops::delta_list(temp, out);
        temp.clear();  // Release the slab/region promptly
        if (!ok) {
            ctx.err = "Out of list memory";
            return false;
        }
        return true;
    }

    return eval_lift(s, out, ctx);
}

// "sort_asc(l3)" / "sort_desc(l3)" with a bare list arg: in-place
// (spec §3.2). Returns the slot, or -1.
int in_place_sort_form(const char* s, bool* asc) {
    for (int which = 0; which < 2; ++which) {
        const char* inner = nullptr;
        size_t inner_len = 0;
        if (!wrapper_form(s, which == 0 ? "sort_asc" : "sort_desc", &inner, &inner_len)) {
            continue;
        }
        char arg[16];
        if (inner_len >= sizeof(arg)) {
            return -1;
        }
        std::memcpy(arg, inner, inner_len);
        arg[inner_len] = 0;
        char targ[16];
        if (!trim_into(arg, targ, sizeof(targ))) {
            return -1;
        }
        if (std::strlen(targ) == 2) {
            const int slot = token_at(targ, targ);
            if (slot >= 0) {
                *asc = which == 0;
                return slot;
            }
        }
        const int named = bare_named(targ);  // 4D.13: in-place named sort
        if (named >= 0) {
            *asc = which == 0;
            return kNamedRefBase + named;
        }
    }
    return -1;
}

}  // namespace

Result evaluate(const char* input) {
    Result res;
    auto& body = g_top.body;
    if (!trim_into(input, body, sizeof(body))) {
        return res;  // Too long — let the engine report it
    }
    if (body[0] == 0) {
        return res;
    }

    // Store suffix "-> lk" or "-> name" (4D.13; "->a" stays in the
    // body for the engine's scalar store). A valid-but-unknown name is
    // recorded for creation at commit time, so a failing expression
    // never leaves a stray empty named list behind.
    int store = -1;  // List ref (0-5 fixed, 6+ named)
    char store_name[NamedLists::kMaxName + 1] = {};
    char* arrow = nullptr;
    for (char* p = body; (p = std::strstr(p, "->")) != nullptr; p += 2) {
        arrow = p;
    }
    if (arrow != nullptr) {
        char rhs[16];
        if (trim_into(arrow + 2, rhs, sizeof(rhs))) {
            bool take = false;
            if (std::strlen(rhs) == 2 && token_at(rhs, rhs) >= 0) {
                store = token_at(rhs, rhs);
                take = true;
            } else {
                const int ni = named_lists().find(rhs);
                if (ni >= 0) {
                    store = kNamedRefBase + ni;
                    take = true;
                } else if (NamedLists::valid_name(rhs)) {
                    std::snprintf(store_name, sizeof(store_name), "%s", rhs);
                    take = true;
                }
            }
            if (take) {
                *arrow = 0;
                auto& tb = g_top.tb;
                trim_into(body, tb, sizeof(tb));
                std::memcpy(body, tb, std::strlen(tb) + 1);
            }
        }
    }
    const bool has_store = store >= 0 || store_name[0] != 0;
    // Commit-time target resolution (creates a pending named list).
    auto store_ref = [&]() -> int {
        if (store >= 0 || store_name[0] == 0) {
            return store;
        }
        const int ni = named_lists().create(store_name);
        if (ni >= 0) {
            res.names_modified = true;
            store = kNamedRefBase + ni;
        }
        return store;  // -1 = registry full
    };

    const bool listy = contains_list_token(body) || contains_named_token(body) ||
                       std::strchr(body, '{') != nullptr || contains_wrapper_call(body);
    if (!listy) {
        if (has_store) {
            res.kind = Kind::kError;
            res.error = "Store target needs a list";
        }
        return res;
    }

    // In-place sort of a list slot (fixed or named ref).
    bool asc = false;
    const int sort_ref = in_place_sort_form(body, &asc);
    if (sort_ref >= 0) {
        Array& lst = list_by_ref(sort_ref);
        if (lst.dtype() == Dtype::kComplex) {
            res.kind = Kind::kError;
            res.error = "Non-real list";  // No ordering on complex (D37)
            return res;
        }
        const bool ok = asc ? listops::sort_asc(lst) : listops::sort_desc(lst);
        if (!ok) {
            res.kind = Kind::kError;
            res.error = "Out of list memory";
            return res;
        }
        // The sorted list itself always persists (this was silently
        // skipped for plain sorts before 4D.13 — the D35 split passed
        // stored_list = -1 to the save).
        res.lists_modified = true;
        res.lists_mask |= 1U << sort_ref;
        res.list = &lst;
        if (has_store) {
            const int target = store_ref();
            if (target < 0) {
                res.kind = Kind::kError;
                res.error = "Too many named lists";
                return res;
            }
            if (target != sort_ref && !listops::copy(lst, list_by_ref(target))) {
                res.kind = Kind::kError;
                res.error = "Out of list memory";
                return res;
            }
            res.stored_list = target;
            res.lists_mask |= 1U << target;
            res.list = &list_by_ref(target);
        }
        res.kind = Kind::kList;
        return res;
    }

    // Vector ops (4D.22), whole-expression forms: dot(A,B), cross(A,B)
    // (3-element lists), norm(A). Arguments are any list-valued
    // expressions; real-only in v1 ("Non-real list", D37).
    {
        static Array s_va;
        static Array s_vb;
        const char* inner = nullptr;
        size_t inner_len = 0;
        int op = -1;  // 0 = dot, 1 = cross, 2 = norm
        if (wrapper_form(body, "dot", &inner, &inner_len)) {
            op = 0;
        } else if (wrapper_form(body, "cross", &inner, &inner_len)) {
            op = 1;
        } else if (wrapper_form(body, "norm", &inner, &inner_len)) {
            op = 2;
        }
        if (op >= 0) {
            res.kind = Kind::kError;
            auto& args = g_top.args;
            if (inner_len >= sizeof(args)) {
                res.error = "Expression too long";
                return res;
            }
            std::memcpy(args, inner, inner_len);
            args[inner_len] = 0;
            char* comma = nullptr;
            int depth = 0;
            for (char* q = args; *q != 0; ++q) {
                if (*q == '(' || *q == '{') {
                    ++depth;
                } else if (*q == ')' || *q == '}') {
                    --depth;
                } else if (*q == ',' && depth == 0) {
                    comma = q;
                    break;
                }
            }
            if ((op == 2) != (comma == nullptr)) {
                res.error = op == 2 ? "norm takes one list" : "Need two lists";
                return res;
            }
            if (comma != nullptr) {
                *comma = 0;
            }
            Ctx ctx;
            if (!eval_list_into(args, s_va, ctx)) {
                res.error = ctx.err != nullptr ? ctx.err : "Syntax error";
                return res;
            }
            if (comma != nullptr) {
                Ctx ctx2;
                if (!eval_list_into(comma + 1, s_vb, ctx2)) {
                    s_va.clear();
                    res.error = ctx2.err != nullptr ? ctx2.err : "Syntax error";
                    return res;
                }
            }
            if (s_va.dtype() == Dtype::kComplex ||
                (comma != nullptr && s_vb.dtype() == Dtype::kComplex)) {
                s_va.clear();
                s_vb.clear();
                res.error = "Non-real list";
                return res;
            }
            if (op == 1) {  // cross: 3-element lists, list result
                if (s_va.size() != 3 || s_vb.size() != 3) {
                    s_va.clear();
                    s_vb.clear();
                    res.error = "cross needs 3-elem lists";
                    return res;
                }
                double a[3];
                double b[3];
                s_va.read_range(0, 3, a);
                s_vb.read_range(0, 3, b);
                s_va.clear();
                s_vb.clear();
                g_result.clear();
                if (!g_result.set_dtype(Dtype::kDouble) || !g_result.resize(3)) {
                    res.error = "Out of list memory";
                    return res;
                }
                g_result.set(0, a[1] * b[2] - a[2] * b[1]);
                g_result.set(1, a[2] * b[0] - a[0] * b[2]);
                g_result.set(2, a[0] * b[1] - a[1] * b[0]);
                res.list = &g_result;
                if (has_store) {
                    const int target = store_ref();
                    if (target < 0) {
                        res.error = "Too many named lists";
                        return res;
                    }
                    if (!listops::copy(g_result, list_by_ref(target))) {
                        res.error = "Out of list memory";
                        return res;
                    }
                    res.stored_list = target;
                    res.lists_modified = true;
                    res.lists_mask |= 1U << target;
                    res.list = &list_by_ref(target);
                }
                res.kind = Kind::kList;
                res.error = nullptr;
                return res;
            }
            // dot / norm: scalar results.
            if (has_store) {
                s_va.clear();
                s_vb.clear();
                res.error = "Store target needs a list";
                return res;
            }
            double v = 0;
            if (op == 0) {
                if (s_va.size() != s_vb.size() || s_va.size() == 0) {
                    s_va.clear();
                    s_vb.clear();
                    res.error = "Dim mismatch";
                    return res;
                }
                for (int i = 0; i < s_va.size(); ++i) {
                    v += s_va.get(i) * s_vb.get(i);
                }
            } else {
                for (int i = 0; i < s_va.size(); ++i) {
                    v += s_va.get(i) * s_va.get(i);
                }
                v = std::sqrt(v);
            }
            s_va.clear();
            s_vb.clear();
            engine().vars().set_real(Variables::kAns, v);
            res.kind = Kind::kScalar;
            res.error = nullptr;
            res.scalar.ok = true;
            res.scalar.value = v;
            return res;
        }
    }

    // Scalar reductions become literals; if nothing listy remains the
    // whole thing is a scalar expression for the normal engine path
    // (Ans update + scalar "->a" store included).
    const char* sub_err = nullptr;
    Complex csum_val;
    bool csum_return = false;
    if (!substitute_reductions(body, sizeof(body), &sub_err, &csum_val, &csum_return)) {
        res.kind = Kind::kError;
        res.error = sub_err;
        return res;
    }
    if (csum_return) {
        // Standalone sum/mean of a complex list (4D.24): a complex
        // scalar. The dispatch layer commits Ans and formats it.
        if (has_store) {
            res.kind = Kind::kError;
            res.error = "Store target needs a list";
            return res;
        }
        // csum_return implies the argument list is complex — in REAL
        // mode that's an error even when the sum lands on the real
        // axis, matching the complex-list recall rule below.
        if (number_mode() == NumberMode::kReal) {
            res.kind = Kind::kError;
            res.error = "Non-real result";  // D30 precedent
            return res;
        }
        res.kind = Kind::kScalar;
        res.scalar.ok = true;
        res.scalar_complex = true;
        res.cvalue = csum_val;
        return res;
    }
    if (!contains_list_token(body) && !contains_named_token(body) &&
        std::strchr(body, '{') == nullptr && !contains_wrapper_call(body)) {
        if (has_store) {
            res.kind = Kind::kError;
            res.error = "Store target needs a list";
            return res;
        }
        res.kind = Kind::kScalar;
        res.scalar = engine().evaluate(body);
        return res;
    }

    Ctx ctx;
    if (!eval_list_into(body, g_result, ctx)) {
        res.kind = Kind::kError;
        res.error = ctx.err != nullptr ? ctx.err : "Syntax error";
        return res;
    }
    // A complex list result needs a complex number mode (D30 precedent)
    // — checked before the store below so REAL mode never commits one.
    if (g_result.dtype() == Dtype::kComplex && number_mode() == NumberMode::kReal) {
        res.kind = Kind::kError;
        res.error = "Non-real result";
        return res;
    }
    res.list = &g_result;
    if (has_store) {
        const int target = store_ref();
        if (target < 0) {
            res.kind = Kind::kError;
            res.error = "Too many named lists";
            return res;
        }
        if (!listops::copy(g_result, list_by_ref(target))) {
            res.kind = Kind::kError;
            res.error = "Out of list memory";
            return res;
        }
        res.stored_list = target;
        res.lists_modified = true;
        res.lists_mask |= 1U << target;
        res.list = &list_by_ref(target);
    }
    res.kind = Kind::kList;
    return res;
}

void format_list(const Array& a, char* buf, size_t buf_len) {
    if (buf_len < 8) {
        if (buf_len > 0) {
            buf[0] = 0;
        }
        return;
    }
    size_t pos = 0;
    buf[pos++] = '{';
    const int n = a.size();
    const bool complex = a.dtype() == Dtype::kComplex;
    for (int i = 0; i < n; ++i) {
        char num[48];
        // Compact per-element formatting so more values fit before the
        // ",..." cutoff (testdrive 2026-07-20); the home screen lets you
        // pan the full list with LEFT/RIGHT.
        if (complex) {
            format_complex(a.cget(i), number_mode(), num, sizeof(num));
        } else {
            format_number_compact(a.get(i), num, sizeof(num));
        }
        const size_t need = std::strlen(num) + (i > 0 ? 1 : 0);
        if (pos + need + 6 > buf_len) {  // Room for ",<ellipsis>}" + NUL
            buf[pos++] = ',';
            buf[pos++] = kEllipsisGlyph;
            break;
        }
        if (i > 0) {
            buf[pos++] = ',';
        }
        std::memcpy(buf + pos, num, std::strlen(num));
        pos += std::strlen(num);
    }
    buf[pos++] = '}';
    buf[pos] = 0;
}

}  // namespace math::listexpr
