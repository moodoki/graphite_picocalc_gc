#include <cctype>
#include <cstdlib>
#include <cstring>

#include "math/catalog.hpp"
#include "math/engine.hpp"
#include "math/named_lists.hpp"
#include "math/unified_eval.hpp"

// Shunting-yard compiler for the unified evaluator (Phase 5.2, task 5.2.3).
//
// Iterative by design. Every previous parser in this project recursed on the
// call stack and needed a separately-discovered depth cap against core 0's
// 4 KB — three of the four were found by something crashing (D45/D47/D48).
// Here nesting consumes operator-stack slots in bss, which is a sized and
// inspectable resource: over-deep input is a clean error at a stated depth.
namespace math::unified {

namespace {

// Operator-stack entries. Function calls, parens and brace literals ride the
// same stack so that argument separation and grouping fall out of one
// mechanism — a `{1,2,3}` literal is an n-ary call to kMakeList and needs no
// parser of its own (contrast listexpr's split_args/whole_literal pair).
enum class Tok : uint8_t { kBinary, kUnary, kLParen, kFunc, kBrace, kMatLit, kIndex };

// Which table `fn` indexes.
enum class FnKind : uint8_t { kCatalog, kBuiltin, kList, kMatrix };

// Fields are shared between token kinds rather than unioned — the operator
// stack is 64 entries of bss and each byte here is 64 bytes there. Which
// kinds use which is noted per field; nothing reads a field a token did not
// set.
struct OpTok {
    Tok tok = Tok::kBinary;
    char ch = 0;         // '+' '-' '*' '/' '^' for kBinary/kUnary
    uint8_t prec = 0;    // higher binds tighter — or rows so far, kMatLit
    bool right = false;  // right-associative (only '^')
    FnKind kind = FnKind::kCatalog;
    uint8_t argc = 0;         // args seen, kFunc/kBrace/kIndex — or columns, kMatLit
    bool quoted = false;      // a body/ref argument is pending (seq, mat2list) —
                              // or "currently inside a row", kMatLit
    bool refs = false;        // the pending quoted argument is a list ref, not a body
    uint16_t fn = 0;          // table index, kFunc — or elements so far, kMatLit
    uint16_t jump_at = 0;     // the kJump to patch, kFunc — or the element count
                              // at the current row's start, kMatLit
    uint16_t body_start = 0;  // where a quoted body begins, kFunc only
};

// Precedence. Matches the existing evaluators' grammar
// (mat_expr.cpp:812-865, complex_expr.cpp:366-404) so that unification does
// not silently re-associate anything: `-` binds looser than `*`, and `^` is
// right-associative, as tinyexpr is built with TE_POW_FROM_RIGHT.
int precedence(char c) {
    switch (c) {
        case '+':
        case '-':
            return 1;
        case '*':
        case '/':
            return 2;
        case '^':
            return 4;
        default:
            return 0;
    }
}
constexpr int kUnaryPrec = 3;  // binds tighter than * /, looser than ^

bool ident_char(char c) {
    return (std::isalnum(static_cast<unsigned char>(c)) != 0) || c == '_';
}

// The operator stack lives in bss, not in Compiler's frame. On target it is
// 64 x 8 = 512 B, and leaving it on the stack cost compile() a 596 B frame —
// exactly the kind of figure that made matexpr's depth-3 cap necessary (D48).
// Putting it here drops the frame to the low tens of bytes.
//
// This makes compile() NON-REENTRANT, which the design supports rather than
// merely tolerates: the whole point of emitting a program is compile once,
// evaluate N times. A nested list expression becomes more instructions in one
// program, not a nested compile — so nothing re-enters this. Same
// non-reentrancy argument the engine's own preprocess buffers rest on
// (engine.cpp:36), and it must be re-checked if a tier ever wants a
// sub-compile. 5.2.6 was the first test of that and it held: seq's deferred
// body is compiled inline, as a range of the same program that the machine
// jumps over and re-enters, so the list tier added no nested compile.
OpTok g_ops[kMaxStack];

// The budget line this sits on. 8 B/entry when 5.2.3 wrote the comment above,
// 14 B after the list and matrix tiers added their grouping state — still an
// order of magnitude under the ~10 KB retiring the three evaluators frees, but
// it grows 64 bytes at a time, which is why the fields are shared.
static_assert(sizeof(g_ops) <= 1024, "operator stack is a bss budget line; keep OpTok narrow");

struct Compiler {
    const char* s = nullptr;
    Program* out = nullptr;
    const char* err = nullptr;

