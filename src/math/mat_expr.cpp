#include "math/mat_expr.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "math/complex_expr.hpp"
#include "math/format.hpp"
#include "math/frac.hpp"
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
// eigenvals() text form when the spectrum has a complex-conjugate
// pair (Phase 4C, D30/P4-7) — lists are real-only, so this can't be a
// g_lresult the way an all-real spectrum is.
char g_ctext[64];

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

// Standalone identifier `name` anywhere in s (the MatAns token gate).
bool contains_ident(const char* s, const char* name) {
    const size_t nl = std::strlen(name);
    for (const char* p = s; *p != 0; ++p) {
        if (p > s && ident_char(p[-1])) {
            continue;
        }
        if (std::strncmp(p, name, nl) == 0 && !ident_char(p[nl])) {
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

// Scalars ride as Complex since 4D.25 (det/element access of a complex
// matrix, i-bearing subterms); real values just carry im == 0 and all
// real behavior is unchanged.
struct Value {
    bool is_matrix = false;
    Complex s;
    const Array* m = nullptr;
};

// v as an exact real integer (indices, dims, exponents).
bool real_int(const Complex& v, int* out) {
    if (v.im != 0 || v.re != std::floor(v.re)) {
        return false;
    }
    *out = static_cast<int>(v.re);
    return true;
}

struct P {
    const char* s = nullptr;  // Cursor
    const char* err = nullptr;
    int temps = 0;  // Next free g_temp slot
    int depth = 0;  // Parse recursion level (D48) — see kMaxParseDepth
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
// call — the span is handed to eval_field (full engine syntax), with
// the complex evaluator as fallback for i-bearing/complex-variable
// spans (4D.25).
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
        if (*p.s == 'i' && !ident_char(p.s[1])) {
            ++p.s;  // "2i" shorthand: fold the unit into the span
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

    const auto len = static_cast<size_t>(p.s - start);
    if (len == 0 || len >= kMaxLen) {
        return fail(p, "Syntax error");
    }

    // Plain numeric literal: parse it here instead of handing it to
    // eval_field. That fallback runs the whole tinyexpr engine —
    // Engine::compile/compile_with (280/288 B) plus its own recursive parser —
    // and it sits at the *leaf* of this parser's recursion, i.e. at maximum
    // stack depth. Same defect D47 fixed in complexexpr (a0939bf); matexpr has
    // its own copy of this function and never got it. It cost the Pico 2 a
    // hard fault on 2026-08-08: det([[1,2][3,4]]) and det(identity(2)) both
    // crashed at depth 3 in parse_power's prologue, while det([a]*[c]+[d]) —
    // matrix references only, no literal at depth — was fine. strtod is what
    // tinyexpr would have used for these anyway.
    //
    // "2i" shorthand falls through correctly: strtod stops at the 'i', so
    // `end` lands before p.s and the complex path below takes it.
    if (std::isdigit(static_cast<unsigned char>(*start)) != 0 || *start == '.') {
        char* end = nullptr;
        const double d = std::strtod(start, &end);
        if (end == p.s) {
            Value v;
            v.s = Complex(static_cast<calc_t>(d));
            return v;
        }
    }

    // Static for the same reason as complexexpr's: this is the leaf of the
    // parser's recursion, so its frame is paid at maximum depth.
    // parse_scalar_span never nests — it consumes a terminal span, and neither
    // eval_field nor complexexpr::evaluate re-enters this parser.
    static char span[kMaxLen];
    std::memcpy(span, start, len);
    span[len] = 0;
    Value v;
    calc_t rv = 0;
    if (eval_field(span, &rv)) {
        v.s = Complex(rv);
        return v;
    }
    // i-bearing spans ("i", "2i") and complex-valued variables don't
    // ride the real field evaluator (4D.15/4D.25).
    // Inside matrix evaluation, several frames deep — see
    // kMaxParseDepthNested (D47).
    const auto cr = complexexpr::evaluate(span, complexexpr::kMaxParseDepthNested);
    if (!cr.ok) {
        return fail(p, "Syntax error");
    }
    v.s = cr.value;
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
    {"inverse", 1},  {"transpose", 1}, {"rref", 1}, {"ref", 1},  {"augment", 2},
    {"identity", 1}, {"det", 1},       {"rank", 1}, {"norm", 1},  // Frobenius (4D.22)
    {"eigenvec", 1},                                              // 4D.23
    {"dim", 1},      {"eigenvals", 1}, {"eig", 1},  // alias of eigenvals (whole-expression form
                                                    // only)
};

Value parse_matrix_fn(P& p, const MatFn& fn) {
    if (std::strcmp(fn.name, "dim") == 0 || std::strcmp(fn.name, "eigenvals") == 0 ||
        std::strcmp(fn.name, "eig") == 0) {
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
        int ni = 0;
        if (n.is_matrix || !real_int(n.s, &ni)) {
            return fail(p, "Bad identity size");
        }
        Array* out = claim_temp(p);
        if (out == nullptr) {
            return kBad;
        }
        Value v;
        v.is_matrix = true;
        v.m = out;
        if (!matops::identity(ni, *out, &p.err)) {
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
        Complex d;
        if (!matops::determinant(*a.m, &d, &p.err)) {
            return kBad;
        }
        v.s = d;
        return v;
    }
    if (std::strcmp(fn.name, "rank") == 0) {
        int rk = 0;
        if (!matops::rank(*a.m, &rk, &p.err)) {
            return kBad;
        }
        Value v;
        v.s = Complex(rk);
        return v;
    }
    if (std::strcmp(fn.name, "norm") == 0) {
        calc_t nf = 0;
        if (!matops::norm_f(*a.m, &nf, &p.err)) {
            return kBad;
        }
        Value v;
        v.s = Complex(nf);
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
    } else if (std::strcmp(fn.name, "eigenvec") == 0) {
        ok = matops::eigenvectors(*a.m, *out, &p.err);
    } else {  // augment (b was parsed above; the null check is for the analyzer)
        ok = b.m != nullptr && matops::augment(*a.m, *b.m, *out, &p.err);
    }
    return ok ? v : kBad;
}

// Home-screen matrix literal [[1,2][3,4]] (4D.14): rows of full scalar
// expressions; complex elements make a complex matrix. Materializes
// into an expression temp.
Value parse_matrix_literal(P& p) {
    constexpr int kMaxLit = 64;
    static Complex vals[kMaxLit];  // Static: rows*cols scratch off the stack
    ++p.s;                         // Outer '['
    int rows = 0;
    int cols = 0;
    int count = 0;
    bool any_complex = false;
    while (true) {
        skip_ws(p);
        if (*p.s != '[') {
            return fail(p, "Syntax error");
        }
        ++p.s;
        int c = 0;
        while (true) {
            const Value v = parse_expr(p);
            if (p.err != nullptr) {
                return kBad;
            }
            if (v.is_matrix) {
                return fail(p, "Syntax error");
            }
            if (count >= kMaxLit) {
                return fail(p, "Matrix literal too large");
            }
            vals[count++] = v.s;
            any_complex = any_complex || v.s.im != 0;
            ++c;
            skip_ws(p);
            if (*p.s == ',') {
                ++p.s;
                continue;
            }
            if (*p.s == ']') {
                ++p.s;
                break;
            }
            return fail(p, "Syntax error");
        }
        if (rows == 0) {
            cols = c;
        } else if (c != cols) {
            return fail(p, "Dim mismatch");
        }
        ++rows;
        skip_ws(p);
        if (*p.s == '[') {
            continue;
        }
        if (*p.s == ']') {
            ++p.s;
            break;
        }
        return fail(p, "Syntax error");
    }
    if (any_complex && number_mode() == NumberMode::kReal) {
        return fail(p, "Non-real result");
    }
    Array* out = claim_temp(p);
    if (out == nullptr) {
        return kBad;
    }
    out->clear();
    if (!out->set_dtype(any_complex ? Dtype::kComplex : Dtype::kDouble) ||
        !out->resize(rows, cols)) {
        return fail(p, "Out of matrix memory");
    }
    for (int i = 0; i < count; ++i) {
        if (any_complex) {
            out->cset(i, vals[i]);
        } else {
            out->set(i, vals[i].re);
        }
    }
    Value v;
    v.is_matrix = true;
    v.m = out;
    return v;
}

// list2mat(l1, l2, ...) (4D.12): pack lists into matrix columns.
// Shorter lists zero-pad to the longest.
Value parse_list2mat(P& p) {
    ++p.s;  // '('
    int src[6];
    int nsrc = 0;
    while (true) {
        skip_ws(p);
        if (p.s[0] != 'l' || p.s[1] < '1' || p.s[1] > '6' || ident_char(p.s[2])) {
            return fail(p, "list2mat takes l1-l6 args");
        }
        if (nsrc >= 6) {
            return fail(p, "list2mat: too many lists");
        }
        src[nsrc++] = p.s[1] - '1';
        p.s += 2;
        skip_ws(p);
        if (*p.s == ',') {
            ++p.s;
            continue;
        }
        if (*p.s == ')') {
            ++p.s;
            break;
        }
        return fail(p, "Syntax error");
    }
    int rows = 0;
    bool any_complex = false;
    for (int i = 0; i < nsrc; ++i) {
        const Array& lst = lists().list(src[i]);
        rows = lst.size() > rows ? lst.size() : rows;
        any_complex = any_complex || lst.dtype() == Dtype::kComplex;
    }
    if (rows == 0) {
        return fail(p, "List is empty");
    }
    if (rows > matops::kMaxRowElems) {
        return fail(p, "Matrix too large");
    }
    if (any_complex && number_mode() == NumberMode::kReal) {
        return fail(p, "Non-real result");
    }
    Array* out = claim_temp(p);
    if (out == nullptr) {
        return kBad;
    }
    out->clear();
    if (!out->set_dtype(any_complex ? Dtype::kComplex : Dtype::kDouble) ||
        !out->resize(rows, nsrc)) {
        return fail(p, "Out of matrix memory");
    }
    for (int c = 0; c < nsrc; ++c) {
        const Array& lst = lists().list(src[c]);
        for (int r = 0; r < rows; ++r) {
            const Complex v = r < lst.size() ? lst.cget(r) : Complex(0.0);
            if (any_complex) {
                out->cset(r, c, v);
            } else {
                out->set(r, c, v.re);
            }
        }
    }
    Value v;
    v.is_matrix = true;
    v.m = out;
    return v;
}

Value parse_primary(P& p) {
    skip_ws(p);

    // Matrix literal (4D.14): "[[" opens a row list, not an [X] token.
    if (p.s[0] == '[' && p.s[1] == '[') {
        return parse_matrix_literal(p);
    }

    const int slot = matrix_token_at(p.s);
    if (slot >= 0) {
        p.s += 3;
        const Array& m = matrices().matrix(slot);
        if (m.size() == 0) {
            return fail(p, "Matrix is empty");
        }
        // REAL mode never touches a complex matrix (4D.25, mirroring
        // Batch 1's strict complex-list rule) — even ops with real
        // results error, so nothing silently reads real parts.
        if (m.dtype() == Dtype::kComplex && number_mode() == NumberMode::kReal) {
            return fail(p, "Non-real result");
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
            int ri = 0;
            int ci = 0;
            if (r.is_matrix || c.is_matrix || !real_int(r.s, &ri) || !real_int(c.s, &ci)) {
                return fail(p, "Expected (row, col)");
            }
            if (ri < 1 || ri > m.dim(0) || ci < 1 || ci > m.dim(1)) {
                return fail(p, "Index out of range");
            }
            Value v;
            v.s = m.cget(ri - 1, ci - 1);  // Promotes a real element
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
        // MatAns: the last matrix result as a typed token (4D.14).
        if (std::strncmp(p.s, "matans", 6) == 0 && !ident_char(p.s[6])) {
            p.s += 6;
            if (g_mresult.size() == 0) {
                return fail(p, "No matrix result");
            }
            if (g_mresult.dtype() == Dtype::kComplex && number_mode() == NumberMode::kReal) {
                return fail(p, "Non-real result");
            }
            Value v;
            v.is_matrix = true;
            v.m = &g_mresult;
            return v;
        }
        if (std::strncmp(p.s, "list2mat", 8) == 0 && p.s[8] == '(') {
            p.s += 8;
            return parse_list2mat(p);
        }
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
        int pe = 0;
        if (e.is_matrix || !real_int(e.s, &pe)) {
            return fail(p, "Bad matrix exponent");
        }
        Array* out = claim_temp(p);
        if (out == nullptr) {
            return kBad;
        }
        Value v;
        v.is_matrix = true;
        v.m = out;
        if (!matops::power(*base.m, pe, *out, &p.err)) {
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
    if (base.s.im == 0 && e.s.im == 0) {
        v.s = Complex(std::pow(base.s.re, e.s.re));  // Real fast path, unchanged semantics
    } else {
        v.s = c_pow(base.s, e.s);
    }
    return v;
}

// RAII so every early return unwinds the count; parse_term/parse_expr call
// parse_unary repeatedly in a loop, and those siblings must not accumulate
// (the same reason complexexpr's guard is shaped this way).
struct DepthGuard {
    P& p;
    explicit DepthGuard(P& q) : p(q) { ++p.depth; }
    ~DepthGuard() { --p.depth; }
    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;
    DepthGuard(DepthGuard&&) = delete;
    DepthGuard& operator=(DepthGuard&&) = delete;
    bool too_deep() const { return p.depth > kMaxParseDepth; }
};

Value parse_unary(P& p) {
    // One guard per cycle of parse_expr -> parse_term -> parse_unary ->
    // parse_power, placed here because it is the single point every level
    // passes through exactly once (D48).
    const DepthGuard guard(p);
    if (guard.too_deep()) {
        return fail(p, "Too deeply nested");
    }
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
    const Complex k = a.is_matrix ? (divide ? Complex(1.0) / b.s : b.s) : a.s;
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
    } else if (len == 1 && rhs[0] >= 'a' && rhs[0] <= 'z' && rhs[0] != 'e' && rhs[0] != 'i') {
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

    // Involves matrices at all? [X] tokens, matrix literals `[[`,
    // identity()/list2mat() (matrix producers without matrix input),
    // or the MatAns token (4D.14).
    if (!contains_matrix_token(body) && !contains_call(body, "identity") &&
        !contains_call(body, "list2mat") && !contains_call(body, "mat2list") &&
        !contains_ident(body, "matans") && std::strstr(body, "[[") == nullptr) {
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

    // mat2list([X], l1, l2, ...) (4D.12): unpack matrix columns into
    // the listed targets. Side-effecting, so whole-expression only.
    {
        const char* inner = nullptr;
        size_t inner_len = 0;
        if (whole_call(body, "mat2list", &inner, &inner_len)) {
            res.kind = Kind::kError;
            if (store.valid) {
                res.error = "mat2list must stand alone";
                return res;
            }
            char args[kMaxLen];
            if (inner_len >= sizeof(args)) {
                res.error = "Expression too long";
                return res;
            }
            std::memcpy(args, inner, inner_len);
            args[inner_len] = 0;
            // First arg: a matrix-valued expression up to the first
            // top-level comma; the rest are l1-l6 targets.
            int depth = 0;
            char* comma = nullptr;
            for (char* q = args; *q != 0; ++q) {
                if (*q == '(' || *q == '[' || *q == '{') {
                    ++depth;
                } else if (*q == ')' || *q == ']' || *q == '}') {
                    --depth;
                } else if (*q == ',' && depth == 0) {
                    comma = q;
                    break;
                }
            }
            if (comma == nullptr) {
                res.error = "mat2list needs ([A], l1, ...)";
                return res;
            }
            *comma = 0;
            Value v;
            if (!eval_matrix_body(args, &v, &res.error)) {
                release_temps();
                return res;
            }
            int targets[6];
            int nt = 0;
            const char* q = comma + 1;
            while (*q != 0) {
                while (*q == ' ') {
                    ++q;
                }
                if (q[0] != 'l' || q[1] < '1' || q[1] > '6') {
                    release_temps();
                    res.error = "mat2list targets are l1-l6";
                    return res;
                }
                if (nt >= 6) {
                    release_temps();
                    res.error = "mat2list: too many lists";
                    return res;
                }
                targets[nt++] = q[1] - '1';
                q += 2;
                while (*q == ' ') {
                    ++q;
                }
                if (*q == ',') {
                    ++q;
                } else if (*q != 0) {
                    release_temps();
                    res.error = "Syntax error";
                    return res;
                }
            }
            if (nt == 0) {
                release_temps();
                res.error = "mat2list needs ([A], l1, ...)";
                return res;
            }
            const int rows = v.m->dim(0);
            const int cols = v.m->dim(1);
            const bool cplx = v.m->dtype() == Dtype::kComplex;
            for (int t = 0; t < nt; ++t) {
                if (t >= cols) {
                    break;  // More targets than columns: extras untouched
                }
                Array& dst = lists().list(targets[t]);
                dst.clear();
                if (!dst.set_dtype(cplx ? Dtype::kComplex : Dtype::kDouble) || !dst.resize(rows)) {
                    release_temps();
                    res.error = "Out of list memory";
                    return res;
                }
                for (int r = 0; r < rows; ++r) {
                    if (cplx) {
                        dst.cset(r, v.m->cget(r, t));
                    } else {
                        dst.set(r, v.m->get(r, t));
                    }
                }
                res.lists_mask = static_cast<uint8_t>(res.lists_mask | (1U << targets[t]));
            }
            release_temps();
            res.kind = Kind::kText;
            res.error = nullptr;
            std::snprintf(g_ctext, sizeof(g_ctext), "Done (%d list%s)", nt < cols ? nt : cols,
                          (nt < cols ? nt : cols) == 1 ? "" : "s");
            res.text = g_ctext;
            return res;
        }
    }

    // Whole-expression list forms: dim(...) and eigenvals(...).
    for (int which = 0; which < 2; ++which) {
        const char* inner = nullptr;
        size_t inner_len = 0;
        if (which == 0) {
            if (!whole_call(body, "dim", &inner, &inner_len)) {
                continue;
            }
        } else {
            // eigenvals(...) or its alias eig(...).
            if (!whole_call(body, "eigenvals", &inner, &inner_len) &&
                !whole_call(body, "eig", &inner, &inner_len)) {
                continue;
            }
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
        if (which == 0) {
            const bool ok = g_lresult.resize(2);
            release_temps();
            if (!ok) {
                res.error = "Out of list memory";
                return res;
            }
            g_lresult.set(0, v.m->dim(0));
            g_lresult.set(1, v.m->dim(1));
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

        // eigenvals(): full spectrum (Phase 4C, D30/P4-7). All-real
        // stays a Kind::kList (unchanged, storable into l1..l6); a
        // complex-conjugate pair formats as unstorable text instead.
        Complex ceig[matops::kMaxEigen];
        int ccount = 0;
        const bool ok = matops::eigenvalues_complex(*v.m, ceig, &ccount, &res.error);
        release_temps();
        if (!ok) {
            return res;
        }
        bool all_real = true;
        for (int i = 0; i < ccount; ++i) {
            if (!ceig[i].is_real()) {
                all_real = false;
                break;
            }
        }
        if (store.matrix >= 0 || store.var >= 0) {
            res.kind = Kind::kError;
            res.error = "Store target mismatch";
            return res;
        }
        if (!all_real) {
            if (store.list >= 0) {
                res.kind = Kind::kError;
                res.error = "Complex results can't be stored";
                return res;
            }
            size_t pos = 0;
            g_ctext[pos++] = '{';
            for (int i = 0; i < ccount; ++i) {
                char num[24];
                format_complex(ceig[i], NumberMode::kRectangular, num, sizeof(num));
                const size_t need = std::strlen(num) + (i > 0 ? 1 : 0);
                // Room for the number plus "...}" + NUL in the worst case.
                if (pos + need + 5 > sizeof(g_ctext)) {
                    g_ctext[pos++] = kEllipsisGlyph;
                    break;
                }
                if (i > 0) {
                    g_ctext[pos++] = ',';
                }
                std::memcpy(g_ctext + pos, num, std::strlen(num));
                pos += std::strlen(num);
            }
            g_ctext[pos++] = '}';
            g_ctext[pos] = 0;
            res.kind = Kind::kText;
            res.error = nullptr;
            res.text = g_ctext;
            return res;
        }

        calc_t real_eig[matops::kMaxEigen];
        for (int i = 0; i < ccount; ++i) {
            real_eig[i] = ceig[i].re;
        }
        if (!g_lresult.resize(ccount)) {
            res.kind = Kind::kError;
            res.error = "Out of list memory";
            return res;
        }
        g_lresult.write_range(0, ccount, real_eig);
        res.kind = Kind::kList;
        res.error = nullptr;
        res.list = &g_lresult;
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
        if (!std::isfinite(v.s.re) || !std::isfinite(v.s.im)) {
            res.kind = Kind::kError;
            res.error = "Undefined result";
            return res;
        }
        if (!v.s.is_real() && number_mode() == NumberMode::kReal) {
            res.kind = Kind::kError;
            res.error = "Non-real result";  // D30 precedent
            return res;
        }
        res.kind = Kind::kScalar;
        res.scalar.ok = true;
        res.scalar.value = v.s.re;
        if (v.s.is_real()) {
            engine().vars().set_real(Variables::kAns, v.s.re);
            if (store.var >= 0) {
                engine().vars().set_real(store.var, v.s.re);
                res.scalar.stored_var = store.var;
            }
        } else {
            // Complex commit (4D.25): Ans/store hold the full value;
            // real-only readers error on them (D37).
            res.scalar_complex = true;
            res.cvalue = v.s;
            engine().vars().set_complex(Variables::kAns, v.s.re, v.s.im);
            if (store.var >= 0) {
                engine().vars().set_complex(store.var, v.s.re, v.s.im);
                res.scalar.stored_var = store.var;
            }
        }
        return res;
    }

    if (store.list >= 0 || store.var >= 0) {
        release_temps();
        res.kind = Kind::kError;
        res.error = "Store target mismatch";
        return res;
    }
    // A complex matrix value can arise in REAL mode without any [X]
    // being complex (e.g. i*[B]) — gate the result too.
    if (v.m->dtype() == Dtype::kComplex && number_mode() == NumberMode::kReal) {
        release_temps();
        res.kind = Kind::kError;
        res.error = "Non-real result";
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

Array& mat_ans_mutable() {
    return g_mresult;
}

namespace {

using RealCellFmt = int (*)(calc_t, char*, size_t);
using CplxCellFmt = int (*)(const Complex&, NumberMode, char*, size_t);

// Fraction cell for >Frac (4D.2, extended to matrices): p/q when a tight
// fraction exists (den <= 10000), else the compact decimal fallback.
int cell_fraction(calc_t v, char* buf, size_t cap) {
    if (frac::format_fraction(v, 10000, buf, cap)) {
        return static_cast<int>(std::strlen(buf));
    }
    return format_number_compact(v, buf, cap);
}

// Shared body: renders "[[a,b][c,d]]" with per-cell formatters, so the
// plain and >Frac variants differ only in how real cells stringify.
// (Complex cells never convert to fractions — they keep the compact form.)
void format_matrix_impl(const Array& m, char* buf, size_t buf_len, RealCellFmt real_fmt,
                        CplxCellFmt cplx_fmt) {
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
    const bool cplx = m.dtype() == Dtype::kComplex;

    // Near-zero cleanup: find the largest cell magnitude, then display any
    // cell more than ~12 orders smaller as a clean "0". This snaps the
    // floating-point roundoff that shows up in e.g. [A]^-1*[A] (off-diagonal
    // 2.22e-16) to an exact identity instead of scientific noise. Relative
    // to the matrix's own scale, so a genuinely tiny-magnitude matrix is
    // preserved (its own max sets the threshold). A cell exactly 0 already
    // prints "0"; this just extends that to sub-tolerance roundoff.
    calc_t maxmag = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const calc_t mag = cplx ? m.cget(r, c).modulus() : std::fabs(m.get(r, c));
            maxmag = mag > maxmag ? mag : maxmag;
        }
    }
    const calc_t zero_tol = maxmag * 1e-12;

    for (int r = 0; r < rows && !truncated; ++r) {
        buf[pos++] = '[';
        for (int c = 0; c < cols; ++c) {
            char num[48];
            const calc_t mag = cplx ? m.cget(r, c).modulus() : std::fabs(m.get(r, c));
            if (maxmag > 0 && mag <= zero_tol) {
                num[0] = '0';
                num[1] = 0;
            } else if (cplx) {
                cplx_fmt(m.cget(r, c), number_mode(), num, sizeof(num));
            } else {
                real_fmt(m.get(r, c), num, sizeof(num));
            }
            const size_t need = std::strlen(num) + (c > 0 ? 1 : 0);
            // Room for the number plus "...]]" + NUL in the worst case
            if (pos + need + 7 > buf_len) {
                buf[pos++] = kEllipsisGlyph;
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

}  // namespace

void format_matrix(const Array& m, char* buf, size_t buf_len) {
    format_matrix_impl(m, buf, buf_len, format_number_compact, format_complex_compact);
}

void format_matrix_frac(const Array& m, char* buf, size_t buf_len) {
    format_matrix_impl(m, buf, buf_len, cell_fraction, format_complex_compact);
}

}  // namespace math::matexpr
