// Host-side tests for the sequence-mode evaluator (math::seqexpr,
// 4D.6): lag rewriting, seeds, u/v/w cross-references, the lockstep
// memo, and web-plot eligibility.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/engine.hpp"
#include "math/seq_expr.hpp"

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

void check_val(double got, double expected, const char* what, double tol = 1e-9) {
    ++g_checks;
    if (std::isnan(got) || std::fabs(got - expected) > tol) {
        std::printf("FAIL: %s -> %.12g (expected %.12g)\n", what, got, expected);
        ++g_failures;
    }
}

math::seqexpr::SeqDef def3(const char* u, const char* v, const char* w, double u1 = 0,
                           double v1 = 0, double w1 = 0, long n_min = 1) {
    math::seqexpr::SeqDef d;
    d.expr[0] = u;
    d.expr[1] = v;
    d.expr[2] = w;
    d.seed1[0] = u1;
    d.seed1[1] = v1;
    d.seed1[2] = w1;
    d.n_min = n_min;
    return d;
}

void test_basic_recurrence() {
    using namespace math::seqexpr;
    // u(n) = u(n-1) + 1, u(1) = 1  ->  u(n) = n
    check(begin(def3("u(n-1)+1", "", "", 1)), "basic compile");
    check(defined(0), "u defined");
    check(!defined(1), "v undefined");
    check_val(value(0, 1), 1, "u(1) = seed");
    check_val(value(0, 5), 5, "u(5)");
    check_val(value(0, 2), 2, "u(2) after backward jump");
    check(std::isnan(value(0, 0)), "n below nMin is NaN");
    check(std::isnan(value(1, 3)), "undefined seq is NaN");
}

void test_explicit_formula() {
    using namespace math::seqexpr;
    // No lags: evaluate directly at any n; seed unused.
    check(begin(def3("n^2", "", "", 99)), "explicit compile");
    check_val(value(0, 4), 16, "n^2 at 4");
    check_val(value(0, 1), 1, "n^2 at 1");
    check(!lag1_only(0), "explicit is not web-eligible");
}

void test_geometric() {
    using namespace math::seqexpr;
    // u(n) = 0.5*u(n-1), u(1) = 8: 8, 4, 2, 1, ...
    check(begin(def3("0.5*u(n-1)", "", "", 8)), "geometric compile");
    check_val(value(0, 4), 1, "geometric u(4)");
    check(lag1_only(0), "own-lag-1 is web-eligible");
    check_val(map_value(0, 6), 3, "map f(6) = 3");
}

void test_fibonacci_lag2() {
    using namespace math::seqexpr;
    // u(n) = u(n-1) + u(n-2), u(1)=1, u(2)=1 (seed2).
    auto d = def3("u(n-1)+u(n-2)", "", "", 1);
    d.seed2[0] = 1;
    check(begin(d), "fibonacci compile");
    check(uses_lag2(0), "lag-2 detected");
    check(!lag1_only(0), "lag-2 not web-eligible");
    check_val(value(0, 1), 1, "fib(1)");
    check_val(value(0, 2), 1, "fib(2) = seed2");
    check_val(value(0, 7), 13, "fib(7)");
    check_val(value(0, 10), 55, "fib(10)");
}

void test_cross_reference() {
    using namespace math::seqexpr;
    // u counts up; v doubles u's previous value: v(n) = 2*u(n-1).
    check(begin(def3("u(n-1)+1", "2*u(n-1)", "", 1, 0)), "cross-ref compile");
    check_val(value(0, 4), 4, "u(4)");
    check_val(value(1, 4), 6, "v(4) = 2*u(3)");
    check(!lag1_only(1), "cross-ref not web-eligible");

    // Mutual reference: u(n)=v(n-1)+1, v(n)=u(n-1)+1, both seeded 0.
    check(begin(def3("v(n-1)+1", "u(n-1)+1", "")), "mutual compile");
    check_val(value(0, 3), 2, "mutual u(3)");
    check_val(value(1, 3), 2, "mutual v(3)");
}

void test_three_sequences() {
    using namespace math::seqexpr;
    // w sums the other two at n-1.
    check(begin(def3("u(n-1)+1", "2*v(n-1)", "u(n-1)+v(n-1)", 1, 1, 0)), "three compile");
    // u: 1,2,3,4  v: 1,2,4,8  w(4) = u(3)+v(3) = 3+4 = 7
    check_val(value(2, 4), 7, "w(4) = u(3)+v(3)");
}

void test_bad_forms() {
    using namespace math::seqexpr;
    check(begin(def3("u(n)", "", "", 1)) == false, "circular u(n) rejected");
    check(!defined(0), "circular u undefined");
    begin(def3("u(n-3)", "", "", 1));
    check(!defined(0), "u(n-3) rejected");
    begin(def3("u(3)", "", "", 1));
    check(!defined(0), "u(3) rejected");
    begin(def3("nosuchfn(2)", "", "", 1));
    check(!defined(0), "unknown fn rejected");
}

void test_engine_vars_in_expr() {
    using namespace math::seqexpr;
    // A referenced home-screen variable participates; refresh() picks
    // up edits on the next sweep.
    math::engine().vars().set_real('k' - 'a', 3);
    check(begin(def3("u(n-1)+k", "", "", 0)), "var compile");
    check_val(value(0, 3), 6, "u(3) with k=3");
    math::engine().vars().set_real('k' - 'a', 5);
    refresh();
    check_val(value(0, 3), 10, "u(3) with k=5 after refresh");
    math::engine().vars().set_real('k' - 'a', 0);
}

void test_nmin_and_cap() {
    using namespace math::seqexpr;
    check(begin(def3("u(n-1)*2", "", "", 1, 0, 0, 5)), "nMin=5 compile");
    check_val(value(0, 5), 1, "seed at nMin=5");
    check_val(value(0, 8), 8, "u(8) from nMin=5");
    check(std::isnan(value(0, 4)), "below nMin=5 NaN");
    check(std::isnan(value(0, 5 + kMaxN + 1)), "beyond kMaxN NaN");
}

void test_recompile_only_on_change() {
    using namespace math::seqexpr;
    // Same definition twice: the memo survives (forward sweep resumes).
    auto d = def3("u(n-1)+1", "", "", 1);
    check(begin(d), "memo compile");
    check_val(value(0, 100), 100, "u(100)");
    check(begin(d), "unchanged begin");
    check_val(value(0, 101), 101, "u(101) incremental");
    // A changed seed recompiles and restarts.
    d.seed1[0] = 11;
    check(begin(d), "changed seed recompile");
    check_val(value(0, 3), 13, "u(3) with new seed");
}

}  // namespace

int main() {
    test_basic_recurrence();
    test_explicit_formula();
    test_geometric();
    test_fibonacci_lag2();
    test_cross_reference();
    test_three_sequences();
    test_bad_forms();
    test_engine_vars_in_expr();
    test_nmin_and_cap();
    test_recompile_only_on_change();

    std::printf("test_seq: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