    OpTok* ops = g_ops;
    int n_ops = 0;

    // Shunting-yard needs to know whether the next token is an operand or an
    // operator: it is what distinguishes unary minus from subtraction.
    bool expect_operand = true;
    // seq's second argument is a variable NAME, not its value; mat2list's
    // trailing arguments are list targets, not values. Both resolve at compile
    // time and reach the machine as kPushInt.
    bool expect_var_name = false;
    bool expect_list_ref = false;

    // The store suffix, if one was seen (5.2.8). Emitted after the expression's
    // own code, so it pops the finished value.
    StoreKind store_kind = StoreKind::kNone;
    uint16_t store_index = 0;

    bool fail(const char* msg) {
        if (err == nullptr) {
            err = msg;
        }
        return false;
    }

    void skip_ws() {
        while (*s == ' ') {
            ++s;
        }
    }

    bool emit(Op op, uint8_t a = 0, uint16_t b = 0) {
        if (out->n_code >= kMaxCode) {
            return fail("Expression too complex");
        }
        out->code[out->n_code++] = Instr{op, a, b};
        return true;
    }

    bool push_const(const Complex& v) {
        if (out->n_consts >= kMaxConsts) {
            return fail("Expression too complex");
        }
        const int idx = out->n_consts++;
        out->consts[idx] = v;
        return emit(Op::kPushConst, 0, static_cast<uint16_t>(idx));
    }

    bool push_op(const OpTok& t) {
        if (n_ops >= kMaxStack) {
            return fail("Too deeply nested");
        }
        ops[n_ops++] = t;
        return true;
    }

    // Both bounds in one place. The lower guard is the one the callers care
    // about; the upper is redundant against push_op but stated so every index
    // into `ops` is provably in range at its use site rather than by tracing
    // back through push_op and pop_while_tighter.
    // const because it does not change which stack this is, only hands back a
    // mutable slot within it — `ops` is a pointer into bss, not owned storage.
    OpTok* top_op() const { return (n_ops > 0 && n_ops <= kMaxStack) ? &ops[n_ops - 1] : nullptr; }

    // Emit one operator-stack entry as an instruction.
    bool emit_op(const OpTok& t) {
        if (t.tok == Tok::kUnary) {
            // Unary '+' is a no-op; only '-' emits.
            return t.ch == '-' ? emit(Op::kNeg) : true;
        }
        if (t.tok == Tok::kBrace) {
            return emit(Op::kMakeList, t.argc);
        }
        if (t.tok == Tok::kMatLit) {
            return emit(Op::kMakeMat, t.prec, t.argc);
        }
        if (t.tok == Tok::kIndex) {
            return emit(Op::kIndex, t.argc);
        }
        if (t.tok == Tok::kFunc) {
            const Op op = t.kind == FnKind::kBuiltin  ? Op::kCallBi
                          : t.kind == FnKind::kList   ? Op::kCallList
                          : t.kind == FnKind::kMatrix ? Op::kCallMat
                                                      : Op::kCall;
            return emit(op, t.argc, t.fn);
        }
        switch (t.ch) {
            case '+':
                return emit(Op::kAdd);
            case '-':
                return emit(Op::kSub);
            case '*':
                return emit(Op::kMul);
            case '/':
                return emit(Op::kDiv);
            case '^':
                return emit(Op::kPow);
            default:
                return fail("Syntax error");
        }
    }

    // Pop while the stacked operator binds at least as tightly as the incoming
    // one. Equal precedence pops for left-associative operators only, which is
    // what keeps `2^3^2` right-associative and `8/4/2` left.
    bool pop_while_tighter(int prec, bool right) {
        while (n_ops > 0) {
            const OpTok& top = ops[n_ops - 1];
            if (top.tok != Tok::kBinary && top.tok != Tok::kUnary) {
                break;  // kLParen / kFunc / kBrace / kMatLit / kIndex all group
            }
            const bool tighter = right ? top.prec > prec : top.prec >= prec;
            if (!tighter) {
                break;
            }
            if (!emit_op(top)) {
                return false;
            }
            --n_ops;
        }
        return true;
    }

