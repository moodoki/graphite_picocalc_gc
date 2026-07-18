#include "math/list_expr.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "math/format.hpp"
#include "math/list_ops.hpp"
#include "math/lists.hpp"

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

const char* const kWrapperNames[] = {"sort_asc", "sort_desc", "cumsum", "delta_list", "seq"};
// The single-list-argument subset (seq is handled separately).
const char* const kValueWrappers[] = {"sort_asc", "sort_desc", "cumsum", "delta_list"};

bool starts_like_wrapper(const char* s) {
    for (const char* name : kWrapperNames) {
        const char* inner = nullptr;
        size_t inner_len = 0;
        if (wrapper_form(s, name, &inner, &inner_len)) {
            return true;
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

// Replace bare-list reductions — sum(l1), prod(l1), length(l1) — with
// numeric literals so the rest of the expression can be a plain
// scalar or vector-lifted engine expression.
bool substitute_reductions(char* buf, size_t cap, const char** err) {
    const char* const names[] = {"sum", "prod", "length"};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const char* name : names) {
            const size_t nl = std::strlen(name);
            for (char* p = buf; (p = std::strstr(p, name)) != nullptr; ++p) {
                if (p > buf && ident_char(p[-1])) {
                    continue;
                }
                char* q = p + nl;
                if (*q != '(') {
                    continue;
                }
                // Argument must be a bare list name (D22): "sum(l1)".
                const char* a = q + 1;
                while (*a == ' ') {
                    ++a;
                }
                const int slot = token_at(buf, a);
                if (slot < 0) {
                    continue;
                }
                const char* close = a + 2;
                while (*close == ' ') {
                    ++close;
                }
                if (*close != ')') {
                    continue;
                }
                const Array& lst = lists().list(slot);
                calc_t v = 0;
                if (name[0] == 's') {
                    v = listops::sum(lst);
                } else if (name[0] == 'p') {
                    v = listops::prod(lst);
                } else {
                    v = static_cast<calc_t>(lst.size());
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

struct Ctx {
    const char* err = nullptr;
    int depth = 0;
};

bool eval_list_into(const char* s, Array& out, Ctx& ctx);

bool eval_literal(const char* s, Array& out, Ctx& ctx) {
    const size_t len = std::strlen(s);
    const char* starts[kMaxLiteral];
    size_t lens[kMaxLiteral];
    if (len == 2) {  // "{}"
        return out.resize(0);
    }
    const int count = split_args(s + 1, len - 2, starts, lens, kMaxLiteral);
    if (count < 0) {
        ctx.err = "List literal too long";
        return false;
    }
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
    } else if (arg[1][0] >= 'a' && arg[1][0] <= 'z' && arg[1][1] == 0 && arg[1][0] != 'e') {
        var_slot = arg[1][0] - 'a';
    } else {
        ctx.err = "seq var must be a-z (not e) or theta";
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

// Element-wise engine expression over l1..l6 (vector lift).
bool eval_lift(const char* s, Array& out, Ctx& ctx) {
    bool used[ListStore::kCount] = {};
    for (const char* p = s; *p != 0; ++p) {
        const int slot = token_at(s, p);
        if (slot >= 0) {
            used[slot] = true;
        }
    }
    int n = -1;
    for (int i = 0; i < ListStore::kCount; ++i) {
        if (!used[i]) {
            continue;
        }
        const int sz = lists().list(i).size();
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

    Engine::ExtraVar extras[ListStore::kCount];
    for (int i = 0; i < ListStore::kCount; ++i) {
        extras[i] = {kSlotNames[i], &g_elem[i]};
    }
    void* h = engine().compile_with(s, extras, ListStore::kCount);
    if (h == nullptr) {
        ctx.err = "Syntax error";
        return false;
    }
    if (!out.resize(n)) {
        engine().free_compiled(h);
        ctx.err = "Out of list memory";
        return false;
    }
    for (int at = 0; at < n; at += kChunk) {
        const int m = n - at < kChunk ? n - at : kChunk;
        for (int i = 0; i < ListStore::kCount; ++i) {
            if (used[i]) {
                lists().list(i).read_range(at, m, g_lift[i]);
            }
        }
        for (int j = 0; j < m; ++j) {
            for (int i = 0; i < ListStore::kCount; ++i) {
                if (used[i]) {
                    g_elem[i] = g_lift[i][j];
                }
            }
            g_outbuf[j] = engine().eval_compiled_raw(h);
        }
        out.write_range(at, m, g_outbuf);
    }
    engine().free_compiled(h);
    return true;
}

bool eval_list_into(const char* s, Array& out, Ctx& ctx) {
    const size_t len = std::strlen(s);
    if (len == 0) {
        ctx.err = "Expected a list";
        return false;
    }

    if (s[0] == '{' && s[len - 1] == '}') {
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

    const bool listy =
        contains_list_token(body) || std::strchr(body, '{') != nullptr || starts_like_wrapper(body);
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
    if (!substitute_reductions(body, sizeof(body), &sub_err)) {
        res.kind = Kind::kError;
        res.error = sub_err;
        return res;
    }
    if (!contains_list_token(body) && std::strchr(body, '{') == nullptr &&
        !starts_like_wrapper(body)) {
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
    for (int i = 0; i < n; ++i) {
        char num[24];
        format_number(a.get(i), num, sizeof(num));
        const size_t need = std::strlen(num) + (i > 0 ? 1 : 0);
        if (pos + need + 6 > buf_len) {  // Room for ",...}" + NUL
            std::memcpy(buf + pos, ",...", 5);
            pos += 4;
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
