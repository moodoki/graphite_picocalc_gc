#pragma once

#include <cstddef>
#include <cstdint>

#include "math/types.hpp"

// CAS symbolic expression tree (Phase 5, phase5-spec.md §2). Distinct from
// the numeric evaluation path (tinyexpr++, math::Engine) — this is a
// manipulable AST the simplify/diff/solve/integrate passes rewrite. All CAS
// work is home-screen-only, Enter-rate; it never touches the graphing hot
// path (spec §13 Risk 3).
//
// Nodes live in a bump-allocated pool (ExprPool below) that overlays the
// shared scratch kCompute region (see expr.cpp and scratch.hpp) — no new
// SRAM is reserved. Nodes are trivially destructible and only ever point
// within the pool, so ExprPool::reset() bulk-reclaims a whole operation's
// intermediate garbage.
namespace math::cas {

enum class ExprType : std::uint8_t {
    kNum,   // Numeric literal: 3, -2.5
    kVar,   // Variable: single char 'a'-'z' (and reserved 'i' = imaginary unit)
    kAdd,   // n-ary sum: child -> a -> b -> c
    kMul,   // n-ary product: child -> a -> b -> c
    kPow,   // binary: child = base, base->next = exponent
    kNeg,   // unary negation: child = operand
    kFunc,  // named function application: child = arg (nullptr = named const, e.g. pi)
    kEq,    // equation: child = lhs, lhs->next = rhs
};

struct Expr {
    ExprType type;

    union {
        calc_t num_val;      // NUM
        char var_name;       // VAR
        char func_name[12];  // FUNC ("sin", "pi", ...)
    };

    // Children as a singly-linked chain via next; tree via child.
    // ADD/MUL are n-ary (flattened); POW/EQ binary; NEG/FUNC unary.
    Expr* child;
    Expr* next;

    // ---- Convenience constructors (allocate from g_cas_pool) ----
    // Any of these returns nullptr if the pool is exhausted or an operand
    // is nullptr (failure propagates so callers can abort — spec §13 Risk 2).
    static Expr* num(calc_t val);
    static Expr* var(char name);
    static Expr* add(Expr* a, Expr* b);
    static Expr* mul(Expr* a, Expr* b);
    static Expr* pow(Expr* base, Expr* exp);
    static Expr* neg(Expr* a);
    static Expr* func(const char* name, Expr* arg);
    static Expr* eq(Expr* lhs, Expr* rhs);

    // ---- Predicates ----
    bool is_num() const { return type == ExprType::kNum; }
    bool is_var() const { return type == ExprType::kVar; }
    bool is_zero() const { return is_num() && num_val == 0.0; }
    bool is_one() const { return is_num() && num_val == 1.0; }
    bool is_neg_one() const { return is_num() && num_val == -1.0; }
    bool is_add() const { return type == ExprType::kAdd; }
    bool is_mul() const { return type == ExprType::kMul; }
    bool is_pow() const { return type == ExprType::kPow; }
    bool is_neg() const { return type == ExprType::kNeg; }
    bool is_func() const { return type == ExprType::kFunc; }
    bool is_eq() const { return type == ExprType::kEq; }

    // Does this expression reference variable v anywhere?
    bool contains(char v) const;

    // Deep structural equality (order-sensitive for n-ary children;
    // canonical ordering is the simplifier's job, Stage 1).
    bool equals(const Expr* other) const;

    // Deep clone into the current pool (nullptr on exhaustion).
    Expr* clone() const;

    // Number of direct children (n-ary chain length).
    int child_count() const;
};

// Bump allocator for Expr nodes, backed by the shared scratch kCompute
// region (math::scratch, 22528 bytes). Reset before each top-level CAS
// operation; there is no per-node free. Because it borrows kCompute, a CAS
// operation must not call into the region's other owners (list_expr / stats
// / infer / matops) while it holds the arena — it never does in v1.
class ExprPool {
public:
    void reset() { offset_ = 0; }
    Expr* alloc();  // nullptr when full
    std::size_t used() const { return offset_; }
    std::size_t capacity() const;

private:
    std::size_t offset_ = 0;
};

extern ExprPool g_cas_pool;

}  // namespace math::cas