    // ---- operands --------------------------------------------------------

    // A numeric literal, with the "2i" imaginary shorthand the existing
    // evaluators accept (complex_expr.cpp:151, mat_expr.cpp:180).
    bool number() {
        char* end = nullptr;
        const double v = std::strtod(s, &end);
        if (end == s) {
            return fail("Syntax error");
        }
        s = end;
        if (*s == 'i' && !ident_char(s[1])) {
            ++s;
            return push_const(Complex(0.0, v));
        }
        return push_const(Complex(v, 0.0));
    }

    // Resolve a bare identifier: reserved constants first, then catalog
    // constants, then a single-letter variable. `e` and `i` are reserved and
    // never reach the variable slots, matching complex_expr.cpp's pointed
    // errors ("e is reserved (Euler's e)").
    bool identifier(const char* name, size_t len) {
        // l1-l6. listexpr's token_at only matches when the token is delimited
        // on both sides, which is exactly what "the whole identifier is two
        // characters" means here — `ln(x)` and `l10` are different identifiers
        // and never reach this test.
        if (len == 2 && name[0] == 'l' && name[1] >= '1' && name[1] <= '6') {
            return emit(Op::kPushList, 0, static_cast<uint16_t>(name[1] - '1'));
        }
        if (len == 1) {
            const char c = name[0];
            if (c == 'i') {
                return push_const(Complex(0.0, 1.0));
            }
            if (c == 'e') {
                return push_const(Complex(2.718281828459045235, 0.0));
            }
            if (c >= 'a' && c <= 'z') {
                return emit(Op::kPushVar, static_cast<uint8_t>(c - 'a'));
            }
        }
        // MatAns, the last matrix result as a typed token (4D.14).
        if (len == 6 && std::strncmp(name, "matans", 6) == 0) {
            return emit(Op::kPushMat, kMatAnsSlot);
        }
        if (len == 5 && std::strncmp(name, "theta", 5) == 0) {
            return emit(Op::kPushVar, static_cast<uint8_t>(Variables::kTheta));
        }
        if (len == 3 && std::strncmp(name, "ans", 3) == 0) {
            return emit(Op::kPushVar, static_cast<uint8_t>(Variables::kAns));
        }
        if (len == 2 && std::strncmp(name, "pi", 2) == 0) {
            return push_const(Complex(3.141592653589793238, 0.0));
        }

        int n = 0;
        const ConstDescriptor* cs = constants(&n);
        for (int k = 0; k < n; ++k) {
            if (std::strlen(cs[k].name) == len && std::strncmp(cs[k].name, name, len) == 0) {
                return emit(Op::kPushSysc, 0, static_cast<uint16_t>(k));
            }
        }

        // Named lists (4D.13), last: NamedLists::valid_name already excludes
        // every identifier resolved above, so the order is belt and braces.
        if (len >= 2 && len <= static_cast<size_t>(NamedLists::kMaxName)) {
            char nm[NamedLists::kMaxName + 1];
            std::memcpy(nm, name, len);
            nm[len] = 0;
            const int idx = named_lists().find(nm);
            if (idx >= 0) {
                return emit(Op::kPushList, 0, static_cast<uint16_t>(kNamedRefBase + idx));
            }
        }
        return fail("Syntax error");
    }

    // seq's variable argument: a name, resolved to a slot at compile time and
    // handed to the machine as a plain integer operand.
    bool var_name_operand() {
        skip_ws();
        const char* start = s;
        while (ident_char(*s)) {
            ++s;
        }
        const auto len = static_cast<size_t>(s - start);
        int slot = -1;
        if (len == 5 && std::strncmp(start, "theta", 5) == 0) {
            slot = Variables::kTheta;
        } else if (len == 1 && start[0] >= 'a' && start[0] <= 'z' && start[0] != 'e' &&
                   start[0] != 'i') {
            slot = start[0] - 'a';
        }
        if (slot < 0) {
            return fail("seq var must be a-z (not e/i) or theta");
        }
        return emit(Op::kPushInt, 0, static_cast<uint16_t>(slot));
    }

