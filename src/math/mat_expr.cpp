#include "math/mat_expr.hpp"

#include <cctype>
#include <cmath>
#include <cstring>

#include "math/format.hpp"
#include "math/list_ops.hpp"
#include "math/lists.hpp"
#include "math/matrix.hpp"

namespace math::matexpr {

namespace {

constexpr size_t kMaxLen = 256;
constexpr int kMaxTemps = 6;

// Intermediate matrix values. Claimed monotonically per evaluate() and
// all released at the end — expression nesting on a 128-char input
// line never needs more than a few.
Array g_temp[kMaxTemps];
// Final matrix result (MatAns) and list result (dim/eigenvals) — kept
// across calls so the home screen and editor can read them.
Array g_mresult;
Array g_lresult;

bool ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Matrix slot 0-9 when p points at a "[X]" token (X in A-J, either
// case), else -1.
int matrix_token_at(const char* p) {
    if (p[0] != '[' || p[2] != ']') {
        return -1;
    }
    const char c = p[1];
    if (c >= 'A' && c <= 'J') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'j') {
        return c - 'a';
    }
    return -1;
}

bool contains_matrix_token(const char* s) {
    for (const char* p = s; *p != 0; ++p) {
        if (matrix_token_at(p) >= 0) {
            return true;
        }
    }
    return false;
}

bool contains_call(const char* s, const char* name) {
    const size_t nl = std::strlen(name);
    for (const char* p = s; *p != 0; ++p) {
        if (p > s && ident_char(p[-1])) {
            continue;
        }
        if (std::strncmp(p, name, nl) == 0 && p[nl] == '(') {
            return true;
        }
    }
    return false;
}

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

// ---- Recursive-descent evaluator ----

struct Value {
    bool is_matrix = false;
    calc_t s = 0;
    const Array* m = nullptr;
};

struct P {
    const char* s = nullptr;  // Cursor
    const char* err = nullptr;
    int temps = 0;  // Next free g_temp slot
};

void skip_ws(P& p) {
    while (*p.s == ' ') {
        ++p.s;
    }
}

Array* claim_temp(P& p) {
    if (p.temps >= kMaxTemps) {
        p.err = "Expression too complex";
        return nullptr;
    }
    return &g_temp[p.temps++];
}

Value parse_expr(P& p);
Value parse_unary(P& p);

const Value kBad = {};

Value fail(P& p, const char* msg) {
    if (p.err == nullptr) {
        p.err = msg;
    }
    return kBad;
}

// Scalar primary: a number, constant/variable, or a scalar function
// call — the span is handed to eval_field (full engine syntax).
Value parse_scalar_span(P& p) {
    const char* start = p.s;
    if (std::isdigit(static_cast<unsigned char>(*p.s)) != 0 || *p.s == '.') {
        while (std::isdigit(static_cast<unsigned char>(*p.s)) != 0 || *p.s == '.') {
            ++p.s;
        }
        if ((*p.s == 'e' || *p.s == 'E') &&
            (std::isdigit(static_cast<unsigned char>(p.s[1])) != 0 ||
             ((p.s[1] == '+' || p.s[1] == '-') &&
              std::isdigit(static_cast<unsigned char>(p.s[2])) != 0))) {
            p.s += 2;
            while (std::isdigit(static_cast<unsigned char>(*p.s)) != 0) {
                ++p.s;
            }
        }
    } else if (std::isalpha(static_cast<unsigned char>(*p.s)) != 0) {
        while (ident_char(*p.s)) {
            ++p.s;
        }
        if (*p.s == '(') {  // Scalar function call: take the balanced parens
            int depth = 0;
            const char* q = p.s;
            for (; *q != 0; ++q) {
                if (*q == '(') {
                    ++depth;
                } else if (*q == ')') {
                    --depth;
                    if (depth == 0) {
                        ++q;
                        break;
                    }
                } else if (*q == '[') {
                    return fail(p, "Matrix not allowed here");
                }
            }
            if (depth != 0) {
                return fail(p, "Syntax error");
            }
            p.s = q;
        }
    } else {
        return fail(p, "Syntax error");
    }

    char span[kMaxLen];
    const auto len = static_cast<size_t>(p.s - start);
    if (len == 0 || len >= sizeof(span)) {
        return fail(p, "Syntax error");
    }
    std::memcpy(span, start, len);
    span[len] = 0;
    Value v;
    if (!eval_field(span, &v.s)) {
        return fail(p, "Syntax error");
    }
    return v;
}

struct MatFn {
    const char* name;
    int args;  // 1 or 2; matrix args except identity (scalar n)
};

// Matrix-valued functions handled by the parser. det/rank (scalar
// results) are here too; dim/eigenvals are whole-expression forms
// handled in evaluate() and rejected here.
const MatFn kMatFns[] = {
    {"inverse", 1},  {"transpose", 1}, {"rref", 1}, {"ref", 1}, {"augment", 2},
    {"identity", 1}, {"det", 1},       {"rank", 1}, {"dim", 1}, {"eigenvals", 1},
};

Value parse_matrix_fn(P& p, const MatFn& fn) {
    if (std::strcmp(fn.name, "dim") == 0 || std::strcmp(fn.name, "eigenvals") == 0) {
        return fail(p, "dim/eigenvals must stand alone");
    }
    ++p.s;  // '('
    skip_ws(p);

    if (std::strcmp(fn.name, "identity") == 0) {
        const Value n = parse_expr(p);
        if (p.err != nullptr) {
            return kBad;
        }
        skip_ws(p);
        if (*p.s != ')') {
            return fail(p, "Syntax error");
        }
        ++p.s;
        if (n.is_matrix || n.s != std::floor(n.s)) {
            return fail(p, "Bad identity size");
        }
        Array* out = claim_temp(p);
        if (out == nullptr) {
            return kBad;
        }
        Value v;
        v.is_matrix = true;
        v.m = out;
        if (!matops::identity(static_cast<int>(n.s), *out, &p.err)) {
            return kBad;
        }
        return v;
    }

    const Value a = parse_expr(p);
    if (p.err != nullptr) {
        return kBad;
    }
    if (!a.is_matrix) {
        return fail(p, "Expected a matrix");
    }
    Value b;
    if (fn.args == 2) {
        skip_ws(p);
        if (*p.s != ',') {
            return fail(p, "augment needs two matrices");
        }
        ++p.s;
        b = parse_expr(p);
        if (p.err != nullptr) {
            return kBad;
        }
        if (!b.is_matrix) {
            return fail(p, "Expected a matrix");
        }
    }
    skip_ws(p);
    if (*p.s != ')') {
        return fail(p, "Syntax error");
    }
    ++p.s;

    if (std::strcmp(fn.name, "det") == 0) {
        Value v;
        if (!matops::determinant(*a.m, &v.s, &p.err)) {
            return kBad;
        }
        return v;
    }
    if (std::strcmp(fn.name, "rank") == 0) {
        int rk = 0;
        if (!matops::rank(*a.m, &rk, &p.err)) {
            return kBad;
        }
        Value v;
        v.s = rk;
        return v;
    }

    Array* out = claim_temp(p);
    if (out == nullptr) {
        return kBad;
    }
    Value v;
    v.is_matrix = true;
    v.m = out;
    bool ok = false;
    if (std::strcmp(fn.name, "inverse") == 0) {
        ok = matops::inverse(*a.m, *out, &p.err);
    } else if (std::strcmp(fn.name, "transpose") == 0) {
        ok = matops::transpose(*a.m, *out, &p.err);
    } else if (std::strcmp(fn.name, "rref") == 0) {
        ok = matops::rref(*a.m, *out, &p.err);
    } else if (std::strcmp(fn.name, "ref") == 0) {
        ok = matops::ref(*a.m, *out, &p.err);
    } else {  // augment (b was parsed above; the null check is for the analyzer)
        ok = b.m != nullptr && matops::augment(*a.m, *b.m, *out, &p.err);
    }
    return ok ? v : kBad;
}

Value parse_primary(P& p) {
    skip_ws(p);

    const int slot = matrix_token_at(p.s);
    if (slot >= 0) {
        p.s += 3;
        const Array& m = matrices().matrix(slot);
        if (m.size() == 0) {
            return fail(p, "Matrix is empty");
        }
        skip_ws(p);
        if (*p.s == '(') {  // Element access: [A](row, col), 1-based
            ++p.s;
            const Value r = parse_expr(p);
            skip_ws(p);
            if (p.err == nullptr && *p.s != ',') {
                return fail(p, "Expected (row, col)");
            }
            if (p.err != nullptr) {
                return kBad;
            }
            ++p.s;
            const Value c = parse_expr(p);
            skip_ws(p);
            if (p.err != nullptr) {
                return kBad;
            }
            if (*p.s != ')') {
                return fail(p, "Expected (row, col)");
            }
            ++p.s;
            if (r.is_matrix || c.is_matrix || r.s != std::floor(r.s) || c.s != std::floor(c.s)) {
                return fail(p, "Expected (row, col)");
            }
            const int ri = static_cast<int>(r.s);
            const int ci = static_cast<int>(c.s);
            if (ri < 1 || ri > m.dim(0) || ci < 1 || ci > m.dim(1)) {
                return fail(p, "Index out of range");
            }
            Value v;
            v.s = m.get(ri - 1, ci - 1);
            return v;
        }
        Value v;
        v.is_matrix = true;
        v.m = &m;
        return v;
    }

    if (*p.s == '(') {
        ++p.s;
        const Value v = parse_expr(p);
        if (p.err != nullptr) {
            return kBad;
        }
        skip_ws(p);
        if (*p.s != ')') {
            return fail(p, "Syntax error");
        }
        ++p.s;
        return v;
    }

    if (std::isalpha(static_cast<unsigned char>(*p.s)) != 0) {
        for (const MatFn& fn : kMatFns) {
            const size_t nl = std::strlen(fn.name);
            if (std::strncmp(p.s, fn.name, nl) == 0 && p.s[nl] == '(') {
                p.s += nl;
                return parse_matrix_fn(p, fn);
            }
        }
    }

    return parse_scalar_span(p);
}

Value parse_power(P& p) {
    Value base = parse_primary(p);
    if (p.err != nullptr) {
        return kBad;
    }
    skip_ws(p);
    if (*p.s != '^') {
        return base;
    }
    ++p.s;
    skip_ws(p);

    if (base.is_matrix) {
        // [A]^T (transpose), [A]^-1, [A]^n
        if ((*p.s == 'T' || *p.s == 't') && !ident_char(p.s[1])) {
            ++p.s;
            Array* out = claim_temp(p);
            if (out == nullptr) {
                return kBad;
            }
            Value v;
            v.is_matrix = true;
            v.m = out;
            if (!matops::transpose(*base.m, *out, &p.err)) {
                return kBad;
            }
            return v;
        }
        const Value e = parse_unary(p);
        if (p.err != nullptr) {
            return kBad;
        }
        if (e.is_matrix || e.s != std::floor(e.s)) {
            return fail(p, "Bad matrix exponent");
        }
        Array* out = claim_temp(p);
        if (out == nullptr) {
            return kBad;
        }
        Value v;
        v.is_matrix = true;
        v.m = out;
        if (!matops::power(*base.m, static_cast<int>(e.s), *out, &p.err)) {
            return kBad;
        }
        return v;
    }

    const Value e = parse_unary(p);  // Right-associative via recursion
    if (p.err != nullptr) {
        return kBad;
    }
    if (e.is_matrix) {
        return fail(p, "Bad exponent");
    }
    Value v;
    v.s = std::pow(base.s, e.s);
    return v;
}

Value parse_unary(P& p) {
    skip_ws(p);
    bool neg = false;
    while (*p.s == '-' || *p.s == '+') {
        if (*p.s == '-') {
            neg = !neg;
        }
        ++p.s;
        skip_ws(p);
    }
    Value v = parse_power(p);
    if (p.err != nullptr || !neg) {
        return v;
    }
    if (!v.is_matrix) {
        v.s = -v.s;
        return v;
    }
    Array* out = claim_temp(p);
    if (out == nullptr) {
        return kBad;
    }
    Value r;
    r.is_matrix = true;
    r.m = out;
    if (!matops::scalar_mul(*v.m, -1, *out, &p.err)) {
        return kBad;
    }
    return r;
}

Value combine_mul(P& p, const Value& a, const Value& b, bool divide) {
    if (!a.is_matrix && !b.is_matrix) {
        Value v;
        v.s = divide ? a.s / b.s : a.s * b.s;
        return v;
    }
    if (a.is_matrix && b.is_matrix) {
        if (divide) {
            return fail(p, "Matrix division: use ^-1");
        }
        Array* out = claim_temp(p);
        if (out == nullptr) {
            return kBad;
        }
        Value v;
        v.is_matrix = true;
        v.m = out;
        if (!matops::mul(*a.m, *b.m, *out, &p.err)) {
            return kBad;
        }
        return v;
    }
    // One matrix, one scalar
    if (divide && b.is_matrix) {
        return fail(p, "Matrix division: use ^-1");
    }
    const Array& m = a.is_matrix ? *a.m : *b.m;
    const calc_t k = a.is_matrix ? (divide ? 1.0 / b.s : b.s) : a.s;
    Array* out = claim_temp(p);
    if (out == nullptr) {
        return kBad;
    }
    Value v;
    v.is_matrix = true;
    v.m = out;
    if (!matops::scalar_mul(m, k, *out, &p.err)) {
        return kBad;
    }
    return v;
}

Value parse_term(P& p) {
    Value left = parse_unary(p);
    while (p.err == nullptr) {
        skip_ws(p);
        if (*p.s != '*' && *p.s != '/') {
            break;
        }
        const bool divide = *p.s == '/';
        ++p.s;
        const Value right = parse_unary(p);
        if (p.err != nullptr) {
            return kBad;
        }
        left = combine_mul(p, left, right, divide);
    }
    return left;
}

Value parse_expr(P& p) {
    Value left = parse_term(p);
    while (p.err == nullptr) {
        skip_ws(p);
        if ((*p.s != '+' && *p.s != '-') || p.s[1] == '>') {
            break;  // "->" is the store arrow, not subtraction
        }
        const bool subtract = *p.s == '-';
        ++p.s;
        const Value right = parse_term(p);
        if (p.err != nullptr) {
            return kBad;
        }
        if (left.is_matrix != right.is_matrix) {
            return fail(p, "Dim mismatch");
        }
        if (!left.is_matrix) {
            left.s = subtract ? left.s - right.s : left.s + right.s;
            continue;
        }
        Array* out = claim_temp(p);
        if (out == nullptr) {
            return kBad;
        }
        Value v;
        v.is_matrix = true;
        v.m = out;
        const bool ok = subtract ? matops::sub(*left.m, *right.m, *out, &p.err)
                                 : matops::add(*left.m, *right.m, *out, &p.err);
        if (!ok) {
            return kBad;
        }
        left = v;
    }
    return left;
}

void release_temps() {
    for (Array& t : g_temp) {
        t.clear();
    }
}

// Store target parsed from the text right of the arrow. Exactly one of
// the fields is >= 0 on success.
struct StoreTarget {
    int matrix = -1;
    int list = -1;
    int var = -1;  // Variables index (0-25 or kTheta)
    bool valid = false;
};

StoreTarget parse_store_target(const char* rhs) {
    StoreTarget t;
    const size_t len = std::strlen(rhs);
    if (len == 3 && matrix_token_at(rhs) >= 0) {
        t.matrix = matrix_token_at(rhs);
        t.valid = true;
    } else if (len == 2 && rhs[0] == 'l' && rhs[1] >= '1' && rhs[1] <= '6') {
        t.list = rhs[1] - '1';
        t.valid = true;
    } else if (std::strcmp(rhs, "theta") == 0) {
        t.var = Variables::kTheta;
        t.valid = true;
    } else if (len == 1 && rhs[0] >= 'a' && rhs[0] <= 'z' && rhs[0] != 'e') {
        t.var = rhs[0] - 'a';
        t.valid = true;
    }
    return t;
}

// True when s is exactly `name(...)` (matching wrapper_form in
// list_expr).
bool whole_call(const char* s, const char* name, const char** inner, size_t* inner_len) {
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
                    return false;
                }
                break;
            }
        }
    }
    *inner = s + nl + 1;
    *inner_len = len - nl - 2;
    return true;
}

