#pragma once

#include <cstddef>

#include "math/types.hpp"

namespace math {

// Variable storage: A-Z (26), theta, Ans = 28 slots.
//
// Complex-valued storage (4D.15, D37/D38): `vars` holds the real parts
// and stays a flat contiguous array because tinyexpr captures raw slot
// addresses at compile time (build_lookup) — the real engine only ever
// sees real parts. `imag` is the parallel imaginary array; imag[i] != 0
// means slot i currently holds a complex value. Real-only consumers
// must error on such a slot rather than silently read the real part
// (P4-11: use refs_complex_var below), and every real write must clear
// the imaginary part (set_real).
struct Variables {
    static constexpr int kCount = 28;
    static constexpr int kTheta = 26;
    static constexpr int kAns = 27;

    calc_t vars[kCount] = {};
    calc_t imag[kCount] = {};

    // name: 'a'..'z' (case-sensitive; anything else maps to Ans).
    // theta/Ans via the indices above.
    calc_t& operator[](char name);
    calc_t& ans() { return vars[kAns]; }

    bool is_complex(int idx) const { return idx >= 0 && idx < kCount && imag[idx] != 0; }
    void set_real(int idx, calc_t v) {
        vars[idx] = v;
        imag[idx] = 0;
    }
    void set_complex(int idx, calc_t re, calc_t im) {
        vars[idx] = re;
        imag[idx] = im;
    }
};

struct EvalResult {
    bool ok = false;
    calc_t value = 0;
    const char* error = nullptr;  // Static string when ok == false
    // When the expression used the store operator ("expr->A"): the
    // variable index that was assigned, else -1.
    int stored_var = -1;
};

class Engine {
public:
    Engine();

    // Evaluate an expression. Supports the store operator "expr->A"
    // (decision D1). Updates Ans on success.
    EvalResult evaluate(const char* expr);

    // Evaluate with X temporarily bound to x_val (graphing hot path —
    // no Ans update, no store).
    EvalResult evaluate_at(const char* expr, calc_t x_val);

    // Compile once, evaluate many (graphing): returns an opaque handle
    // (nullptr on parse error). eval_compiled binds the swept variable
    // then evaluates; caller must free_compiled() when done. Not
    // reentrant across the shared variable slots — evaluate one
    // function's points at a time.
    //
    // var_slot picks which Variables slot the sweep writes (task 2.4):
    // 'x'-'a' for function mode, 't'-'a' for parametric,
    // Variables::kTheta for polar. The 2-arg form sweeps X (Phase 1).
    //
    // sweep_slot (4D.15): the slot the caller is about to sweep — it is
    // excluded from the complex-variable check because the sweep
    // overwrites its real part anyway (a stale complex value in x must
    // not block graphing Y1=sin(x)). -1 checks every slot;
    // kNoComplexCheck skips the check entirely (syntax-validity-only
    // compiles, e.g. the slot editors).
    static constexpr int kNoComplexCheck = -2;
    void* compile(const char* expr, int sweep_slot = -1);
    calc_t eval_compiled(void* handle, calc_t x_val);
    calc_t eval_compiled(void* handle, int var_slot, calc_t value);
    void free_compiled(void* handle);

    // List support (task 3A.5): compile with extra variable bindings
    // layered over the standard lookup — list_expr binds l1..l6 to
    // per-element slots during vector-lifted evaluation. The bound
    // addresses must outlive the handle.
    struct ExtraVar {
        const char* name;
        const calc_t* addr;
    };
    static constexpr int kMaxExtraVars = 8;
    void* compile_with(const char* expr, const ExtraVar* extras, int extra_count,
                       int sweep_slot = -1);
    // te_eval without rebinding a variable slot (the caller writes the
    // extras' addresses directly between calls).
    calc_t eval_compiled_raw(void* handle);

    Variables& vars() { return vars_; }

private:
    Variables vars_;

    EvalResult eval_internal(const char* expr);
};

Engine& engine();

// Evaluate a standalone numeric field entry (WINDOW, table setup, ASK
// values): full expression syntax — 2*pi, pi/180, stored vars — with
// no Ans update and no store. strtod-style prefix parsing silently
// committed `2*pi` as 2.0 before this (HW 2026-07-18). Returns false
// on parse error or a non-finite result; the field keeps its old value.
bool eval_field(const char* text, calc_t* out);

// Rewrite postfix factorial `<primary>!` into `fac(<primary>)`, the same
// pass Engine::evaluate runs before compiling. Exposed so the complex
// evaluator (math::complexexpr) can share it — otherwise `5!` fails as a
// syntax error whenever input routes through the complex path (non-REAL
// mode or an `i`-bearing expression). Returns false if the result won't
// fit in out_len or a bare `!` has no preceding operand.
bool preprocess_factorial(const char* in, char* out, size_t out_len);

// True when the expression references a variable slot that currently
// holds a complex value (imag != 0): standalone a-z tokens, `ans`,
// `theta`. Real-only consumers use this to error rather than silently
// read the real part (4D.15, P4-11/D37). skip_slot: a slot to ignore
// (the caller's sweep variable, about to be overwritten).
bool refs_complex_var(const char* expr, int skip_slot = -1);

}  // namespace math