    // A list target: `l1`-`l6` or a named list, as a ref rather than a value.
    bool list_ref_operand() {
        skip_ws();
        const char* start = s;
        while (ident_char(*s)) {
            ++s;
        }
        const auto len = static_cast<size_t>(s - start);
        if (len == 2 && start[0] == 'l' && start[1] >= '1' && start[1] <= '6') {
            return emit(Op::kPushInt, 0, static_cast<uint16_t>(start[1] - '1'));
        }
        if (len >= 2 && len <= static_cast<size_t>(NamedLists::kMaxName)) {
            char nm[NamedLists::kMaxName + 1];
            std::memcpy(nm, start, len);
            nm[len] = 0;
            const int idx = named_lists().find(nm);
            if (idx >= 0) {
                return emit(Op::kPushInt, 0, static_cast<uint16_t>(kNamedRefBase + idx));
            }
        }
        return fail("mat2list targets are l1-l6");
    }

    // Close seq's quoted body: terminate it, point the skip past it, and push
    // the body's address as the call's first operand. Called when the body's
    // trailing ',' arrives — or at ')' for a malformed `seq(expr)`, so that a
    // program which fails its arity check at run time is still well formed.
    bool close_quoted_body(OpTok* fn) {
        if (!emit(Op::kRet)) {
            return false;
        }
        out->code[fn->jump_at].b = static_cast<uint16_t>(out->n_code);
        fn->quoted = false;
        return emit(Op::kPushInt, 0, fn->body_start);
    }

    // An identifier followed by '(' — resolved against the shared catalog,
    // which is the same table tinyexpr registers from (engine.cpp:194-200), so
    // absorbing it costs an arity dispatcher rather than sixty ports.
    bool function_call(const char* name, size_t len) {
        // List functions first (5.2.6). The catalogue carries their names as
        // help-only rows with no implementation, so letting it win would
        // resolve `sum` to a row that cannot be called.
        const int li = list_fn_index(name, len);
        if (li >= 0) {
            OpTok t;
            t.tok = Tok::kFunc;
            t.kind = FnKind::kList;
            t.fn = static_cast<uint16_t>(li);
            t.argc = 1;
            if (list_fn_is_seq(li)) {
                // Emit the skip before the body so the body is code the top
                // level jumps over rather than executes.
                t.quoted = true;
                t.jump_at = static_cast<uint16_t>(out->n_code);
                if (!emit(Op::kJump)) {
                    return false;
                }
                t.body_start = static_cast<uint16_t>(out->n_code);
            }
            return push_op(t);
        }

        // Matrix functions and the vector ops, for the same reason.
        const int mi = mat_fn_index(name, len);
        if (mi >= 0) {
            OpTok t;
            t.tok = Tok::kFunc;
            t.kind = FnKind::kMatrix;
            t.fn = static_cast<uint16_t>(mi);
            t.argc = 1;
            // mat2list writes its list arguments, so from the second onward
            // they are targets rather than values.
            t.refs = mat_fn_quotes_list_refs(mi);
            return push_op(t);
        }

        int n = 0;
        const FnDescriptor* cat = catalog(&n);
        for (int k = 0; k < n; ++k) {
            if (std::strlen(cat[k].name) == len && std::strncmp(cat[k].name, name, len) == 0) {
                OpTok t;
                t.tok = Tok::kFunc;
                t.fn = static_cast<uint16_t>(k);
                t.argc = 1;  // corrected to 0 below if the call is empty
                return push_op(t);
            }
        }
        // Not in the catalogue: try the builtin table (tinyexpr's own
        // functions plus the complex-only set). Catalog first, deliberately —
        // its sin/cos/tan are the angle-mode-aware entries, and tinyexpr's raw
        // radian versions must not shadow them (D46).
        const int bi = builtin_index(name, len);
        if (bi >= 0) {
            OpTok t;
            t.tok = Tok::kFunc;
            t.fn = static_cast<uint16_t>(bi);
            t.kind = FnKind::kBuiltin;
            t.argc = 1;
            return push_op(t);
        }
        return fail("Syntax error");
    }

