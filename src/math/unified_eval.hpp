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
//     in 256-element chunks against PSRAM-backed arrays. A flat program keeps
//     that: parsing happens once, and the per-element work is instruction
//     dispatch. An interpreter re-parsing per element would be N times slower
//     on a 999-element list.
//
// How the list tier actually lifts — REVISED 2026-08-09 (5.2.6). The 5.2.1
// sketch had the whole program re-run per element with `kPushElem` slots
// rebound each time. Building the tier showed that shape is wrong for this
// evaluator, on correctness before performance: a program is not uniformly
// element-dependent. In `l1/sum(l1)` the reduction is loop-invariant, so a
// whole-program re-run recomputes an O(N) reduction N times — quadratic on
// exactly the expression listexpr handles in linear time today (it substitutes
// reductions before lifting). Recovering that would need dataflow analysis to
// find the invariant subranges and a rewritten program to hoist them.
//
// Instead a list operand *broadcasts* at the instruction that consumes it:
// every elementwise op streams its operands in 256-element chunks and
// materialises one temporary Array. Each node is then evaluated exactly once
// per element by construction, reductions included, and nesting
// (`mean(l1*2)+l3`) falls out with no analysis. This is what
// phase5.2-spec.md §1 describes as "the same generic binary-op dispatch over a
// wider tag enum"; §3's element-slot row is the mechanism it replaces.
// `kPushElem` is gone, and with it Program::n_elem_slots (-8 B).
//
// The cost is intermediate arrays — `sin(l1)+2*l2` streams three passes where
// today's lift streams one. They come from the ArrayStore temporaries listexpr
// already pays for (g_op[4], g_temp[2], g_red), and the per-element work they
// replace is a tinyexpr tree walk per element, which is not obviously cheaper
// than the extra streaming. 5.2.12 measures it on hardware against today's
// lift; that measurement, not this comment, is what settles it.
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
    kPushInt,    // b = small literal integer — quoted arguments, see kJump
    kMakeList,   // a = element count; pops that many scalars (5.2.6)
    kMakeMat,    // a = rows, b = cols; pops rows*cols scalars (5.2.7)
    kAdd,
    kSub,
    kMul,
    kDiv,
    kPow,
    kNeg,
    kTranspose,  // postfix ^T
    kCall,       // b = catalog index, a = arity
    kCallBi,     // b = builtin index, a = arity — see builtin_index()
    kCallList,   // b = list-function index, a = arity — see list_fn_index()
    kCallMat,    // b = matrix-function index, a = arity — see mat_fn_index()
    kJump,       // b = absolute code index; skips a quoted body (5.2.6)
    kRet,        // ends a quoted body, handing its value back to the caller
    kIndex,      // [A](row, col) — pops col, row, matrix
    kStore,      // a = StoreKind, b = target index (5.2.8)
};

// ---- Stores (5.2.8) ------------------------------------------------------
//
// One grammar for the five target forms the three evaluators accepted
// separately — `-> a`, `-> theta`, `-> l1`-`l6`, `-> name` and `-> [A]`-`[J]`.
// The arrow is compiled, not stripped: every evaluator today finds the
// rightmost "->" by string search and re-trims the body around it
// (engine.cpp:402, complex_expr.cpp:454, list_expr.cpp:1306, mat_expr.cpp:978),
// which is four copies of one rule and the reason `2->3` reports different
// errors depending on which parser claimed the input.
enum class StoreKind : uint8_t {
    kNone,
    kVar,      // index = Variables slot (a-z, theta)
    kList,     // index = list ref, one numbering: 0-5 = l1-l6, 6+k = named slot k
    kNewList,  // a named list that does not exist yet — the name is in
               // Program::new_list and the registry entry is created at COMMIT
               // time, so a program that fails to evaluate leaves no stray
               // empty list behind (listexpr's rule since 4D.13)
    kMatrix,   // index = matrix slot 0-9
};

// Longest storable named list. Must match NamedLists::kMaxName, asserted where
// the compiler resolves the name; kept as a literal so this header does not
// pull in the list stores.
constexpr int kMaxStoreName = 5;

// Commit vs probe (5.2.8). A probe run computes the value and touches nothing:
// no Ans, no store, no MatAns, no in-place sort, no mat2list writes. It is what
// the REAL-mode probe on the home screen needs (home_screen.cpp:669, which
// leans on complexexpr never mutating engine state) and what the differential
// harness in 5.2.9 needs to run the same input through both evaluators without
// the first one changing what the second sees.
//
// The two modes return the same *value* for the same input — the difference is
// side effects only, with one deliberate exception: REAL mode's rule that a
// non-real value is never committed or displayed (D30) is a commit rule, so
// kCommit reports "Non-real result" where kProbe hands back the value. That is
// exactly the question a probe is asked.
enum class Mode : uint8_t { kCommit, kProbe };