// Evaluate `s` as a matrix-valued expression (parser + full-consumption
// check). On success the value lives in *out_val (a temp or a store
// slot reference).
bool eval_matrix_body(const char* s, Value* out_val, const char** err) {
    P p;
    p.s = s;
    const Value v = parse_expr(p);
    if (p.err == nullptr) {
        skip_ws(p);
        if (*p.s != 0) {
            p.err = "Syntax error";
        } else if (!v.is_matrix) {
            p.err = "Expected a matrix";
        }
    }
    if (p.err != nullptr) {
        *err = p.err;
        return false;
    }
    *out_val = v;
    return true;
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

    // Involves matrices at all? [X] tokens, or identity() (the only
    // matrix producer that needs no matrix input).
    if (!contains_matrix_token(body) && !contains_call(body, "identity")) {
        return res;
    }

    // Store suffix: "-> [C]" / "-> lk" / "-> a" (rightmost arrow).
    StoreTarget store;
    char* arrow = nullptr;
    for (char* q = body; (q = std::strstr(q, "->")) != nullptr; q += 2) {
        arrow = q;
    }
    if (arrow != nullptr) {
        char rhs[16];
        if (trim_into(arrow + 2, rhs, sizeof(rhs))) {
            store = parse_store_target(rhs);
            if (store.valid) {
                *arrow = 0;
                char tb[kMaxLen];
                trim_into(body, tb, sizeof(tb));
                std::memcpy(body, tb, std::strlen(tb) + 1);
            }
        }
        if (!store.valid) {
            res.kind = Kind::kError;
            res.error = "Bad store target";
            return res;
        }
    }

    release_temps();

    // Whole-expression list forms: dim(...) and eigenvals(...).
    for (int which = 0; which < 2; ++which) {
        const char* name = which == 0 ? "dim" : "eigenvals";
        const char* inner = nullptr;
        size_t inner_len = 0;
        if (!whole_call(body, name, &inner, &inner_len)) {
            continue;
        }
        char arg[kMaxLen];
        if (inner_len >= sizeof(arg)) {
            res.kind = Kind::kError;
            res.error = "Expression too long";
            return res;
        }
        std::memcpy(arg, inner, inner_len);
        arg[inner_len] = 0;
        Value v;
        res.kind = Kind::kError;
        if (!eval_matrix_body(arg, &v, &res.error)) {
            release_temps();
            return res;
        }
        bool ok = false;
        if (which == 0) {
            ok = g_lresult.resize(2);
            if (ok) {
                g_lresult.set(0, v.m->dim(0));
                g_lresult.set(1, v.m->dim(1));
            } else {
                res.error = "Out of list memory";
            }
        } else {
            ok = matops::eigenvalues(*v.m, g_lresult, &res.error);
        }
        release_temps();
        if (!ok) {
            return res;
        }
        res.kind = Kind::kList;
        res.error = nullptr;
        res.list = &g_lresult;
        if (store.matrix >= 0 || store.var >= 0) {
            res.kind = Kind::kError;
            res.error = "Store target mismatch";
            return res;
        }
        if (store.list >= 0) {
            if (!listops::copy(g_lresult, lists().list(store.list))) {
                res.kind = Kind::kError;
                res.error = "Out of list memory";
                return res;
            }
            res.stored_list = store.list;
            res.lists_modified = true;
            res.list = &lists().list(store.list);
        }
        return res;
    }

    // General expression.
    P p;
    p.s = body;
    const Value v = parse_expr(p);
    if (p.err == nullptr) {
        skip_ws(p);
        if (*p.s != 0) {
            p.err = "Syntax error";
        }
    }
    if (p.err != nullptr) {
        release_temps();
        res.kind = Kind::kError;
        res.error = p.err;
        return res;
    }

    if (!v.is_matrix) {
        release_temps();
        if (store.matrix >= 0 || store.list >= 0) {
            res.kind = Kind::kError;
            res.error = "Store target mismatch";
            return res;
        }
        if (!std::isfinite(v.s)) {
            res.kind = Kind::kError;
            res.error = "Undefined result";
            return res;
        }
        res.kind = Kind::kScalar;
        res.scalar.ok = true;
        res.scalar.value = v.s;
        engine().vars().ans() = v.s;
        if (store.var >= 0) {
            engine().vars().vars[store.var] = v.s;
            res.scalar.stored_var = store.var;
        }
        return res;
    }

    if (store.list >= 0 || store.var >= 0) {
        release_temps();
        res.kind = Kind::kError;
        res.error = "Store target mismatch";
        return res;
    }
    // Keep the result in the persistent MatAns buffer, then release
    // the expression temps.
    if (!matops::copy(*v.m, g_mresult)) {
        release_temps();
        res.kind = Kind::kError;
        res.error = "Out of matrix memory";
        return res;
    }
    release_temps();
    res.kind = Kind::kMatrix;
    res.matrix = &g_mresult;
    if (store.matrix >= 0) {
        if (!matops::copy(g_mresult, matrices().matrix(store.matrix))) {
            res.kind = Kind::kError;
            res.error = "Out of matrix memory";
            return res;
        }
        res.stored_matrix = store.matrix;
        res.matrices_modified = true;
        res.matrix = &matrices().matrix(store.matrix);
    }
    return res;
}

const Array& mat_ans() {
    return g_mresult;
}

void format_matrix(const Array& m, char* buf, size_t buf_len) {
    if (buf_len < 12) {
        if (buf_len > 0) {
            buf[0] = 0;
        }
        return;
    }
    size_t pos = 0;
    buf[pos++] = '[';
    const int rows = m.dim(0);
    const int cols = m.dim(1);
    bool truncated = false;
    for (int r = 0; r < rows && !truncated; ++r) {
        buf[pos++] = '[';
        for (int c = 0; c < cols; ++c) {
            char num[24];
            format_number(m.get(r, c), num, sizeof(num));
            const size_t need = std::strlen(num) + (c > 0 ? 1 : 0);
            // Room for the number plus "...]]" + NUL in the worst case
            if (pos + need + 7 > buf_len) {
                std::memcpy(buf + pos, "...", 3);
                pos += 3;
                truncated = true;
                break;
            }
            if (c > 0) {
                buf[pos++] = ',';
            }
            std::memcpy(buf + pos, num, std::strlen(num));
            pos += std::strlen(num);
        }
        buf[pos++] = ']';
    }
    buf[pos++] = ']';
    buf[pos] = 0;
}

}  // namespace math::matexpr