    bool operand() {
        skip_ws();
        const char c = *s;
        if (c == 0) {
            return fail("Syntax error");
        }
        if ((std::isdigit(static_cast<unsigned char>(c)) != 0) || c == '.') {
            if (!number()) {
                return false;
            }
            expect_operand = false;
            return true;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) != 0) {
            const char* start = s;
            while (ident_char(*s)) {
                ++s;
            }
            const auto len = static_cast<size_t>(s - start);
            const char* after = s;
            while (*after == ' ') {
                ++after;
            }
            if (*after == '(') {
                s = after + 1;
                if (!function_call(start, len)) {
                    return false;
                }
                // An immediately-closing paren is a zero-argument call.
                skip_ws();
                if (*s == ')') {
                    OpTok* fn = top_op();
                    if (fn == nullptr) {
                        return fail("Syntax error");
                    }
                    fn->argc = 0;
                    ++s;
                    if (!emit_op(*fn)) {
                        return false;
                    }
                    --n_ops;
                    expect_operand = false;
                    return true;
                }
                expect_operand = true;
                return true;
            }
            if (!identifier(start, len)) {
                return false;
            }
            expect_operand = false;
            return true;
        }
        return fail("Syntax error");
    }

    // Start a matrix-literal row. Rows are tracked on the operator stack like
    // any other grouping, so the literal needs no parser of its own.
    bool open_row(OpTok* t) {
        ++s;  // '['
        skip_ws();
        if (*s == ']') {
            return fail("Bad matrix literal");  // "[[]]" has no elements
        }
        t->quoted = true;    // inside a row
        t->jump_at = t->fn;  // where this row's elements start
        ++t->fn;             // the element about to be compiled
        return true;
    }

    // '[' in operand position: a matrix reference `[A]`, or a literal `[[…]]`.
    bool open_bracket() {
        const char c = s[1];
        if (c == '[') {
            ++s;  // outer '['
            OpTok t;
            t.tok = Tok::kMatLit;
            if (!push_op(t)) {
                return false;
            }
            return open_row(top_op());
        }
        const int slot = (c >= 'A' && c <= 'J') ? c - 'A' : (c >= 'a' && c <= 'j') ? c - 'a' : -1;
        if (slot < 0 || s[2] != ']') {
            return fail("Syntax error");
        }
        s += 3;
        expect_operand = false;
        return emit(Op::kPushMat, static_cast<uint8_t>(slot));
    }

    // Postfix `!` compiles to the catalogue's own `fac`, looked up by name so
    // there is one factorial in the system rather than a second copy wired to
    // an operator.
    bool emit_factorial() {
        int n = 0;
        const FnDescriptor* cat = catalog(&n);
        for (int k = 0; k < n; ++k) {
            if (std::strcmp(cat[k].name, "fac") == 0) {
                return emit(Op::kCall, 1, static_cast<uint16_t>(k));
            }
        }
        return fail("Syntax error");
    }

    // ---- the store suffix (5.2.8) ----------------------------------------

    // Record the target and require it to be the end of the input. The old
    // parsers each searched for the RIGHTMOST "->" and re-trimmed the body
    // around it, which quietly makes `1->a->b` mean `(1->a) -> b` and then
    // fails inside tinyexpr. Here the first arrow ends the expression, so a
    // second one is a pointed error instead of a syntax error from three
    // layers down.
    bool take_target(StoreKind kind, int index) {
        skip_ws();
        if (*s != 0) {
            return fail("Bad store target");
        }
        store_kind = kind;
        store_index = static_cast<uint16_t>(index);
        return true;
    }

