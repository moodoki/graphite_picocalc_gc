#pragma once

#include <cstddef>
#include <cstdint>

#include "math/complex.hpp"
#include "math/types.hpp"

namespace math {
class Array;
}

// Unified home-screen evaluator (Phase 5.2, idea F — D37/D40/D48).
//
// Replaces math::matexpr, math::complexexpr and math::listexpr with one
// evaluator over a tagged Value. tinyexpr (`evaluate_real`) is NOT replaced —
// it keeps the graphing/tables/stats path, per phase4-spec.md §5.2's
// performance guardrail. Four parsers become two.
//
// Structure, decided 2026-08-09 (5.2.1): a **shunting-yard compiler emitting a
// flat RPN program, executed by a stack machine**. Neither phase recurses on
// the C++ call stack. One mechanism satisfies two requirements that looked
// independent:
//
//   * D48's constraint — every previous parser needed a separately-discovered
//     depth cap against core 0's 4 KB, and three of the four were found by
//     something crashing. matexpr's landed at depth 3 with 84 B of margin.
//     Here depth is a sized array in bss, inspectable and bounded by
//     construction.
//   * The list lift's performance contract — listexpr today binds list names
//     to scalar slots, compiles ONCE via tinyexpr, then evaluates per element
//     in 256-element chunks against PSRAM-backed arrays. A flat program is
//     exactly that artifact: compile once, rebind the element slots, re-run.
//     An interpreter re-parsing per element would be N times slower on a
//     999-element list.
//
// This header is task 5.2.2: the value type, the instruction encoding and the
// sizing. The compiler is 5.2.3, the machine 5.2.4.
namespace math::unified {

// ---- Value ---------------------------------------------------------------

enum class Kind : uint8_t { kReal, kComplex, kMatrix, kList };

// 24 bytes on target, measured (5.2.1) — and notably *smaller* than the 32 B
// matexpr::Value it replaces, which carries a full Complex AND a const Array*
// without overlapping them. §F's long-standing "a tagged union is larger per
// node than any of today's narrower working types" did not survive the
// measurement; see phase5.2-spec.md §5.
//
// Matrices and lists are held by *reference*. Ownership stays with the stores
// (matrices(), lists(), named_lists()) and with the evaluator's own temporaries
// — a Value never owns an Array, exactly as matexpr::Value never did.
struct Value {
    Kind kind = Kind::kReal;
    union {
        calc_t r;
        Complex c;
        const Array* a;
    };

    Value() : r(0.0) {}
    static Value real(calc_t v) {
        Value x;
        x.kind = Kind::kReal;
        x.r = v;
        return x;
    }
    static Value complex(const Complex& v) {
        Value x;
        x.kind = Kind::kComplex;
        x.c = v;
        return x;
    }
    static Value matrix(const Array* m) {
        Value x;
        x.kind = Kind::kMatrix;
        x.a = m;
        return x;
    }
    static Value list(const Array* l) {
        Value x;
        x.kind = Kind::kList;
        x.a = l;
        return x;
    }

