#include "math/complex_expr.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "math/engine.hpp"

namespace math::complexexpr {

namespace {

constexpr size_t kMaxLen = 160;

bool ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Left boundary for a standalone `i` token: anything except a letter or
// underscore. Digits ARE allowed immediately before — that's the "2i"
// shorthand (spec §5.3).
bool i_left_ok(char prev) {
    return std::isalpha(static_cast<unsigned char>(prev)) == 0 && prev != '_';
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

// Parse-nesting cap. Two recursion cycles run through this parser and both
// pass through parse_unary, which is why the guard lives there:
//   1. paren/function nesting — parse_primary -> parse_expr -> parse_term
//      -> parse_unary
//   2. right-associative '^' — parse_power -> parse_unary, with no
//      parentheses involved, so `2^2^2^...` nests once per caret.
// Cycle 2 is why this needs a real counter and not the paren-depth scan
// tinyexpr gets (D47's kMaxParseDepth): a string pre-scan cannot see it.
// tinyexpr has no such cycle — its `factor` builds right-associativity
// iteratively via an insertion pointer.
//
// The limit itself is the caller's, not the parser's — see kMaxParseDepth /
// kMaxParseDepthNested in the header for the measurement.

struct P {
    const char* s = nullptr;
    const char* err = nullptr;
    int depth = 0;
    int max_depth = kMaxParseDepth;
};

void skip_ws(P& p) {
    while (*p.s == ' ') {
        ++p.s;
    }
}

Complex parse_expr(P& p);
Complex parse_unary(P& p);

const Complex kBad;

Complex fail(P& p, const char* msg) {
    if (p.err == nullptr) {
        p.err = msg;
    }
    return kBad;
}

struct CFn {
    const char* name;
    Complex (*fn)(const Complex&);
};

Complex do_abs(const Complex& z) {
    return {c_abs(z)};
}
Complex do_arg(const Complex& z) {
    return {c_arg(z)};
}
Complex do_real(const Complex& z) {
    return {c_real(z)};
}
Complex do_imag(const Complex& z) {
    return {c_imag(z)};
}

// Angle-mode wrappers. complex.cpp's c_sin/c_asin/... are deliberately pure
// math — a Complex sine must not depend on global UI state — so the DEG/RAD
// scaling lives here at the evaluator boundary, mirroring exactly what
// functions.cpp's rad()/deg() do for the real path. Without these the complex
// evaluator answered every trig call in radians, so DEGREE mode was silently
// ignored on the home screen whenever Number mode was not REAL.
//
// The whole complex argument is scaled, not just its real part (TI-89's
// behavior): for a real-valued input this reduces exactly to the real path,
// which is the property that matters — the two evaluators must not disagree
// about sin(30).
constexpr calc_t kDegPerRad = 180.0;
constexpr calc_t kPiConst = 3.14159265358979323846;

Complex to_radians(const Complex& z) {
    return angle_mode() == AngleMode::kDegrees ? z * Complex(kPiConst / kDegPerRad) : z;
}
Complex from_radians(const Complex& z) {
    return angle_mode() == AngleMode::kDegrees ? z * Complex(kDegPerRad / kPiConst) : z;
}

Complex m_sin(const Complex& z) {
    return c_sin(to_radians(z));
}
Complex m_cos(const Complex& z) {
    return c_cos(to_radians(z));
}
Complex m_tan(const Complex& z) {
    return c_tan(to_radians(z));
}
Complex m_asin(const Complex& z) {
    return from_radians(c_asin(z));
}
Complex m_acos(const Complex& z) {
    return from_radians(c_acos(z));
}
Complex m_atan(const Complex& z) {
    return from_radians(c_atan(z));
}

const CFn kFns[] = {
    {"sqrt", c_sqrt}, {"exp", c_exp},   {"ln", c_ln},      {"sin", m_sin},    {"cos", m_cos},
    {"tan", m_tan},   {"asin", m_asin}, {"acos", m_acos},  {"atan", m_atan},  {"abs", do_abs},
    {"arg", do_arg},  {"conj", c_conj}, {"real", do_real}, {"imag", do_imag},
};

// Anything that isn't `i` or one of kFns: an identifier (optionally
// followed by a balanced-paren call) or a numeric literal, handed to
// eval_field as an opaque real span — the scalar-subterm technique
// math::matexpr uses for its own non-matrix primaries.
Complex parse_scalar_span(P& p) {
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
        // "2i" shorthand: an imaginary literal, no operator needed.
        if (*p.s == 'i' && !ident_char(p.s[1])) {
            char span[32];
            const auto len = static_cast<size_t>(p.s - start);
            if (len == 0 || len >= sizeof(span)) {
                return fail(p, "Syntax error");
            }
            std::memcpy(span, start, len);
            span[len] = 0;
            ++p.s;
            calc_t v = 0;
            if (!eval_field(span, &v)) {
                return fail(p, "Syntax error");
            }
            return {0.0, v};
        }
    } else if (std::isalpha(static_cast<unsigned char>(*p.s)) != 0) {
        while (ident_char(*p.s)) {
            ++p.s;
        }
        // A bare variable token holding a complex value resolves here —
        // eval_field is real-only and would reject it (4D.15). Real-
        // valued variables keep taking the eval_field path below.
        if (*p.s != '(') {
            const auto ilen = static_cast<size_t>(p.s - start);
            int slot = -1;
            if (ilen == 1 && start[0] >= 'a' && start[0] <= 'z') {
                slot = start[0] - 'a';
            } else if (ilen == 3 && std::strncmp(start, "ans", 3) == 0) {
                slot = Variables::kAns;
            } else if (ilen == 5 && std::strncmp(start, "theta", 5) == 0) {
                slot = Variables::kTheta;
            }
            const Variables& vars = engine().vars();
            if (slot >= 0 && vars.is_complex(slot)) {
                return {vars.vars[slot], vars.imag[slot]};
            }
        }
        if (*p.s == '(') {
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
    // Engine::evaluate -> eval_internal -> preprocess — roughly 1.2 KB of
    // stack, and it sat at the *leaf* of this parser's recursion, which is
    // the deepest point on the stack. Four nested parens overran core 0
    // because of it (HW 2026-08-08, faulting in preprocess's prologue).
    // strtod is what tinyexpr would have used for these anyway.
    if (std::isdigit(static_cast<unsigned char>(*start)) != 0 || *start == '.') {
        char* end = nullptr;
        const double d = std::strtod(start, &end);
        if (end == p.s) {
            return {static_cast<calc_t>(d)};
        }
    }

    // Static for the same reason preprocess's buffers are: this is the leaf
    // of the parser's recursion, so its frame is paid at maximum depth.
    // parse_scalar_span never nests (it consumes a terminal span and calls
    // nothing that re-enters the parser), so one copy is enough.
    static char span[kMaxLen];
    std::memcpy(span, start, len);
    span[len] = 0;
    calc_t v = 0;
    if (!eval_field(span, &v)) {
        // eval_field is real-only; a complex variable inside an opaque
        // span (e.g. fac(a) with a = 2i) is a pointed error, not a
        // syntax error (4D.15).
        if (refs_complex_var(span)) {
            return fail(p, "Non-real variable");
        }
        return fail(p, "Syntax error");
    }
    return {v};
}

Complex parse_primary(P& p) {
    skip_ws(p);

    if (*p.s == '(') {
        ++p.s;
        const Complex v = parse_expr(p);
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

    // Standalone `i`: parse_primary is only ever entered at a token
    // start (unary/term/expr consume operators before recursing), so
    // the left boundary is already guaranteed here.
    if (*p.s == 'i' && !ident_char(p.s[1])) {
        ++p.s;
        return {0.0, 1.0};
    }

    if (std::isalpha(static_cast<unsigned char>(*p.s)) != 0) {
        for (const CFn& f : kFns) {
            const size_t nl = std::strlen(f.name);
            if (std::strncmp(p.s, f.name, nl) == 0 && p.s[nl] == '(') {
                p.s += nl + 1;
                const Complex arg = parse_expr(p);
                if (p.err != nullptr) {
                    return kBad;
                }
                skip_ws(p);
                if (*p.s != ')') {
                    return fail(p, "Syntax error");
                }
                ++p.s;
                return f.fn(arg);
            }
        }
    }

    return parse_scalar_span(p);
}

Complex parse_power(P& p) {
    const Complex base = parse_primary(p);
    if (p.err != nullptr) {
        return kBad;
    }
    skip_ws(p);
    if (*p.s != '^') {
        return base;
    }
    ++p.s;
    skip_ws(p);
    const Complex exp = parse_unary(p);  // Right-assoc via recursion, matches matexpr
    if (p.err != nullptr) {
        return kBad;
    }
    return c_pow(base, exp);
}

// RAII so every early return unwinds the count; parse_term/parse_expr call
// parse_unary repeatedly in a loop, and those siblings must not accumulate.
struct DepthGuard {
    P& p;
    explicit DepthGuard(P& q) : p(q) { ++p.depth; }
    ~DepthGuard() { --p.depth; }
    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;
    DepthGuard(DepthGuard&&) = delete;
    DepthGuard& operator=(DepthGuard&&) = delete;
    bool too_deep() const { return p.depth > p.max_depth; }
};

Complex parse_unary(P& p) {
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
    Complex v = parse_power(p);
    if (p.err != nullptr || !neg) {
        return v;
    }
    return -v;
}

Complex parse_term(P& p) {
    Complex left = parse_unary(p);
    while (p.err == nullptr) {
        skip_ws(p);
        if (*p.s != '*' && *p.s != '/') {
            break;
        }
        const bool divide = *p.s == '/';
        ++p.s;
        const Complex right = parse_unary(p);
        if (p.err != nullptr) {
            return kBad;
        }
        left = divide ? left / right : left * right;
    }
    return left;
}

Complex parse_expr(P& p) {
    Complex left = parse_term(p);
    while (p.err == nullptr) {
        skip_ws(p);
        if ((*p.s != '+' && *p.s != '-') || p.s[1] == '>') {
            break;  // "->" is the store arrow, not subtraction
        }
        const bool subtract = *p.s == '-';
        ++p.s;
        const Complex right = parse_term(p);
        if (p.err != nullptr) {
            return kBad;
        }
        left = subtract ? left - right : left + right;
    }
    return left;
}

// Store target: same rules as Engine::evaluate (D1/D11/D19), plus the
// new `i` reservation (D30). Returns -2 with *err untouched when rhs
// isn't a store target at all (caller leaves the arrow for parse_expr
// to reject); *err set means a pointed reserved-word/case error.
int parse_store_target(const char* rhs, const char** err) {
    if (std::strcmp(rhs, "theta") == 0) {
        return Variables::kTheta;
    }
    if (std::strlen(rhs) == 1 && rhs[0] >= 'A' && rhs[0] <= 'Z') {
        *err = "Variables are lowercase a-z";
        return -2;
    }
    if (std::strlen(rhs) != 1 || rhs[0] < 'a' || rhs[0] > 'z') {
        return -2;
    }
    if (rhs[0] == 'e') {
        *err = "e is reserved (Euler's e)";
        return -2;
    }
    if (rhs[0] == 'i') {
        *err = "i is reserved (imaginary unit)";
        return -2;
    }
    return rhs[0] - 'a';
}

}  // namespace

bool mentions_i(const char* s) {
    for (const char* p = s; *p != 0; ++p) {
        if (*p == 'i' && i_left_ok(p == s ? ' ' : p[-1]) && !ident_char(p[1])) {
            return true;
        }
    }
    return false;
}

Result evaluate(const char* input, int max_depth) {
    Result res;
    char body[kMaxLen];
    if (!trim_into(input, body, sizeof(body))) {
        res.error = "Expression too long";
        return res;
    }
    if (body[0] == 0) {
        res.error = "Syntax error";
        return res;
    }

    // Store suffix: "-> a" / "-> theta" (rightmost arrow, matching
    // Engine::evaluate).
    char* arrow = nullptr;
    for (char* q = body; (q = std::strstr(q, "->")) != nullptr; q += 2) {
        arrow = q;
    }
    // Only a recognized target (theta / lowercase a-z) or one of the
    // two pointed reserved-word errors strips the arrow. Anything else
    // (not a store at all — e.g. "->" inside a stray comparison) is
    // left in body for parse_expr to reject as a plain syntax error,
    // matching Engine::evaluate's leniency.
    int store_target = -1;
    if (arrow != nullptr) {
        char rhs[16];
        if (trim_into(arrow + 2, rhs, sizeof(rhs))) {
            const char* err = nullptr;
            const int t = parse_store_target(rhs, &err);
            if (err != nullptr) {
                res.error = err;
                return res;
            }
            if (t >= 0) {
                store_target = t;
                *arrow = 0;
                char trimmed[kMaxLen];
                if (!trim_into(body, trimmed, sizeof(trimmed))) {
                    res.error = "Expression too long";
                    return res;
                }
                std::memcpy(body, trimmed, std::strlen(trimmed) + 1);
            }
        }
    }

    // Postfix factorial (`5!` -> `fac(5)`): the complex parser has no `!`
    // rule of its own, so share Engine's rewrite before parsing —
    // otherwise `5!` fails as a syntax error on this path (non-REAL mode
    // or an `i`-bearing expression). fac() itself resolves through
    // eval_field's real engine, same as any other scalar-span call.
    char processed[kMaxLen];
    if (!preprocess_factorial(body, processed, sizeof(processed))) {
        res.error = "Syntax error";
        return res;
    }

    P p;
    p.s = processed;
    p.max_depth = max_depth;
    const Complex v = parse_expr(p);
    if (p.err == nullptr) {
        skip_ws(p);
        if (*p.s != 0) {
            p.err = "Syntax error";
        }
    }
    if (p.err != nullptr) {
        res.error = p.err;
        return res;
    }

    // Complex values are storable since 4D.15 (widened Variables); the
    // dispatch layer commits the store, same as for real results.
    res.ok = true;
    res.value = v;
    res.stored_var = store_target;
    return res;
}

}  // namespace math::complexexpr