    // The five forms, in one place: `-> a`, `-> theta`, `-> l1`-`l6`,
    // `-> name` (existing or new) and `-> [A]`-`[J]`. Whether the *value*
    // suits the target is a run-time check — the compiler does not know the
    // kind of what it just compiled.
    bool store_target() {
        skip_ws();
        if (*s == '[') {
            const char c = s[1];
            const int slot = (c >= 'A' && c <= 'J')   ? c - 'A'
                             : (c >= 'a' && c <= 'j') ? c - 'a'
                                                      : -1;
            if (slot < 0 || s[2] != ']') {
                return fail("Bad store target");
            }
            s += 3;
            return take_target(StoreKind::kMatrix, slot);
        }
        const char* start = s;
        while (ident_char(*s)) {
            ++s;
        }
        const auto len = static_cast<size_t>(s - start);
        if (len == 0) {
            return fail("Bad store target");
        }
        // The pointed reserved-word errors, verbatim (engine.cpp:426-443,
        // complex_expr.cpp:410-424). They are the most-seen strings in this
        // grammar and the case fold they replaced was a silent wrong answer.
        if (len == 1 && start[0] >= 'A' && start[0] <= 'Z') {
            return fail("Variables are lowercase a-z");
        }
        if (len == 1 && start[0] == 'e') {
            return fail("e is reserved (Euler's e)");
        }
        if (len == 1 && start[0] == 'i') {
            return fail("i is reserved (imaginary unit)");
        }
        if (len == 1 && start[0] >= 'a' && start[0] <= 'z') {
            return take_target(StoreKind::kVar, start[0] - 'a');
        }
        if (len == 5 && std::strncmp(start, "theta", 5) == 0) {
            return take_target(StoreKind::kVar, Variables::kTheta);
        }
        if (len == 2 && start[0] == 'l' && start[1] >= '1' && start[1] <= '6') {
            return take_target(StoreKind::kList, start[1] - '1');
        }
        static_assert(kMaxStoreName == NamedLists::kMaxName,
                      "Program::new_list must hold any storable list name");
        if (len >= 2 && len <= static_cast<size_t>(kMaxStoreName)) {
            char nm[kMaxStoreName + 1];
            std::memcpy(nm, start, len);
            nm[len] = 0;
            const int idx = named_lists().find(nm);
            if (idx >= 0) {
                return take_target(StoreKind::kList, kNamedRefBase + idx);
            }
            // A name that could exist but does not: the registry entry is
            // created at commit time, so a program that fails to evaluate
            // leaves nothing behind (list_expr.cpp:1301).
            if (!NamedLists::valid_name(nm)) {
                return fail("Bad store target");
            }
            if (!take_target(StoreKind::kNewList, 0)) {
                return false;
            }
            std::memcpy(out->new_list, nm, len + 1);
            return true;
        }
        return fail("Bad store target");
    }

    // `sort_asc(l4)` sorts l4 IN PLACE in listexpr — a bare list argument makes
    // the call a statement rather than an expression. The expression tier
    // evaluates it by value (5.2.6, where this was deferred as "a commit
    // decision, not an expression one"), so the in-place half is recovered
    // here, in the one place that writes: the exact two-instruction program
    // `push-ref; sort` gets an implicit store back to the same ref. Anything
    // compound (`sort_asc(l1+1)`, `sort_asc(l1)*2`) is more than two
    // instructions and stays by value — which is listexpr's rule too.
    bool emit_in_place_sort() {
        if (out->n_code != 2) {
            return true;
        }
        const Instr& src = out->code[0];
        const Instr& call = out->code[1];
        if (src.op != Op::kPushList || call.op != Op::kCallList || call.a != 1 ||
            !list_fn_is_sort(call.b)) {
            return true;
        }
        return emit(Op::kStore, static_cast<uint8_t>(StoreKind::kList), src.b);
    }

    // mat2list writes its list arguments, which makes it a statement rather
    // than an expression — and unification is what made that distinction
    // load-bearing. Inside an expression, one term can rewrite a list another
    // term is holding: `l1 * mat2list([A], l1)` would multiply by l1's NEW
    // contents, because the operand stack holds the slot by reference. matexpr
    // forbade the composition ("mat2list must stand alone", mat_expr.cpp:1009)
    // and it stays forbidden for that reason, stated here as a shape rule on
    // the compiled program: the call must be its last instruction, and its
    // result cannot be stored.
    //
    // The in-place sort above is the same kind of statement and needs no rule,
    // because it only writes when it IS the whole program.
    bool check_statement_forms() {
        for (int i = 0; i < out->n_code; ++i) {
            const Instr& in = out->code[i];
            if (in.op != Op::kCallMat || !mat_fn_quotes_list_refs(in.b)) {
                continue;
            }
            if (i != out->n_code - 1 || store_kind != StoreKind::kNone) {
                return fail("mat2list must stand alone");
            }
        }
        return true;
    }

    // ---- driver ----------------------------------------------------------

