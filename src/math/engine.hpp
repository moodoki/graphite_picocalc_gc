#pragma once

#include <cstddef>

#include "math/types.hpp"

namespace math {

// Variable storage: A-Z (26), theta, Ans = 28 slots.
struct Variables {
    static constexpr int kCount = 28;
    static constexpr int kTheta = 26;
    static constexpr int kAns = 27;

    calc_t vars[kCount] = {};

    // name: 'a'..'z' (case-sensitive; anything else maps to Ans).
    // theta/Ans via the indices above.
    calc_t& operator[](char name);
    calc_t& ans() { return vars[kAns]; }
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
    void* compile(const char* expr);
    calc_t eval_compiled(void* handle, calc_t x_val);
    calc_t eval_compiled(void* handle, int var_slot, calc_t value);
    void free_compiled(void* handle);

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

}  // namespace math
