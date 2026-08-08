#include "math/engine.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#include "math/catalog.hpp"

extern "C" {
#include "tinyexpr.h"
}

namespace math {

namespace {

constexpr size_t kMaxExpr = 256;

// Preprocess into `out`:
//  - postfix factorial: <primary>! -> fac(<primary>)
// Input is case-sensitive (2026-07-18, was blanket-lowercased before):
// identifiers are lowercase (sin, pi, a-z, theta, ans); uppercase input
// fails compile with the normal parse error. Numeric literals are
// unaffected — tinyexpr parses them with strtod, which accepts 1E10.
// Returns false if the expression is too long.
// tmp/rebuilt are static, not stack locals. At kMaxExpr each they made this
// a 560 B frame, and preprocess sits at the very bottom of every engine
// call — including the one complexexpr's parser reaches at the leaf of its
// recursion, where it was the frame that finally overran core 0's stack
// (HW 2026-08-08: the fault PC was this function's prologue). Safe to share:
// preprocess is a pure string rewrite that calls nothing which re-enters it,
// and the UI is single-threaded on core 0.
bool preprocess(const char* in, char* out, size_t out_len) {
    static char tmp[kMaxExpr];
    size_t n = 0;
    for (const char* p = in; *p != 0; ++p) {
        if (n + 1 >= sizeof(tmp)) {
            return false;
        }
        tmp[n++] = *p;
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
        static char rebuilt[kMaxExpr];
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

bool preprocess_factorial(const char* in, char* out, size_t out_len) {
    return preprocess(in, out, out_len);
}

bool refs_complex_var(const char* expr, int skip_slot) {
    const Variables& v = engine().vars();
    const char* p = expr;
    while (*p != 0) {
        if (std::isalpha(static_cast<unsigned char>(*p)) != 0 || *p == '_') {
            const char* start = p;
            while (std::isalnum(static_cast<unsigned char>(*p)) != 0 || *p == '_') {
                ++p;
            }
            const auto len = static_cast<size_t>(p - start);
            int slot = -1;
            if (len == 1 && start[0] >= 'a' && start[0] <= 'z') {
                slot = start[0] - 'a';
            } else if (len == 3 && std::strncmp(start, "ans", 3) == 0) {
                slot = Variables::kAns;
            } else if (len == 5 && std::strncmp(start, "theta", 5) == 0) {
                slot = Variables::kTheta;
            }
            if (slot >= 0 && slot != skip_slot && v.imag[slot] != 0) {
                return true;
            }
        } else {
            ++p;
        }
    }
    return false;
}

calc_t& Variables::operator[](char name) {
    // Lowercase only (case-sensitive since 2026-07-18).
    if (name >= 'a' && name <= 'z') {
        return vars[name - 'a'];
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
constexpr int kLookupCount = 24 + 2 + kMaxCatalogEntries + kMaxConstants;

int build_lookup(Variables& vars, te_variable* lookup) {
    static char names[26][2];
    int li = 0;
    for (int i = 0; i < 26; ++i) {
        if (i == 'e' - 'a') {
            continue;  // 'e' = Euler's constant, not a variable
        }
        if (i == 'i' - 'a') {
            continue;  // 'i' = imaginary unit (Phase 4C); the real engine
                       // has no complex type, so it stays unbound here —
                       // referencing bare 'i' outside math::complexexpr
                       // is a plain unknown-identifier parse error.
        }
        names[i][0] = static_cast<char>('a' + i);
        names[i][1] = 0;
        lookup[li++] = {names[i], &vars.vars[i], TE_VARIABLE, nullptr};
    }
    lookup[li++] = {"theta", &vars.vars[Variables::kTheta], TE_VARIABLE, nullptr};
    lookup[li++] = {"ans", &vars.vars[Variables::kAns], TE_VARIABLE, nullptr};

    // Scientific constants (4D.17): read-only identifiers bound to the
    // catalog's own descriptor storage (multi-char, so no a-z shadow).
    int c_count = 0;
    const ConstDescriptor* cs = constants(&c_count);
    for (int i = 0; i < c_count; ++i) {
        lookup[li++] = {cs[i].name, &cs[i].value, TE_VARIABLE, nullptr};
    }

    // Functions come from the shared catalog (task 2.26) so the help
    // browser and the parser cannot drift apart. Help-only rows
    // (fn == nullptr — the list functions) are not engine-callable.
    int fn_count = 0;
    const FnDescriptor* cat = catalog(&fn_count);
    for (int i = 0; i < fn_count; ++i) {
        if (cat[i].fn == nullptr) {
            continue;
        }
        lookup[li++] = {cat[i].name, cat[i].fn, TE_FUNCTION0 + cat[i].arity, nullptr};
    }
    return li;
}

// The table lives in bss, not on the caller's stack, and is built once.
//
// Not a speed optimization: at kLookupCount x sizeof(te_variable) this is
// ~1.95 KB, and a stack copy in each of the three callers below pushed
// Engine::compile's frame to 2,232 B. Core 0 has 4 KB before core 1's
// stack top, so compiling from inside a render() (the slot editors used
// to, per row per 16-px strip) overran into core 1 mid-DMA and hung the
// machine — the 2026-08-05 Y=-editor lockup, D47.
//
// Building once is sound because the contents never change after
// startup: a-z bind to the singleton Engine's Variables (stable
// addresses), and constants()/catalog() return constexpr tables. The
// `built_for` pointer only guards against a second Engine instance
// (host tests); it is not a per-call cost. Trailing kMaxExtraVars slots
// are compile_with's to fill.
//
// Safe to share across calls because te_compile reads the array only
// while parsing (tinyexpr.c stashes it in the parse state; the built
// tree keeps just the copied addresses) and engine compiles never nest —
// preprocess() below is a pure string rewrite that cannot re-enter.
te_variable g_lookup[kLookupCount + Engine::kMaxExtraVars];
int g_lookup_len = 0;
const Variables* g_lookup_for = nullptr;

int lookup_table(Variables& vars) {
    if (g_lookup_for != &vars) {
        g_lookup_len = build_lookup(vars, g_lookup);
        g_lookup_for = &vars;
    }
    return g_lookup_len;
}

// Parse-nesting cap, the counterpart of the stated depth caps D45 gave
// the CAS parser. tinyexpr's parser is recursive descent — one
// list/expr/term/factor/power/base cycle per level of parenthesis or
// function-argument nesting — and it has no limit of its own, so the
// depth is whatever the input says.
//
// Sized to the measurement, not to taste: the cycle is **200 B** per level
// on the Pico 1, and core 0 has 4 KB before core 1's stack. The tightest
// caller is the list-lift path
// (evaluate_input -> listexpr::evaluate -> eval_list_into x3 -> eval_lift ->
// compile_with), whose prefix measures 2,392 B and so affords seven levels.
// A bare home-screen expression starts ~1,296 B in and could afford
// thirteen, but one cap has to hold for every call site.
//
// This was 8 when first written, sized against frames that have since
// shrunk on one path and not the other; re-measuring after the D47 leaf
// work showed 8 overshooting the list-lift budget by 24 B. Kept equal to
// math::complexexpr's home cap so both number modes stop at the same place.
//
// This is what the 2026-08-05 Y= lockup needed. Its stored slot held one of
// the 2026-08-02 nesting stress probes ("up to 20 nested trig calls"), and
// at 20 levels the parser walked off the stack — silently into core 1
// before stack guards, and as a hard fault in `factor`'s prologue push
// after them. Now it is a parse error, so the row just draws red and the
// slot stays editable.
constexpr int kMaxParseDepth = 7;

bool too_deeply_nested(const char* expr) {
    int depth = 0;
    for (const char* p = expr; *p != 0; ++p) {
        if (*p == '(') {
            if (++depth > kMaxParseDepth) {
                return true;
            }
        } else if (*p == ')') {
            --depth;
        }
    }
    return false;
}

}  // namespace

Engine::Engine() = default;

EvalResult Engine::eval_internal(const char* expr) {
    EvalResult res;

    // Static, like preprocess's buffers below: this chain is reached at the
    // leaf of complexexpr's recursion, where every byte of frame is paid at
    // maximum stack depth (D47). Not reentrant — nothing it calls comes back
    // through the engine.
    static char processed[kMaxExpr];
    if (!preprocess(expr, processed, sizeof(processed))) {
        res.error = "Expression too long";
        return res;
    }

    // Real-only path: a referenced variable holding a complex value is
    // an error, never a silent real-part read (4D.15, P4-11/D37). The
    // complex-capable evaluator (math::complexexpr) resolves such
    // variables itself before spans ever reach this engine.
    if (refs_complex_var(processed)) {
        res.error = "Non-real variable";
        return res;
    }
    if (too_deeply_nested(processed)) {
        res.error = "Too deeply nested";
        return res;
    }

    const int li = lookup_table(vars_);

    int err = 0;
    te_expr* compiled = te_compile(processed, g_lookup, li, &err);
    if (compiled == nullptr) {
        res.error = "Syntax error";
        return res;
    }
    res.value = te_eval(compiled);
    te_free(compiled);
    res.ok = true;
    return res;
}

void* Engine::compile(const char* expr, int sweep_slot) {
    char processed[kMaxExpr];
    if (!preprocess(expr, processed, sizeof(processed))) {
        return nullptr;
    }
    if (sweep_slot != kNoComplexCheck && refs_complex_var(processed, sweep_slot)) {
        return nullptr;  // Non-real variable (4D.15) — same surface as a parse error
    }
    if (too_deeply_nested(processed)) {
        return nullptr;  // Same surface as a parse error; see kMaxParseDepth
    }
    const int li = lookup_table(vars_);
    int err = 0;
    return te_compile(processed, g_lookup, li, &err);
}

void* Engine::compile_with(const char* expr, const ExtraVar* extras, int extra_count,
                           int sweep_slot) {
    if (extra_count < 0 || extra_count > kMaxExtraVars) {
        return nullptr;
    }
    char processed[kMaxExpr];
    if (!preprocess(expr, processed, sizeof(processed))) {
        return nullptr;
    }
    if (sweep_slot != kNoComplexCheck && refs_complex_var(processed, sweep_slot)) {
        return nullptr;  // Non-real variable (4D.15)
    }
    if (too_deeply_nested(processed)) {
        return nullptr;  // Same surface as a parse error; see kMaxParseDepth
    }
    // The extras go in the slots reserved past the shared table's tail;
    // they are overwritten by the next compile_with, which is fine — the
    // array is only read during te_compile below.
    int li = lookup_table(vars_);
    for (int i = 0; i < extra_count; ++i) {
        g_lookup[li++] = {extras[i].name, extras[i].addr, TE_VARIABLE, nullptr};
    }
    int err = 0;
    return te_compile(processed, g_lookup, li, &err);
}

calc_t Engine::eval_compiled_raw(void* handle) {
    if (handle == nullptr) {
        return std::numeric_limits<calc_t>::quiet_NaN();
    }
    return te_eval(static_cast<te_expr*>(handle));
}

calc_t Engine::eval_compiled(void* handle, calc_t x_val) {
    return eval_compiled(handle, 'x' - 'a', x_val);
}

calc_t Engine::eval_compiled(void* handle, int var_slot, calc_t value) {
    if (handle == nullptr || var_slot < 0 || var_slot >= Variables::kCount) {
        return std::numeric_limits<calc_t>::quiet_NaN();
    }
    vars_.vars[var_slot] = value;
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
    //
    // Static for the same reason as eval_internal's buffer: this is reached
    // at the leaf of complexexpr's recursion (D47). eval_internal copies out
    // of it before doing anything, so the two statics never alias.
    static char body[kMaxExpr];
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
        // Case-sensitive (2026-07-18): targets are lowercase a-z or
        // "theta"; an uppercase single letter gets a pointed error
        // instead of the old silent fold.
        char name[8] = {};
        size_t ni = 0;
        while (ni < sizeof(name) - 1 && std::isalpha(rhs[ni]) != 0) {
            name[ni] = rhs[ni];
            ++ni;
        }
        const char* after = rhs + ni;
        while (*after == ' ') {
            ++after;
        }
        if (*after == 0 && ni > 0) {
            if (ni == 1 && name[0] >= 'a' && name[0] <= 'z') {
                store_index = name[0] - 'a';
            } else if (ni == 1 && name[0] >= 'A' && name[0] <= 'Z') {
                EvalResult res;
                res.error = "Variables are lowercase a-z";
                return res;
            } else if (std::strcmp(name, "theta") == 0) {
                store_index = Variables::kTheta;
            }
        }
        if (store_index == 'e' - 'a') {
            EvalResult res;
            res.error = "e is reserved (Euler's e)";
            return res;
        }
        if (store_index == 'i' - 'a') {
            EvalResult res;
            res.error = "i is reserved (imaginary unit)";
            return res;
        }
        if (store_index >= 0) {
            *arrow = 0;  // Evaluate only the left side
        }
    }

    EvalResult res = eval_internal(body);
    if (res.ok) {
        // Real writes clear any complex value the slot held (4D.15).
        vars_.set_real(Variables::kAns, res.value);
        if (store_index >= 0) {
            vars_.set_real(store_index, res.value);
            res.stored_var = store_index;
        }
    }
    return res;
}

EvalResult Engine::evaluate_at(const char* expr, calc_t x_val) {
    // Save/restore both parts: the temporary binding is real, but a
    // complex value the user stored in x must survive unrelated field
    // evaluations (4D.15).
    const calc_t saved = vars_.vars['x' - 'a'];
    const calc_t saved_im = vars_.imag['x' - 'a'];
    vars_.set_real('x' - 'a', x_val);
    EvalResult res = eval_internal(expr);
    vars_.vars['x' - 'a'] = saved;
    vars_.imag['x' - 'a'] = saved_im;
    return res;
}

Engine& engine() {
    static Engine instance;
    return instance;
}

bool eval_field(const char* text, calc_t* out) {
    Engine& eng = engine();
    // The real-only seam (matexpr/listexpr/complexexpr scalar spans,
    // UI numeric fields): a complex-valued variable reference is an
    // error here, checked before evaluate_at rebinds x — otherwise a
    // complex x would silently evaluate as its real part (4D.15, D37).
    if (refs_complex_var(text)) {
        return false;
    }
    // evaluate_at leaves Ans/store untouched; binding X to its own
    // current value makes the call side-effect free.
    const EvalResult res = eng.evaluate_at(text, eng.vars()['x']);
    if (!res.ok || std::isnan(res.value) || std::isinf(res.value)) {
        return false;
    }
    *out = res.value;
    return true;
}

}  // namespace math
