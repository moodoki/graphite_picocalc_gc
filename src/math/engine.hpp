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

    // name: 'A'..'Z' (case-insensitive). theta/Ans via the indices above.
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

    Variables& vars() { return vars_; }

private:
    Variables vars_;

    EvalResult eval_internal(const char* expr);
};

Engine& engine();

}  // namespace math
