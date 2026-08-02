// Host-side tests for the CAS foundations (Phase 5 Stage 0, tasks 4D.1-4D.3):
// the Expr tree + pool (expr.cpp), the recursive-descent parser (parser.cpp),
// and expr_to_string (serialize.cpp). No numeric evaluation yet — that starts
// with the Stage 1 simplifier.

#include <cstdio>
#include <cstring>

#include "math/cas/expr.hpp"
#include "math/cas/parser.hpp"
#include "math/cas/serialize.hpp"
#include "math/cas/simplify.hpp"

using math::cas::Expr;
using math::cas::ExprType;
using math::cas::g_cas_pool;
using math::cas::parse_expr;
using math::cas::simplify;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

// parse(s) succeeds and its serialization re-parses to a structurally equal
// tree (round-trip stability — spec §4 task 4D.3).
void check_roundtrip(const char* s) {
    g_cas_pool.reset();
    const char* err = nullptr;
    Expr* t1 = parse_expr(s, &err);
    if (t1 == nullptr) {
        ++g_checks;
        ++g_failures;
        std::printf("FAIL: parse(\"%s\") -> null (%s)\n", s, err ? err : "?");
        return;
    }
    char buf[256];
    math::cas::expr_to_string(t1, buf, sizeof(buf));
    Expr* t2 = parse_expr(buf, &err);
    ++g_checks;
    if (t2 == nullptr) {
        ++g_failures;
        std::printf("FAIL: reparse(\"%s\") of \"%s\" -> null (%s)\n", buf, s, err ? err : "?");
        return;
    }
    if (!t1->equals(t2)) {
        ++g_failures;
        std::printf("FAIL: round-trip \"%s\" -> \"%s\" not structurally equal\n", s, buf);
    }
}

// parse(a) and parse(b) yield structurally equal trees.
void check_same(const char* a, const char* b) {
    g_cas_pool.reset();
    Expr* ta = parse_expr(a, nullptr);
    Expr* tb = parse_expr(b, nullptr);
    ++g_checks;
    if (ta == nullptr || tb == nullptr) {
        ++g_failures;
        std::printf("FAIL: parse null in check_same(\"%s\",\"%s\")\n", a, b);
        return;
    }
    if (!ta->equals(tb)) {
        ++g_failures;
        std::printf("FAIL: \"%s\" != \"%s\" structurally\n", a, b);
    }
}

void test_tree_ops() {
    g_cas_pool.reset();
    // Build 2*x^2 + 3*x - 1 by hand.
    Expr* t = Expr::add(Expr::add(Expr::mul(Expr::num(2), Expr::pow(Expr::var('x'), Expr::num(2))),
                                  Expr::mul(Expr::num(3), Expr::var('x'))),
                        Expr::neg(Expr::num(1)));
    check(t != nullptr, "hand-built tree non-null");
    check(t->is_add(), "top is ADD");
    check(t->child_count() == 3, "ADD flattened to 3 children");
    check(t->contains('x'), "contains x");
    check(!t->contains('y'), "does not contain y");

    Expr* c = t->clone();
    check(c != nullptr, "clone non-null");
    check(c != t, "clone is a distinct node");
    check(t->equals(c), "clone equals original");

    // Structural inequality.
    Expr* other = Expr::add(Expr::var('x'), Expr::num(1));
    check(!t->equals(other), "different trees not equal");

    // Predicates on atoms.
    check(Expr::num(0)->is_zero(), "num(0) is_zero");
    check(Expr::num(1)->is_one(), "num(1) is_one");
    check(Expr::num(-1)->is_neg_one(), "num(-1) is_neg_one");
    check(Expr::var('i')->is_var(), "var(i) is_var");
}

