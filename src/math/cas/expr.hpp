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
//
// The arena is two-ended (Stage 5 / D45):
//
//   [ nodes ---->                              <---- scratch ]
//   0                                              kComputeBytes
//
// Nodes bump up from the bottom and are only reclaimed by reset(). The
// passes' per-invocation scratch arrays bump *down* from the top under
// LIFO mark/release, because they used to be stack-local: 64-operand arrays
// gave simplify_sum/simplify_product ~1.1 KB frames on a core-0 stack with
// only 4 KB below it before core 1's (see simplify.cpp). They cannot share
// the node end — simplify() runs its fixed-point loop up to 50 times without
// resetting, so a bump-only scratch would multiply by passes x depth and
// exhaust the arena. LIFO release makes each pass reuse the same space.
// Exhaustion at either end is a clean nullptr; a stack overrun was not.
class ExprPool {
public:
    void reset() {
        offset_ = 0;
        scratch_used_ = 0;
        overflow_ = false;
    }
    Expr* alloc();  // nullptr when full

    // Raw bump allocation from the node end, for anything node-lifetime.
    void* alloc_raw(std::size_t bytes, std::size_t align);

    // ---- LIFO scratch end (align must be a power of two) ----
    void* alloc_scratch(std::size_t bytes, std::size_t align);
    std::size_t scratch_mark() const { return scratch_used_; }
    void scratch_release(std::size_t mark) { scratch_used_ = mark; }

    // Both ends together — what near_capacity() judges.
    std::size_t used() const { return offset_ + scratch_used_; }
    std::size_t capacity() const;

    // Sticky: set by any allocation that failed since the last reset(). When
    // this is set the operation's result is incomplete, so callers must
    // report an error rather than display it (spec §13 Risk 2) — the
    // simplifier's own "last good form" fallback is otherwise indistinguishable
    // from a converged answer.
    bool overflowed() const { return overflow_; }

    // At or past the spec's 80% abort threshold. Node-multiplying passes
    // check this and give up early rather than grinding the pool to nothing.
    bool near_capacity() const { return used() * 5 >= capacity() * 4; }

private:
    std::size_t offset_ = 0;        // node end, grows up
    std::size_t scratch_used_ = 0;  // scratch end, grows down from the top
    bool overflow_ = false;
};

// Scoped LIFO scratch allocation. Takes a mark on construction and restores
// it on destruction, so a pass's arrays are released even on the early-return
// paths (of which the simplifier has many).
class ScratchScope {
public:
    ScratchScope();
    ~ScratchScope();
    ScratchScope(const ScratchScope&) = delete;
    ScratchScope& operator=(const ScratchScope&) = delete;
    ScratchScope(ScratchScope&&) = delete;
    ScratchScope& operator=(ScratchScope&&) = delete;

    // nullptr when the arena is full; the caller must abort the pass.
    void* alloc(std::size_t bytes, std::size_t align);

private:
    std::size_t mark_;
};

extern ExprPool g_cas_pool;

}  // namespace math::cas
