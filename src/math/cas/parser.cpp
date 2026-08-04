#include "math/cas/parser.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace math::cas {

namespace {

// Known multi-character function names (Stage 0 set; extended in Stage 2).
// A run of letters is treated as one of these only when it is not followed
// by an alphanumeric char AND (for functions) is followed by '('. Otherwise
// letters split into single-char variables so implicit multiplication works
// (xy -> x*y), matching CAS-mode entry (spec §4, P5-3).
const char* const kFuncNames[] = {
    "sinh", "cosh", "tanh", "asin", "acos", "atan", "sin",
    "cos",  "tan",  "exp",  "log",  "ln",   "sqrt", "abs",
};

// Named constants (nullary FUNC nodes), matched without a following '('.
const char* const kConstNames[] = {"pi"};

// Nesting cap (D45). Every level of parens, function argument or unary sign
// costs a chain of parse_* frames, and the tree it builds is then walked
// recursively afterwards by simplify / deriv / integrate_rec. That walk, not
// the parse, is what sets this number.
//
// Measured on the linked Pico 1 build, the deepest-recursing CAS frame is
// integrate_rec at 172 B (simplify_sum/simplify_product and deriv_product
// were the big ones at ~1.1 KB and 256 B; both now take their arrays from the
// pool). 12 x 172 B = ~2.1 KB, plus evaluate_home's 580 B base, keeps a CAS
// operation near 2.7 KB — inside the 4 KB core 0 has before it reaches core
// 1's stack, with margin. Hand-entered expressions sit at 2-4 levels;
// sin(cos(tan(x))) is 3.
constexpr int kMaxDepth = 12;

struct Parser {
    const char* p = nullptr;
    const char** err = nullptr;
    bool failed = false;
    int depth = 0;

    void fail(const char* msg) {
        if (!failed) {
            failed = true;
            if (err != nullptr) {
                *err = msg;
            }
        }
    }

    void skip_spaces() {
        while (*p == ' ' || *p == '\t') {
            ++p;
        }
    }

    char peek() {
        skip_spaces();
        return *p;
    }

    // True if the next operand-starting char permits implicit multiplication.
    bool starts_operand(char c) const {
        return std::isdigit(static_cast<unsigned char>(c)) || c == '.' ||
               std::isalpha(static_cast<unsigned char>(c)) || c == '(';
    }

    // Every nested group re-enters here (parens in parse_atom, function
    // arguments in parse_identifier), so this is where nesting is counted.
    Expr* parse_equation() {
        if (depth >= kMaxDepth) {
            fail("too deeply nested");
            return nullptr;
        }
        ++depth;
        Expr* r = parse_equation_inner();
        --depth;
        return r;
    }

    Expr* parse_equation_inner() {
        Expr* lhs = parse_add();
        if (lhs == nullptr) {
            return nullptr;
        }
        if (peek() == '=') {
            ++p;
            Expr* rhs = parse_add();
            if (rhs == nullptr) {
                return nullptr;
            }
            return Expr::eq(lhs, rhs);
        }
        return lhs;
    }

    Expr* parse_add() {
        Expr* lhs = parse_mul();
        if (lhs == nullptr) {
            return nullptr;
        }
        for (;;) {
            const char c = peek();
            if (c == '+' || c == '-') {
                ++p;
                Expr* rhs = parse_mul();
                if (rhs == nullptr) {
                    return nullptr;
                }
                lhs = (c == '+') ? Expr::add(lhs, rhs) : Expr::add(lhs, Expr::neg(rhs));
                if (lhs == nullptr) {
                    fail("out of memory");
                    return nullptr;
                }
            } else {
                break;
            }
        }
        return lhs;
    }

    Expr* parse_mul() {
        Expr* lhs = parse_unary();
        if (lhs == nullptr) {
            return nullptr;
        }
        for (;;) {
            const char c = peek();
            if (c == '*' || c == '/') {
                ++p;
                Expr* rhs = parse_unary();
                if (rhs == nullptr) {
                    return nullptr;
                }
                lhs = (c == '*') ? Expr::mul(lhs, rhs)
                                 : Expr::mul(lhs, Expr::pow(rhs, Expr::num(-1.0)));
            } else if (starts_operand(c)) {
                // Implicit multiplication: 2x, xy, 2(x+1), (x+1)(x-1).
                Expr* rhs = parse_unary();
                if (rhs == nullptr) {
                    return nullptr;
                }
                lhs = Expr::mul(lhs, rhs);
            } else {
                break;
            }
            if (lhs == nullptr) {
                fail("out of memory");
                return nullptr;
            }
        }
        return lhs;
    }