void test_parse_structure() {
    g_cas_pool.reset();
    const char* err = nullptr;
    Expr* t = parse_expr("2*x^2 + 3*x - 1", &err);
    check(t != nullptr, "parse quadratic non-null");
    if (t != nullptr) {
        check(t->is_add(), "quadratic top is ADD");
        check(t->child_count() == 3, "quadratic has 3 terms");
        // First term 2*x^2 is a MUL of 2 and x^2.
        Expr* term0 = t->child;
        check(term0->is_mul(), "first term is MUL");
        check(term0->child_count() == 2, "first term MUL has 2 factors");
    }
    // Equation form.
    Expr* eq = parse_expr("x^2 = 4", &err);
    check(eq != nullptr && eq->is_eq(), "equation parses to EQ");
}

void test_roundtrips() {
    const char* cases[] = {
        "2*x^2 + 3*x - 1", "x^2 + 1",     "(x+1)*(x-1)",  "sin(x) + cos(x)",
        "2*x/(4*x^2)",     "-x + 1",      "x^-1",         "3 + 2*i",
        "pi",              "2*pi",        "a = b + c",    "x^2^3",
        "sqrt(x+1)",       "ln(x)/x",     "-(x + 1)",     "2*(x + 3)^4",
    };
    for (const char* s : cases) {
        check_roundtrip(s);
    }
}

void test_implicit_mult() {
    check_same("2x", "2*x");
    check_same("xy", "x*y");
    check_same("2(x+1)", "2*(x+1)");
    check_same("(x+1)(x-1)", "(x+1)*(x-1)");
    check_same("3x^2", "3*x^2");
    check_same("2pi", "2*pi");
}

void test_encodings() {
    // Subtraction encodes as ADD(a, NEG(b)).
    check_same("a - b", "a + (-b)");
    // Right-associative exponentiation.
    check_same("2^3^2", "2^(3^2)");
    check(!parse_expr("2^3^2", nullptr)->equals(parse_expr("(2^3)^2", nullptr)),
          "^ is right-associative, not left");
}

void test_errors_and_exhaustion() {
    g_cas_pool.reset();
    const char* err = nullptr;
    check(parse_expr("2 +", &err) == nullptr, "trailing operator errors");
    check(parse_expr("(x+1", &err) == nullptr, "unbalanced paren errors");
    check(parse_expr("2 2 x", nullptr) != nullptr, "implicit mult of literals ok");

    // Pool exhaustion returns nullptr rather than crashing.
    g_cas_pool.reset();
    int n = 0;
    while (n < 1000000 && Expr::num(1.0) != nullptr) {
        ++n;
    }
    check(n > 100, "pool holds a reasonable number of nodes");
    check(n < 1000000, "pool eventually reports exhaustion");
    check(Expr::add(Expr::num(1), Expr::num(2)) == nullptr, "alloc fails cleanly when full");
    g_cas_pool.reset();
    check(Expr::num(1) != nullptr, "reset reclaims the pool");
}

// simplify(input) and simplify(expected) produce structurally equal trees.
void check_simplifies(const char* input, const char* expected) {
    g_cas_pool.reset();
    Expr* a = parse_expr(input, nullptr);
    Expr* b = parse_expr(expected, nullptr);
    ++g_checks;
    if (a == nullptr || b == nullptr) {
        ++g_failures;
        std::printf("FAIL: parse null in check_simplifies(\"%s\",\"%s\")\n", input, expected);
        return;
    }
    Expr* sa = simplify(a);
    Expr* sb = simplify(b);
    if (sa == nullptr || sb == nullptr || !sa->equals(sb)) {
        ++g_failures;
        char got[128] = "?";
        char want[128] = "?";
        if (sa != nullptr) {
            math::cas::expr_to_string(sa, got, sizeof(got));
        }
        if (sb != nullptr) {
            math::cas::expr_to_string(sb, want, sizeof(want));
        }
        std::printf("FAIL: simplify(\"%s\") = \"%s\", expected form of \"%s\" = \"%s\"\n", input,
                    got, expected, want);
    }
}