// What a kCommit run wrote, so the caller knows what to persist. One
// convention replacing three: matexpr's {stored_matrix, stored_list,
// matrices_modified, lists_modified, uint8 lists_mask}, listexpr's
// {stored_list, lists_modified, uint32 lists_mask, names_modified} and
// engine/complexexpr's bare stored_var.
struct Commit {
    int8_t var = -1;     // `-> a` / `-> theta` target, else -1. Ans is written
                         // for every scalar result, store or no store.
    int8_t matrix = -1;  // `-> [X]` target, else -1
    int16_t list = -1;   // `-> lk` / `-> name` target, else -1
    // Every list ref written by this run, by ref number: the store, an in-place
    // sort, and mat2list's column targets. Persistence keys off this, not off
    // `list` — the D35 sort-persistence gap was exactly that distinction.
    uint32_t lists_mask = 0;
    bool names_modified = false;  // the named-list registry gained an entry
    bool mat_ans = false;         // a matrix result rewrote MatAns; persist it
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
//                                         bookkeeping and the pending store
//                                         name; measured on target, not
//                                         derived — the struct pads)
//                             -------
//                              3,600 B
//
// The list tier (5.2.6) adds ~160 B on top of that and no buffers at all: its
// chunk staging overlays the shared compute arena (scratch.hpp, 4 B for the
// region pointer) and its six result temporaries are Array *handles* (24 B
// each) whose storage comes from the existing ArrayStore — exactly what
// listexpr's g_op/g_temp/g_result are today, and what 5.2.11 deletes.
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

// A compiled program: emitted once per input, executed without any further
// parsing. That is what the list tier's per-element work rides on.
struct Program {
    Instr code[kMaxCode];
    Complex consts[kMaxConsts];
    int n_code = 0;
    int n_consts = 0;
    // The pending `-> name` target of a kStore with kind kNewList (5.2.8).
    // Text, because the registry entry is created when the store commits, not
    // when it compiles.
    char new_list[kMaxStoreName + 1] = {};
};

// ---- Compiler (5.2.3) ----------------------------------------------------
//
// Shunting-yard, iterative: no parse recursion, so nesting costs operator-stack
// slots rather than call frames. Emits RPN into `out`.
//
// Scope as of 5.2.8: numeric and imaginary literals, variables, catalog
// constants and function calls, `+ - * / ^`, unary sign, parentheses, brace
// list literals, `l1`-`l6` and named-list references, the list and matrix
// functions, matrix literals and references, and the store suffix.
//
// `kPushVar` slots are `Variables`' own indices (0-25 = a-z, 26 = theta,
// 27 = Ans) rather than a parallel numbering, so `Variables::is_complex(idx)`
// can be read directly at run time.
//
// Returns false and sets *err to a static string on failure.
// Builtin functions: the surface that is NOT in catalog.cpp. Absorbing "the
// catalogue" turned out to mean absorbing three tables, not one (found in
// 5.2.4) — catalog.cpp's 82 rows, tinyexpr's own 24 builtins (sqrt, abs, exp,
// the hyperbolics, ceil/floor/log10/atan2/pow), and complex_expr's complex-only
// set (conj, real, imag, arg). These two are the latter pair.
//
// Returns a table index, or -1 if the name is not a builtin.
int builtin_index(const char* name, size_t len);
int builtin_arity(int idx);

// List functions: the fourth table (5.2.6). sum/prod/length/mean/median/stdev
// (plus listexpr's `std` alias) consume a list and yield a scalar;
// cumsum/delta_list/sort_asc/sort_desc/seq/range yield a list. They are the
// catalogue's `fn == nullptr` help-only rows — phase5.2-spec.md §2's "help-only
// rows gain real bindings" — so they are looked up BEFORE the catalogue, which
// carries their names but no implementation.
//
// `seq(expr, var, lo, hi, step)` is the one call whose first two arguments are
// not values: the body is compiled as a quoted range (kJump/kRet) and both it
// and the variable slot reach the machine as kPushInt operands.
int list_fn_index(const char* name, size_t len);
bool list_fn_is_seq(int idx);
// sort_asc/sort_desc, the one pair whose whole-expression form is a statement
// rather than an expression — see the store grammar in unified_compile.cpp.
bool list_fn_is_sort(int idx);

// Matrix functions (5.2.7): the catalogue's other `fn == nullptr` block, plus
// the vector ops. Same story as the list table and looked up right after it —
// `norm` is in both worlds and dispatches on the argument's Kind (Frobenius
// for a matrix, Euclidean for a list), which is the unification this phase is
// named for rather than two functions sharing a name.
//
// `mat2list([A], l1, l2, …)` writes its list arguments, so they are targets,
// not values: they compile to kPushInt refs the way seq's variable does.
int mat_fn_index(const char* name, size_t len);
bool mat_fn_quotes_list_refs(int idx);

bool compile(const char* src, Program& out, const char** err);

// ---- Stack machine (5.2.4) -----------------------------------------------
//
// Executes a compiled program. No evaluation recursion — depth is the operand
// stack, a bss array, so over-deep input reports rather than faults.
//
// Scope as of 5.2.6: real and complex scalars, and lists of either. Matrix
// operands are recognised and rejected; that tier is 5.2.7.
//
// A complex result that lands exactly on the real axis is returned as real, so
// `i^2` is -1 rather than -1+0i. That is exactness, not a tolerance: D49 made
// integer powers of a complex base exact so this test could be `im == 0`.
//
// A returned list Value points at an evaluator-owned temporary that stays valid
// until the NEXT run() — the same contract listexpr's Result::list has had
// since Phase 3A. A returned *matrix* Value does not need that caveat in
// kCommit mode: a matrix result is copied into MatAns (or into its `-> [X]`
// slot), which is where matexpr has always left it.
//
// `mode` is deliberately not defaulted: silently committing is the failure mode
// this parameter exists to prevent. `commit` may be null when the caller does
// not care what was written.
bool run(const Program& p, Value* out, const char** err, Mode mode, Commit* commit = nullptr);

static_assert(sizeof(Value) == 24,
              "Value is the measured 24 B (5.2.1); update the budget if it moves");
static_assert(sizeof(Instr) == 4, "Instr is 4 B; the program array is sized on it");
static_assert(alignof(Value) == alignof(Complex), "Value must not over-align the operand stack");

}  // namespace math::unified
