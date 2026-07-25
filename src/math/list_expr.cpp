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
#include "math/stats.hpp"

namespace math::listexpr {

namespace {

constexpr size_t kMaxLen = 256;
constexpr int kChunk = 256;
constexpr int kMaxLiteral = 64;  // Input line is 128 chars — plenty
constexpr int kMaxDepth = 2;     // Wrapper nesting (cumsum(sort_asc(..)))

Array g_result;
Array g_temp[kMaxDepth];

// Vector-lift state: l1..l6 bind to g_elem slots in the compiled
// expression; chunks stream through g_lift.
calc_t g_elem[ListStore::kCount];
calc_t g_lift[ListStore::kCount][kChunk];
calc_t g_outbuf[kChunk];
const char* const kSlotNames[ListStore::kCount] = {"l1", "l2", "l3", "l4", "l5", "l6"};

// Lift operands (D24): brace literals and wrapper calls inside a lifted
// expression ("{1,2,3}+2", "range(1,9)*l1") are evaluated into these
// side arrays and bound like extra list slots. Slots are handed out
// monotonically per evaluate() (Ctx::ops) and released when the lift
// that extracted them finishes, so nested lifts never alias.
constexpr int kMaxOperands = 4;
Array g_op[kMaxOperands];
calc_t g_op_elem[kMaxOperands];
calc_t g_op_lift[kMaxOperands][kChunk];
const char* const kOpNames[kMaxOperands] = {"lopa", "lopb", "lopc", "lopd"};

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
};

bool eval_list_into(const char* s, Array& out, Ctx& ctx);

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
                char raw[kMaxLen];
                const auto alen = static_cast<size_t>(close - (q + 1));
                if (alen >= sizeof(raw)) {
                    *err = "Expression too long";
                    return false;
                }
                std::memcpy(raw, q + 1, alen);
                raw[alen] = 0;
                char arg[kMaxLen];
                if (!trim_into(raw, arg, sizeof(arg))) {
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
                char rebuilt[kMaxLen];
                const int wrote = std::snprintf(rebuilt, sizeof(rebuilt), "%.*s(%s)%s",
                                                static_cast<int>(p - buf), buf, num, close + 1);
                if (wrote < 0 || wrote >= static_cast<int>(cap) ||
                    wrote >= static_cast<int>(sizeof(rebuilt))) {
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

bool eval_literal(const char* s, Array& out, Ctx& ctx) {
    const size_t len = std::strlen(s);
    const char* starts[kMaxLiteral];
    size_t lens[kMaxLiteral];
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
    for (int i = 0; i < count; ++i) {
        char arg[kMaxLen];
        if (lens[i] >= sizeof(arg)) {
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
            char arg[kMaxLen];
            std::memcpy(arg, starts[i], lens[i]);
            arg[lens[i]] = 0;
            const auto r = complexexpr::evaluate(arg);
            if (!r.ok) {
                ctx.err = "Bad list element";
                return false;
            }
            out.cset(i, r.value);
        }
        return true;
    }

    out.set_dtype(Dtype::kDouble);
    calc_t values[kMaxLiteral];
    for (int i = 0; i < count; ++i) {
        char arg[kMaxLen];
        if (lens[i] >= sizeof(arg)) {
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

bool eval_seq(const char* inner, size_t inner_len, Array& out, Ctx& ctx) {
    const char* starts[5];
    size_t lens[5];
    const int count = split_args(inner, inner_len, starts, lens, 5);
    if (count != 5) {
        ctx.err = "seq needs (expr, var, lo, hi, step)";
        return false;
    }
    char arg[5][kMaxLen];
    for (int i = 0; i < 5; ++i) {
        char raw[kMaxLen];
        if (lens[i] >= sizeof(raw)) {
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
bool eval_range(const char* inner, size_t inner_len, Array& out, Ctx& ctx) {
    const char* starts[3];
    size_t lens[3];
    const int count = split_args(inner, inner_len, starts, lens, 3);
    if (count != 2 && count != 3) {
        ctx.err = "range needs (lo, hi[, step])";
        return false;
    }
    calc_t vals[3] = {0, 0, 0};
    for (int i = 0; i < count; ++i) {
        char raw[kMaxLen];
        char arg[kMaxLen];
        if (lens[i] >= sizeof(raw)) {
            ctx.err = "Expression too long";
            return false;
        }
        std::memcpy(raw, starts[i], lens[i]);
        raw[lens[i]] = 0;
        if (!trim_into(raw, arg, sizeof(arg)) || !eval_field(arg, &vals[i])) {
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
        char sub[kMaxLen];
        if (span_len >= sizeof(sub)) {
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

bool eval_clift(const char* rw, int first_op, Array& out, Ctx& ctx) {
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
                char termbuf[kMaxLen];
                const size_t tlen = i - term_start;
                if (tlen >= sizeof(termbuf)) {
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
                    char fbuf[kMaxLen];
                    const size_t flen2 = j - fstart;
                    if (flen2 == 0 || flen2 >= sizeof(fbuf)) {
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
                        const auto r = complexexpr::evaluate(fbuf);
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
    char rw[kMaxLen];
    if (!extract_operands(s, rw, sizeof(rw), ctx)) {
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
        char arg[kMaxLen];
        if (inner_len >= sizeof(arg)) {
            ctx.err = "Expression too long";
            return false;
        }
        std::memcpy(arg, inner, inner_len);
        arg[inner_len] = 0;
        char targ[kMaxLen];
        if (!trim_into(arg, targ, sizeof(targ))) {
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
        if (!trim_into(arg, targ, sizeof(targ)) || std::strlen(targ) != 2) {
            return -1;
        }
        const int slot = token_at(targ, targ);
        if (slot >= 0) {
            *asc = which == 0;
            return slot;
        }
    }
    return -1;
}

}  // namespace

Result evaluate(const char* input) {
    Result res;
    char body[kMaxLen];
    if (!trim_into(input, body, sizeof(body))) {
        return res;  // Too long — let the engine report it
    }
    if (body[0] == 0) {
        return res;
    }

    // Store suffix "-> lk" (list targets only; "->a" stays in the body
    // for the engine's scalar store).
    int store = -1;
    char* arrow = nullptr;
    for (char* p = body; (p = std::strstr(p, "->")) != nullptr; p += 2) {
        arrow = p;
    }
    if (arrow != nullptr) {
        char rhs[16];
        if (trim_into(arrow + 2, rhs, sizeof(rhs)) && std::strlen(rhs) == 2) {
            const int slot = token_at(rhs, rhs);
            if (slot >= 0) {
                store = slot;
                *arrow = 0;
                char tb[kMaxLen];
                trim_into(body, tb, sizeof(tb));
                std::memcpy(body, tb, std::strlen(tb) + 1);
            }
        }
    }

    const bool listy = contains_list_token(body) || std::strchr(body, '{') != nullptr ||
                       contains_wrapper_call(body);
    if (!listy) {
        if (store >= 0) {
            res.kind = Kind::kError;
            res.error = "Store to l1-l6 needs a list";
        }
        return res;
    }

    // In-place sort of a list slot.
    bool asc = false;
    const int sort_slot = in_place_sort_form(body, &asc);
    if (sort_slot >= 0) {
        Array& lst = lists().list(sort_slot);
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
        res.lists_modified = true;
        res.list = &lst;
        if (store >= 0 && store != sort_slot) {
            if (!listops::copy(lst, lists().list(store))) {
                res.kind = Kind::kError;
                res.error = "Out of list memory";
                return res;
            }
            res.stored_list = store;
            res.list = &lists().list(store);
        } else if (store == sort_slot) {
            res.stored_list = store;
        }
        res.kind = Kind::kList;
        return res;
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
        if (store >= 0) {
            res.kind = Kind::kError;
            res.error = "Store to l1-l6 needs a list";
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
    if (!contains_list_token(body) && std::strchr(body, '{') == nullptr &&
        !contains_wrapper_call(body)) {
        if (store >= 0) {
            res.kind = Kind::kError;
            res.error = "Store to l1-l6 needs a list";
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
    if (store >= 0) {
        if (!listops::copy(g_result, lists().list(store))) {
            res.kind = Kind::kError;
            res.error = "Out of list memory";
            return res;
        }
        res.stored_list = store;
        res.lists_modified = true;
        res.list = &lists().list(store);
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