// simplify(input) is exactly the numeric literal `value`.
void check_simplify_num(const char* input, double value) {
    g_cas_pool.reset();
    Expr* a = parse_expr(input, nullptr);
    ++g_checks;
    if (a == nullptr) {
        ++g_failures;
        std::printf("FAIL: parse null in check_simplify_num(\"%s\")\n", input);
        return;
    }
    Expr* s = simplify(a);
    if (s == nullptr || !s->is_num() || s->num_val != value) {
        ++g_failures;
        std::printf("FAIL: simplify(\"%s\") not num %.12g\n", input, value);
    }
}

void test_simplify_identity() {
    check_simplifies("x + 0", "x");
    check_simplifies("0 + x", "x");
    check_simplifies("x*1", "x");
    check_simplifies("1*x", "x");
    check_simplify_num("x*0", 0.0);
    check_simplify_num("x^0", 1.0);
    check_simplifies("x^1", "x");
    check_simplifies("-(-x)", "x");
    check_simplify_num("0^5", 0.0);
    check_simplify_num("1^x", 1.0);
}

void test_simplify_constfold() {
    check_simplify_num("2 + 3", 5.0);
    check_simplify_num("4*5", 20.0);
    check_simplify_num("2^10", 1024.0);
    check_simplify_num("2 - 2", 0.0);
    check_simplify_num("10/2", 5.0);
    check_simplify_num("sin(0)", 0.0);
    check_simplify_num("cos(0)", 1.0);
    check_simplify_num("ln(1)", 0.0);
    check_simplify_num("sqrt(0)", 0.0);
    check_simplify_num("sqrt(1)", 1.0);
    check_simplify_num("abs(-3)", 3.0);
}

void test_simplify_like_terms() {
    check_simplifies("3x + 5x", "8x");
    check_simplifies("x + x", "2x");
    check_simplifies("2x + 3y + x", "3x + 3y");
    check_simplify_num("x - x", 0.0);
    check_simplifies("y + x + 1", "x + y + 1");
}

void test_simplify_like_factors() {
    check_simplifies("x^2 * x^3", "x^5");
    check_simplifies("x*x", "x^2");
    check_simplifies("x*x*x", "x^3");
    check_simplifies("2*x*3", "6x");
}

void test_simplify_imaginary() {
    check_simplify_num("i*i", -1.0);
    check_simplify_num("i^2", -1.0);
    check_simplifies("i^3", "-i");
    check_simplify_num("i^4", 1.0);
    check_simplify_num("2*i*i", -2.0);
    check_simplify_num("i*i*i*i", 1.0);
}

void test_simplify_fractions() {
    check_simplify_num("x/x", 1.0);
    check_simplifies("a/a", "1");
    check_simplifies("2x/(4x^2)", "1/(2x)");
    check_simplifies("6x^3/(2x)", "3x^2");
}

void test_simplify_commutativity() {
    check_simplifies("x + y", "y + x");
    check_simplifies("x*y", "y*x");
    check_simplifies("3x + 5x", "5x + 3x");
}

void test_simplify_termination() {
    // Risk-1 tricky expressions must terminate (no infinite rule cycling).
    g_cas_pool.reset();
    check(simplify(parse_expr("0^0", nullptr)) != nullptr, "0^0 terminates");
    g_cas_pool.reset();
    check(simplify(parse_expr("x/x", nullptr)) != nullptr, "x/x terminates");
    g_cas_pool.reset();
    Expr* big = simplify(parse_expr("(x+y)^20", nullptr));
    check(big != nullptr && big->is_pow(), "(x+y)^20 stays a POW (not expanded)");
    g_cas_pool.reset();
    check(simplify(parse_expr("i^4", nullptr)) != nullptr, "i^4 terminates");
}

}  // namespace

int main() {
    test_tree_ops();
    test_parse_structure();
    test_roundtrips();
    test_implicit_mult();
    test_encodings();
    test_errors_and_exhaustion();
    test_simplify_identity();
    test_simplify_constfold();
    test_simplify_like_terms();
    test_simplify_like_factors();
    test_simplify_imaginary();
    test_simplify_fractions();
    test_simplify_commutativity();
    test_simplify_termination();

    std::printf("test_cas: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