    bool is_scalar() const { return kind == Kind::kReal || kind == Kind::kComplex; }
    bool is_array() const { return kind == Kind::kMatrix || kind == Kind::kList; }
    // Scalars promote to Complex on demand; the reverse is a caller decision
    // (REAL mode rejects a non-real result rather than truncating it).
    Complex as_complex() const { return kind == Kind::kComplex ? c : Complex(r, 0.0); }
};

// ---- Instructions --------------------------------------------------------

enum class Op : uint8_t {
    kPushConst,  // b = constant-pool index
    kPushVar,    // a = variable slot (a-z, theta, ans)
    kPushSysc,   // b = catalog constant index (pi, e, clight, ...)
    kPushMat,    // a = matrix slot 0-9, or kMatAnsSlot
    kPushList,   // b = list ref (l1-l6 and named, one numbering)
    kPushElem,   // a = element slot; the lift rebinds these per index
    kAdd,
    kSub,
    kMul,
    kDiv,
    kPow,
    kNeg,
    kTranspose,  // postfix ^T
    kCall,       // b = catalog index, a = arity
    kIndex,      // [A](row, col) — pops col, row, matrix
    kStore,      // b = encoded target, see StoreTarget
};

// 4 bytes. Kept deliberately narrow: the program array is sized for the worst
// case and lives in bss, so per-instruction width is a real budget line.
struct Instr {
    Op op = Op::kPushConst;
    uint8_t a = 0;
    uint16_t b = 0;
};

// ---- Sizing --------------------------------------------------------------
//
// Budget, against the ~10 KB of bss that retiring the three evaluators frees
// (10,053 B across 19 symbols, 8.4 KB of it listexpr's string scratch — see
// phase5.2-spec.md §5). Everything here is bss:
//
//   operand stack   64 x 24 =  1,536 B
//   Program                  =  2,064 B  (code 1,024 + consts 1,024 + 16
//                                         bookkeeping; measured on target,
//                                         not derived — the struct pads)
//                             -------
//                              3,600 B
//
// So the phase still nets several KB back once 5.2.11 deletes the old
// evaluators. Deletion is what banks it — it is not optional cleanup.
//
// kMaxStack is the user-visible nesting limit that replaces matexpr's cap of
// 3, complexexpr's 7/4 and tinyexpr's 7. It bounds operand depth, not paren
// depth: `((((1))))` costs one stack slot, while `[A]*[B]*[C]*...` costs one
// per pending operand. 64 is far beyond anything the old caps allowed.
constexpr int kMaxStack = 64;
constexpr int kMaxCode = 256;
constexpr int kMaxConsts = 64;

// The MatAns pseudo-slot, addressed like a matrix register beyond [A]-[J].
constexpr uint8_t kMatAnsSlot = 10;

// A compiled program. Held once and re-run per element by the list lift, which
// is the entire reason this type exists rather than a direct interpreter.
struct Program {
    Instr code[kMaxCode];
    Complex consts[kMaxConsts];
    int n_code = 0;
    int n_consts = 0;
    // Element slots referenced by kPushElem, in binding order. The lift writes
    // these per index; the program itself is unchanged between elements.
    int n_elem_slots = 0;
};

// ---- Compiler (5.2.3) ----------------------------------------------------
//
// Shunting-yard, iterative: no parse recursion, so nesting costs operator-stack
// slots rather than call frames. Emits RPN into `out`.
//
// Scope as of 5.2.3: numeric and imaginary literals, variables, catalog
// constants and function calls, `+ - * / ^`, unary sign, parentheses. Matrix
// and list literals/references arrive with their tiers (5.2.6/5.2.7), stores
// with 5.2.8.
//
// `kPushVar` slots are `Variables`' own indices (0-25 = a-z, 26 = theta,
// 27 = Ans) rather than a parallel numbering, so `Variables::is_complex(idx)`
// can be read directly at run time.
//
// Returns false and sets *err to a static string on failure.
bool compile(const char* src, Program& out, const char** err);

// ---- Stack machine (5.2.4) -----------------------------------------------
//
// Executes a compiled program. No evaluation recursion — depth is the operand
// stack, a bss array, so over-deep input reports rather than faults.
//
// Scope as of 5.2.4: real and complex scalars. Matrix and list operands are
// recognised and rejected; their tiers are 5.2.6 and 5.2.7.
//
// A complex result that lands exactly on the real axis is returned as real, so
// `i^2` is -1 rather than -1+0i. That is exactness, not a tolerance: D49 made
// integer powers of a complex base exact so this test could be `im == 0`.
bool run(const Program& p, Value* out, const char** err);

static_assert(sizeof(Value) == 24,
              "Value is the measured 24 B (5.2.1); update the budget if it moves");
static_assert(sizeof(Instr) == 4, "Instr is 4 B; the program array is sized on it");
static_assert(alignof(Value) == alignof(Complex), "Value must not over-align the operand stack");

}  // namespace math::unified
