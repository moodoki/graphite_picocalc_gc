#include "math/complex_expr.hpp"

#include <cctype>
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

struct P {
    const char* s = nullptr;
    const char* err = nullptr;
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

const CFn kFns[] = {
    {"sqrt", c_sqrt}, {"exp", c_exp},   {"ln", c_ln},      {"sin", c_sin},    {"cos", c_cos},
    {"tan", c_tan},   {"asin", c_asin}, {"acos", c_acos},  {"atan", c_atan},  {"abs", do_abs},
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

    char span[kMaxLen];
    const auto len = static_cast<size_t>(p.s - start);
    if (len == 0 || len >= sizeof(span)) {
        return fail(p, "Syntax error");
    }
    std::memcpy(span, start, len);
    span[len] = 0;
    calc_t v = 0;
    if (!eval_field(span, &v)) {
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

Complex parse_unary(P& p) {
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

Result evaluate(const char* input) {
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

    P p;
    p.s = body;
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

    if (store_target >= 0 && !v.is_real()) {
        res.error = "Complex results can't be stored";
        return res;
    }

    res.ok = true;
    res.value = v;
    res.stored_var = store_target;
    return res;
}

}  // namespace math::complexexpr
