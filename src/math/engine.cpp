#include "math/engine.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>

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
        const char* bang = std::strchr(tmp, '!');
        if (bang == nullptr) {
            break;
        }
        // Find the start of the primary that precedes '!'.
        char const* end = bang;  // One past the primary's last char
        char const* s = bang - 1;
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
        const auto head = static_cast<int>(s - tmp);
        const auto prim = static_cast<int>(end - s);
        const int wrote = std::snprintf(rebuilt, sizeof(rebuilt), "%.*sfac(%.*s)%s", head, tmp,
                                        prim, s, bang + 1);
        if (wrote < 0 || wrote >= static_cast<int>(sizeof(rebuilt))) {
            return false;
        }
        std::memcpy(tmp, rebuilt, static_cast<size_t>(wrote) + 1);
    }

    const size_t len = std::strlen(tmp);
    if (len >= out_len) {
        return false;
    }
    std::memcpy(out, tmp, len + 1);
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

namespace {

// Build the variable + function binding table. Names a-z bind to
// vars.vars[] (stable addresses); the extended library shadows tinyexpr
// builtins where semantics differ (log = base 10, angle-aware trig).
// The letter 'e' is NOT bound: tinyexpr checks this lookup before its
// builtins, so binding it would shadow Euler's constant (like `pi`,
// `e` must reach the builtin table). Variable E is reserved.
// The lookup array is only needed during te_compile — the pointers it
// captures (into vars and static function code) outlive it. Returns the
// number of entries written; `lookup` must hold at least kLookupCount.
constexpr int kLookupCount = 25 + 2 + 17;

int build_lookup(Variables& vars, te_variable* lookup) {
    static char names[26][2];
    int li = 0;
    for (int i = 0; i < 26; ++i) {
        if (i == 'e' - 'a') {
            continue;  // 'e' = Euler's constant, not a variable
        }
        names[i][0] = static_cast<char>('a' + i);
        names[i][1] = 0;
        lookup[li++] = {names[i], &vars.vars[i], TE_VARIABLE, nullptr};
    }
    lookup[li++] = {"theta", &vars.vars[Variables::kTheta], TE_VARIABLE, nullptr};
    lookup[li++] = {"ans", &vars.vars[Variables::kAns], TE_VARIABLE, nullptr};

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
    return li;
}

}  // namespace

Engine::Engine() = default;

EvalResult Engine::eval_internal(const char* expr) {
    EvalResult res;

    char processed[kMaxExpr];
    if (!preprocess(expr, processed, sizeof(processed))) {
        res.error = "Expression too long";
        return res;
    }

    te_variable lookup[kLookupCount];
    const int li = build_lookup(vars_, lookup);

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

void* Engine::compile(const char* expr) {
    char processed[kMaxExpr];
    if (!preprocess(expr, processed, sizeof(processed))) {
        return nullptr;
    }
    te_variable lookup[kLookupCount];
    const int li = build_lookup(vars_, lookup);
    int err = 0;
    return te_compile(processed, lookup, li, &err);
}

calc_t Engine::eval_compiled(void* handle, calc_t x_val) {
    if (handle == nullptr) {
        return std::numeric_limits<calc_t>::quiet_NaN();
    }
    vars_.vars['x' - 'a'] = x_val;
    return te_eval(static_cast<te_expr*>(handle));
}

void Engine::free_compiled(void* handle) {
    if (handle != nullptr) {
        te_free(static_cast<te_expr*>(handle));
    }
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
        if (store_index == 'e' - 'a') {
            EvalResult res;
            res.error = "E is reserved (Euler's e)";
            return res;
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