    Expr* parse_unary() {
        const char c = peek();
        if (c != '-' && c != '+') {
            return parse_power();
        }
        // A sign chain (`---x`) recurses without passing through
        // parse_equation, so it needs its own count against the same cap.
        if (depth >= kMaxDepth) {
            fail("too deeply nested");
            return nullptr;
        }
        ++depth;
        Expr* operand = nullptr;
        ++p;
        operand = parse_unary();
        --depth;
        if (operand == nullptr) {
            return nullptr;
        }
        if (c == '+') {
            return operand;
        }
        Expr* r = Expr::neg(operand);
        if (r == nullptr) {
            fail("out of memory");
        }
        return r;
    }

    Expr* parse_power() {
        Expr* base = parse_atom();
        if (base == nullptr) {
            return nullptr;
        }
        if (peek() == '^') {
            ++p;
            Expr* exp = parse_unary();  // right-associative; allows x^-1, x^2^3
            if (exp == nullptr) {
                return nullptr;
            }
            Expr* r = Expr::pow(base, exp);
            if (r == nullptr) {
                fail("out of memory");
            }
            return r;
        }
        return base;
    }

    Expr* parse_atom() {
        const char c = peek();
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            char* end = nullptr;
            const double v = std::strtod(p, &end);
            if (end == p) {
                fail("bad number");
                return nullptr;
            }
            p = end;
            Expr* r = Expr::num(v);
            if (r == nullptr) {
                fail("out of memory");
            }
            return r;
        }
        if (c == '(') {
            ++p;
            Expr* inner = parse_equation();
            if (inner == nullptr) {
                return nullptr;
            }
            if (peek() != ')') {
                fail("expected ')'");
                return nullptr;
            }
            ++p;
            return inner;
        }
        if (std::isalpha(static_cast<unsigned char>(c))) {
            return parse_identifier();
        }
        fail("unexpected character");
        return nullptr;
    }

    Expr* parse_identifier() {
        skip_spaces();

        // Try a function name: matches, not followed by alnum, then '('.
        for (const char* name : kFuncNames) {
            const std::size_t len = std::strlen(name);
            if (std::strncmp(p, name, len) == 0 &&
                !std::isalnum(static_cast<unsigned char>(p[len]))) {
                const char* after = p + len;
                while (*after == ' ' || *after == '\t') {
                    ++after;
                }
                if (*after == '(') {
                    p = after + 1;
                    Expr* arg = parse_equation();
                    if (arg == nullptr) {
                        return nullptr;
                    }
                    if (peek() != ')') {
                        fail("expected ')'");
                        return nullptr;
                    }
                    ++p;
                    Expr* r = Expr::func(name, arg);
                    if (r == nullptr) {
                        fail("out of memory");
                    }
                    return r;
                }
            }
        }

        // Try a named constant (pi), matched without '('.
        for (const char* name : kConstNames) {
            const std::size_t len = std::strlen(name);
            if (std::strncmp(p, name, len) == 0 &&
                !std::isalpha(static_cast<unsigned char>(p[len]))) {
                p += len;
                Expr* r = Expr::func(name, nullptr);
                if (r == nullptr) {
                    fail("out of memory");
                }
                return r;
            }
        }

        // Otherwise a single-char variable (implicit-mult friendly).
        const char name = *p;
        ++p;
        Expr* r = Expr::var(name);
        if (r == nullptr) {
            fail("out of memory");
        }
        return r;
    }
};

}  // namespace

Expr* parse_expr(const char* input, const char** error) {
    if (error != nullptr) {
        *error = nullptr;
    }
    if (input == nullptr) {
        if (error != nullptr) {
            *error = "empty input";
        }
        return nullptr;
    }
    Parser parser{input, error};
    Expr* result = parser.parse_equation();
    if (parser.failed) {
        return nullptr;
    }
    if (result == nullptr) {
        if (error != nullptr && *error == nullptr) {
            *error = "empty input";
        }
        return nullptr;
    }
    if (parser.peek() != '\0') {
        if (error != nullptr) {
            *error = "unexpected trailing input";
        }
        return nullptr;
    }
    return result;
}

}  // namespace math::cas
