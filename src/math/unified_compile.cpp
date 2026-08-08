#include <cctype>
#include <cstdlib>
#include <cstring>

#include "math/catalog.hpp"
#include "math/engine.hpp"
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

// Operator-stack entries. Function calls and parens ride the same stack so
// that argument separation and grouping fall out of one mechanism.
enum class Tok : uint8_t { kBinary, kUnary, kLParen, kFunc };

struct OpTok {
    Tok tok = Tok::kBinary;
    char ch = 0;         // '+' '-' '*' '/' '^' for kBinary/kUnary
    uint8_t prec = 0;    // higher binds tighter
    bool right = false;  // right-associative (only '^')
    uint16_t fn = 0;     // catalog index, kFunc only
    uint8_t argc = 0;    // arguments seen so far, kFunc only
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
// (engine.cpp:36), and it must be re-checked if 5.2.6 ever wants a sub-compile.
OpTok g_ops[kMaxStack];

struct Compiler {
    const char* s = nullptr;
    Program* out = nullptr;
    const char* err = nullptr;

    OpTok* ops = g_ops;
    int n_ops = 0;

    // Shunting-yard needs to know whether the next token is an operand or an
    // operator: it is what distinguishes unary minus from subtraction.
    bool expect_operand = true;

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
        if (t.tok == Tok::kFunc) {
            return emit(Op::kCall, t.argc, t.fn);
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
            if (top.tok == Tok::kLParen || top.tok == Tok::kFunc) {
                break;
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
        return fail("Syntax error");
    }

    // An identifier followed by '(' — resolved against the shared catalog,
    // which is the same table tinyexpr registers from (engine.cpp:194-200), so
    // absorbing it costs an arity dispatcher rather than sixty ports.
    bool function_call(const char* name, size_t len) {
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

    // ---- driver ----------------------------------------------------------

    bool run() {
        for (;;) {
            skip_ws();
            const char c = *s;
            if (c == 0) {
                break;
            }

            if (expect_operand) {
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
            if (c == ')') {
                ++s;
                if (!pop_while_tighter(0, false)) {
                    return false;
                }
                const OpTok* t = top_op();
                if (t == nullptr) {
                    return fail("Syntax error");
                }
                const OpTok top = *t;
                --n_ops;
                if (top.tok == Tok::kFunc) {
                    if (!emit_op(top)) {
                        return false;
                    }
                } else if (top.tok != Tok::kLParen) {
                    return fail("Syntax error");
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
                if (fn == nullptr || fn->tok != Tok::kFunc) {
                    return fail("Syntax error");
                }
                if (fn->argc >= 255) {
                    return fail("Expression too complex");
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
            if (top.tok == Tok::kLParen || top.tok == Tok::kFunc) {
                return fail("Syntax error");  // unbalanced
            }
            if (!emit_op(top)) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace

bool compile(const char* src, Program& out, const char** err) {
    out.n_code = 0;
    out.n_consts = 0;
    out.n_elem_slots = 0;
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
