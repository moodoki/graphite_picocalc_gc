#include "math/engine.hpp"

#include <cctype>
#include <cstring>

#include "math/functions.hpp"

extern "C" {
#include "tinyexpr.h"
}

namespace math {

namespace {

constexpr size_t kMaxExpr = 256;

// Preprocess into `out`:
//  - lowercase (tinyexpr identifiers are lowercase; vars are A-Z)
//  - postfix factorial: <primary>! -> fac(<primary>)
// Returns false if the expression is too long.
bool preprocess(const char* in, char* out, size_t out_len) {
    char tmp[kMaxExpr];
    size_t n = 0;
    for (const char* p = in; *p != 0; ++p) {
        if (n + 1 >= sizeof(tmp)) {
            return false;
        }
        tmp[n++] = static_cast<char>(std::tolower(*p));
    }
    tmp[n] = 0;

    // Factorial rewrite, innermost-last: repeat until no '!' remains.
    while (true) {
        char* bang = std::strchr(tmp, '!');
        if (bang == nullptr) {
            break;
        }
        // Find the start of the primary that precedes '!'.
        char* end = bang;  // One past the primary's last char
        char* s = bang - 1;
        while (s >= tmp && *s == ' ') {
            --s;
        }
        if (s < tmp) {
            return false;  // "!" with nothing before it
        }
        if (*s == ')') {
            int depth = 0;
            while (s >= tmp) {
                if (*s == ')') {
                    ++depth;
                } else if (*s == '(') {
                    if (--depth == 0) {
                        break;
                    }
                }
                --s;
            }
            if (depth != 0) {
                return false;
            }
            // Include a preceding function name, e.g. "fac(3)!"
            while (s > tmp && (std::isalnum(s[-1]) != 0 || s[-1] == '_')) {
                --s;
            }
        } else if (std::isalnum(*s) != 0 || *s == '.') {
            while (s > tmp && (std::isalnum(s[-1]) != 0 || s[-1] == '.' || s[-1] == '_')) {
                --s;
            }
        } else {
            return false;
        }

        // Rebuild: [0,s) + "fac(" + [s,end) + ")" + [bang+1,...)
        char rebuilt[kMaxExpr];
        const size_t head = static_cast<size_t>(s - tmp);
        const size_t prim = static_cast<size_t>(end - s);
        const size_t tail = std::strlen(bang + 1);
        if (head + 4 + prim + 1 + tail + 1 >= sizeof(rebuilt)) {
            return false;
        }
        std::memcpy(rebuilt, tmp, head);
        std::memcpy(rebuilt + head, "fac(", 4);
        std::memcpy(rebuilt + head + 4, s, prim);
        rebuilt[head + 4 + prim] = ')';
        std::memcpy(rebuilt + head + 4 + prim + 1, bang + 1, tail + 1);
        std::strcpy(tmp, rebuilt);
    }

    if (std::strlen(tmp) >= out_len) {
        return false;
    }
    std::strcpy(out, tmp);
    return true;
}

}  // namespace

calc_t& Variables::operator[](char name) {
    if (name >= 'a' && name <= 'z') {
        return vars[name - 'a'];
    }
    if (name >= 'A' && name <= 'Z') {
        return vars[name - 'A'];
    }
    return vars[kAns];
}

Engine::Engine() = default;

EvalResult Engine::eval_internal(const char* expr) {
    EvalResult res;

    char processed[kMaxExpr];
    if (!preprocess(expr, processed, sizeof(processed))) {
        res.error = "Expression too long";
        return res;
    }

    // Variable + function bindings. Single-letter names a-z map into
    // vars_; user lookups shadow tinyexpr builtins (trig, log).
    static const char* kNames = "abcdefghijklmnopqrstuvwxyz";
    te_variable lookup[26 + 2 + 17];
    int li = 0;
    static char names[26][2];
    for (int i = 0; i < 26; ++i) {
        names[i][0] = kNames[i];
        names[i][1] = 0;
        lookup[li++] = {names[i], &vars_.vars[i], TE_VARIABLE, nullptr};
    }
    lookup[li++] = {"theta", &vars_.vars[Variables::kTheta], TE_VARIABLE, nullptr};
    lookup[li++] = {"ans", &vars_.vars[Variables::kAns], TE_VARIABLE, nullptr};

    lookup[li++] = {"sin", reinterpret_cast<const void*>(fn::sin_am), TE_FUNCTION1, nullptr};
    lookup[li++] = {"cos", reinterpret_cast<const void*>(fn::cos_am), TE_FUNCTION1, nullptr};
    lookup[li++] = {"tan", reinterpret_cast<const void*>(fn::tan_am), TE_FUNCTION1, nullptr};
    lookup[li++] = {"asin", reinterpret_cast<const void*>(fn::asin_am), TE_FUNCTION1, nullptr};
    lookup[li++] = {"acos", reinterpret_cast<const void*>(fn::acos_am), TE_FUNCTION1, nullptr};
    lookup[li++] = {"atan", reinterpret_cast<const void*>(fn::atan_am), TE_FUNCTION1, nullptr};
    lookup[li++] = {"log", reinterpret_cast<const void*>(fn::log10_ti), TE_FUNCTION1, nullptr};
    lookup[li++] = {"ln", reinterpret_cast<const void*>(fn::ln_nat), TE_FUNCTION1, nullptr};
    lookup[li++] = {"fac", reinterpret_cast<const void*>(fn::factorial), TE_FUNCTION1, nullptr};
    lookup[li++] = {"ncr", reinterpret_cast<const void*>(fn::ncr), TE_FUNCTION2, nullptr};
    lookup[li++] = {"npr", reinterpret_cast<const void*>(fn::npr), TE_FUNCTION2, nullptr};
    lookup[li++] = {"rand", reinterpret_cast<const void*>(fn::rand01), TE_FUNCTION0, nullptr};
    lookup[li++] = {"round", reinterpret_cast<const void*>(fn::round_n), TE_FUNCTION2, nullptr};
    lookup[li++] = {"min", reinterpret_cast<const void*>(fn::min2), TE_FUNCTION2, nullptr};
    lookup[li++] = {"max", reinterpret_cast<const void*>(fn::max2), TE_FUNCTION2, nullptr};
    lookup[li++] = {"deg", reinterpret_cast<const void*>(fn::deg), TE_FUNCTION1, nullptr};
    lookup[li++] = {"rad", reinterpret_cast<const void*>(fn::rad), TE_FUNCTION1, nullptr};

    int err = 0;
    te_expr* compiled = te_compile(processed, lookup, li, &err);
    if (compiled == nullptr) {
        res.error = "Syntax error";
        return res;
    }
    res.value = te_eval(compiled);
    te_free(compiled);
    res.ok = true;
    return res;
}

EvalResult Engine::evaluate(const char* expr) {
    // Store operator (D1): "expr->A" / "expr->theta". Split on the last
    // "->" whose right side is a bare variable name.
    char body[kMaxExpr];
    std::strncpy(body, expr, sizeof(body) - 1);
    body[sizeof(body) - 1] = 0;

    int store_index = -1;
    char* arrow = nullptr;
    for (char* p = body; (p = std::strstr(p, "->")) != nullptr; p += 2) {
        arrow = p;
    }
    if (arrow != nullptr) {
        const char* rhs = arrow + 2;
        while (*rhs == ' ') {
            ++rhs;
        }
        char name[8] = {};
        size_t ni = 0;
        while (ni < sizeof(name) - 1 && std::isalpha(rhs[ni]) != 0) {
            name[ni] = static_cast<char>(std::tolower(rhs[ni]));
            ++ni;
        }
        const char* after = rhs + ni;
        while (*after == ' ') {
            ++after;
        }
        if (*after == 0 && ni > 0) {
            if (ni == 1) {
                store_index = name[0] - 'a';
            } else if (std::strcmp(name, "theta") == 0) {
                store_index = Variables::kTheta;
            }
        }
        if (store_index >= 0) {
            *arrow = 0;  // Evaluate only the left side
        }
    }

    EvalResult res = eval_internal(body);
    if (res.ok) {
        vars_.ans() = res.value;
        if (store_index >= 0) {
            vars_.vars[store_index] = res.value;
            res.stored_var = store_index;
        }
    }
    return res;
}

EvalResult Engine::evaluate_at(const char* expr, calc_t x_val) {
    const calc_t saved = vars_.vars['x' - 'a'];
    vars_.vars['x' - 'a'] = x_val;
    EvalResult res = eval_internal(expr);
    vars_.vars['x' - 'a'] = saved;
    return res;
}

Engine& engine() {
    static Engine instance;
    return instance;
}

}  // namespace math
