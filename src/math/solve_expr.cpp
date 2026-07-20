#include "math/solve_expr.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/engine.hpp"
#include "math/numeric_solve.hpp"

namespace math::solveexpr {

namespace {

constexpr size_t kMaxLen = 256;

bool ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
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

// Split on top-level commas (paren/brace depth 0), like list_expr's
// split_args. Returns the count, or -1 on overflow.
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

// One solve(inner) call -> numeric literal in num_out.
bool eval_solve_call(const char* inner, size_t inner_len, char* num_out, size_t num_cap,
                     const char** err) {
    const char* starts[4];
    size_t lens[4];
    const int count = split_args(inner, inner_len, starts, lens, 4);
    if (count != 3 && count != 4) {
        *err = "solve needs (expr, var, guess) or (expr, var, lo, hi)";
        return false;
    }
    char arg[4][kMaxLen];
    for (int i = 0; i < count; ++i) {
        char raw[kMaxLen];
        if (lens[i] >= sizeof(raw)) {
            *err = "Expression too long";
            return false;
        }
        std::memcpy(raw, starts[i], lens[i]);
        raw[lens[i]] = 0;
        if (!trim_into(raw, arg[i], sizeof(arg[i]))) {
            *err = "Expression too long";
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
        *err = "solve var must be a-z (not e/i) or theta";
        return false;
    }

    calc_t lo = 0;
    calc_t hi = 0;
    if (!eval_field(arg[2], &lo)) {
        *err = "Bad solve bound";
        return false;
    }
    if (count == 4) {
        if (!eval_field(arg[3], &hi)) {
            *err = "Bad solve bound";
            return false;
        }
    } else {
        hi = lo;  // Guess form: numeric_solve runs Newton from it
    }

    // Equation form: split a top-level '=' into lhs/rhs.
    char* eq = nullptr;
    int depth = 0;
    for (char* q = arg[0]; *q != 0; ++q) {
        if (*q == '(' || *q == '{') {
            ++depth;
        } else if (*q == ')' || *q == '}') {
            --depth;
        } else if (*q == '=' && depth == 0) {
            eq = q;
            break;
        }
    }
    SolveResult sr;
    if (eq != nullptr) {
        *eq = 0;
        sr = numeric_solve_equation(arg[0], eq + 1, var_slot, lo, hi);
    } else {
        sr = numeric_solve(arg[0], var_slot, lo, hi);
    }
    if (!sr.converged) {
        *err = sr.error != nullptr ? sr.error : "No solution found";
        return false;
    }
    std::snprintf(num_out, num_cap, "%.17g", static_cast<double>(sr.root));
    return true;
}

}  // namespace

bool contains_solve(const char* s) {
    for (const char* p = s; *p != 0; ++p) {
        if (p > s && ident_char(p[-1])) {
            continue;
        }
        if (std::strncmp(p, "solve", 5) == 0 && p[5] == '(') {
            return true;
        }
    }
    return false;
}

bool substitute(char* buf, size_t cap, const char** err) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (char* p = buf; (p = std::strstr(p, "solve")) != nullptr; ++p) {
            if (p > buf && ident_char(p[-1])) {
                continue;
            }
            char* q = p + 5;
            if (*q != '(') {
                continue;
            }
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
                *err = "Syntax error";
                return false;
            }
            // Innermost-first: an argument still containing a solve()
            // call waits for a later pass.
            char inner_probe[kMaxLen];
            const auto alen = static_cast<size_t>(close - (q + 1));
            if (alen >= sizeof(inner_probe)) {
                *err = "Expression too long";
                return false;
            }
            std::memcpy(inner_probe, q + 1, alen);
            inner_probe[alen] = 0;
            if (contains_solve(inner_probe)) {
                continue;
            }

            char num[40];
            if (!eval_solve_call(q + 1, alen, num, sizeof(num), err)) {
                return false;
            }
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
    return true;
}

}  // namespace math::solveexpr