    bool run() {
        for (;;) {
            skip_ws();
            const char c = *s;
            if (c == 0) {
                break;
            }

            if (expect_operand) {
                if (expect_var_name || expect_list_ref) {
                    const bool ok = expect_var_name ? var_name_operand() : list_ref_operand();
                    if (!ok) {
                        return false;
                    }
                    expect_var_name = false;
                    expect_list_ref = false;
                    expect_operand = false;
                    continue;
                }
                if (c == '[') {
                    if (!open_bracket()) {
                        return false;
                    }
                    continue;
                }
                if (c == '{') {
                    ++s;
                    OpTok t;
                    t.tok = Tok::kBrace;
                    t.argc = 1;  // corrected to 0 just below when empty
                    if (!push_op(t)) {
                        return false;
                    }
                    skip_ws();
                    if (*s == '}') {
                        ++s;
                        --n_ops;
                        if (!emit(Op::kMakeList, 0)) {
                            return false;
                        }
                        expect_operand = false;
                    }
                    continue;
                }
                if (c == '+' || c == '-') {
                    ++s;
                    OpTok t;
                    t.tok = Tok::kUnary;
                    t.ch = c;
                    t.prec = kUnaryPrec;
                    // Unary is right-associative: `--3` folds correctly.
                    t.right = true;
                    if (!push_op(t)) {
                        return false;
                    }
                    continue;
                }
                if (c == '(') {
                    ++s;
                    OpTok t;
                    t.tok = Tok::kLParen;
                    if (!push_op(t)) {
                        return false;
                    }
                    continue;
                }
                if (!operand()) {
                    return false;
                }
                continue;
            }

            // Operator position.
            // The store arrow, before '-' is read as subtraction. It ends the
            // expression: everything after it is the target.
            if (c == '-' && s[1] == '>') {
                s += 2;
                if (!store_target()) {
                    return false;
                }
                break;
            }
            if (c == ']') {
                ++s;
                if (!pop_while_tighter(0, false)) {
                    return false;
                }
                OpTok* t = top_op();
                if (t == nullptr || t->tok != Tok::kMatLit) {
                    return fail("Syntax error");
                }
                if (t->quoted) {  // close a row
                    const int width = t->fn - t->jump_at;
                    if (t->prec == 0) {
                        t->argc = static_cast<uint8_t>(width);  // first row sets the width
                    } else if (width != t->argc) {
                        return fail("Dim mismatch");
                    }
                    if (t->prec == 255) {
                        return fail("Matrix literal too large");
                    }
                    ++t->prec;  // rows
                    t->quoted = false;
                    continue;  // next is '[' for another row, or ']' to close
                }
                const OpTok top = *t;
                --n_ops;
                if (!emit_op(top)) {
                    return false;
                }
                expect_operand = false;
                continue;
            }
            if (c == '[') {  // another row of the literal being built
                OpTok* t = top_op();
                if (t == nullptr || t->tok != Tok::kMatLit || t->quoted) {
                    return fail("Syntax error");
                }
                if (!open_row(t)) {
                    return false;
                }
                expect_operand = true;
                continue;
            }
            if (c == '(') {  // element access: [A](row, col)
                ++s;
                OpTok t;
                t.tok = Tok::kIndex;
                t.argc = 1;
                if (!push_op(t)) {
                    return false;
                }
                expect_operand = true;
                continue;
            }
            // `[A]^T` — postfix transpose, tighter than anything else, so it
            // applies to the operand already emitted and needs no stack entry.
            // `^-1` and `^n` stay ordinary powers and are dispatched on the
            // base's kind at run time.
            if (c == '^' && (s[1] == 'T' || s[1] == 't') && !ident_char(s[2])) {
                s += 2;
                if (!emit(Op::kTranspose)) {
                    return false;
                }
                continue;
            }
            // `5!` — postfix factorial, the same shape. Both retired scalar
            // paths reached it by REWRITING the input to `fac(5)` before
            // parsing (engine.cpp:46 and complex_expr.cpp's
            // preprocess_factorial, a copy of it). A postfix operator needs no
            // rewrite here: emit the call against the operand already on the
            // stack. It binds tightest, so `2*4!` is 48 and `2^3!` is 64, and
            // `3!!` composes without the innermost-last rescan the rewrite
            // needed.
            if (c == '!') {
                ++s;
                if (!emit_factorial()) {
                    return false;
                }
                continue;
            }
            if (c == ')') {
                ++s;
                if (!pop_while_tighter(0, false)) {
                    return false;
                }
                OpTok* t = top_op();
                if (t == nullptr) {
                    return fail("Syntax error");
                }
                if (t->tok == Tok::kFunc && t->quoted && !close_quoted_body(t)) {
                    return false;  // `seq(expr)` — arity fails at run time
                }
                const OpTok top = *t;
                --n_ops;
                if (top.tok == Tok::kFunc || top.tok == Tok::kIndex) {
                    if (!emit_op(top)) {
                        return false;
                    }
                } else if (top.tok != Tok::kLParen) {
                    return fail("Syntax error");
                }
                expect_operand = false;
                continue;
            }
            if (c == '}') {
                ++s;
                if (!pop_while_tighter(0, false)) {
                    return false;
                }
                const OpTok* t = top_op();
                if (t == nullptr || t->tok != Tok::kBrace) {
                    return fail("Syntax error");
                }
                const OpTok top = *t;
                --n_ops;
                if (!emit_op(top)) {
                    return false;
                }
                expect_operand = false;
                continue;
            }
            if (c == ',') {
                ++s;
                if (!pop_while_tighter(0, false)) {
                    return false;
                }
                OpTok* fn = top_op();
                if (fn == nullptr || (fn->tok != Tok::kFunc && fn->tok != Tok::kBrace &&
                                      fn->tok != Tok::kIndex && fn->tok != Tok::kMatLit)) {
                    return fail("Syntax error");
                }
                if (fn->tok == Tok::kMatLit) {  // another element in this row
                    if (!fn->quoted) {
                        return fail("Syntax error");
                    }
                    if (fn->fn >= 255) {
                        return fail("Matrix literal too large");
                    }
                    ++fn->fn;
                    expect_operand = true;
                    continue;
                }
                if (fn->argc >= 255) {
                    return fail("Expression too complex");
                }
                if (fn->quoted) {
                    if (!close_quoted_body(fn)) {
                        return false;
                    }
                    expect_var_name = true;
                } else if (fn->refs) {
                    expect_list_ref = true;
                }
                ++fn->argc;
                expect_operand = true;
                continue;
            }
            if (precedence(c) != 0) {
                const int p = precedence(c);
                const bool right = c == '^';
                if (!pop_while_tighter(p, right)) {
                    return false;
                }
                ++s;
                OpTok t;
                t.tok = Tok::kBinary;
                t.ch = c;
                t.prec = static_cast<uint8_t>(p);
                t.right = right;
                if (!push_op(t)) {
                    return false;
                }
                expect_operand = true;
                continue;
            }
            return fail("Syntax error");
        }

        if (expect_operand) {
            return fail("Syntax error");  // trailing operator, or empty input
        }
        while (n_ops > 0) {
            const OpTok top = ops[--n_ops];
            if (top.tok != Tok::kBinary && top.tok != Tok::kUnary) {
                return fail("Syntax error");  // unbalanced
            }
            if (!emit_op(top)) {
                return false;
            }
        }
        if (!check_statement_forms()) {
            return false;
        }
        // Both stores come after the whole expression, and in this order: the
        // implicit sort target is the list the expression names, the explicit
        // one is where the result goes. `sort_asc(l4) -> l5` writes both, which
        // is what listexpr does (list_expr.cpp:1361-1395).
        if (!emit_in_place_sort()) {
            return false;
        }
        if (store_kind != StoreKind::kNone) {
            return emit(Op::kStore, static_cast<uint8_t>(store_kind), store_index);
        }
        return true;
    }
};

}  // namespace

bool compile(const char* src, Program& out, const char** err) {
    out.n_code = 0;
    out.n_consts = 0;
    out.new_list[0] = 0;
    if (err != nullptr) {
        *err = nullptr;
    }
    if (src == nullptr) {
        if (err != nullptr) {
            *err = "Syntax error";
        }
        return false;
    }

    // Compiler itself is a shallow local; its operator stack is the file-static
    // g_ops (see there for the non-reentrancy argument).
    Compiler c;
    c.s = src;
    c.out = &out;
    const bool ok = c.run();
    if (!ok && err != nullptr) {
        *err = c.err != nullptr ? c.err : "Syntax error";
    }
    return ok;
}

}  // namespace math::unified
