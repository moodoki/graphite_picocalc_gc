// Host-side tests for the Phase 3A list stack: Array/ArrayStore,
// list_ops, and the home-screen list expression layer. The PSRAM tier
// runs against the malloc shim in host_psram_backend.cpp.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/array.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/list_expr.hpp"
#include "math/list_ops.hpp"
#include "math/lists.hpp"

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

void check_near(double got, double expected, const char* what, double tol = 1e-9) {
    ++g_checks;
    if (std::isnan(got) || std::fabs(got - expected) > tol) {
        std::printf("FAIL: %s -> %.12g (expected %.12g)\n", what, got, expected);
        ++g_failures;
    }
}

math::listexpr::Result eval_list(const char* input) {
    return math::listexpr::evaluate(input);
}

void check_list_result(const char* input, const double* expected, int n) {
    ++g_checks;
    const auto res = eval_list(input);
    if (res.kind != math::listexpr::Kind::kList) {
        std::printf("FAIL: '%s' -> kind %d, error %s (expected list)\n", input,
                    static_cast<int>(res.kind), res.error != nullptr ? res.error : "-");
        ++g_failures;
        return;
    }
    if (res.list->size() != n) {
        std::printf("FAIL: '%s' -> size %d (expected %d)\n", input, res.list->size(), n);
        ++g_failures;
        return;
    }
    for (int i = 0; i < n; ++i) {
        if (std::fabs(res.list->get(i) - expected[i]) > 1e-9) {
            std::printf("FAIL: '%s' [%d] -> %.12g (expected %.12g)\n", input, i,
                        res.list->get(i), expected[i]);
            ++g_failures;
            return;
        }
    }
}

void check_list_error(const char* input, const char* expected_err) {
    ++g_checks;
    const auto res = eval_list(input);
    if (res.kind != math::listexpr::Kind::kError) {
        std::printf("FAIL: '%s' -> kind %d (expected error '%s')\n", input,
                    static_cast<int>(res.kind), expected_err);
        ++g_failures;
        return;
    }
    if (std::strcmp(res.error, expected_err) != 0) {
        std::printf("FAIL: '%s' -> error '%s' (expected '%s')\n", input, res.error, expected_err);
        ++g_failures;
    }
}

void test_array_basics() {
    math::Array a;
    check(a.size() == 0 && a.ndim() == 0, "default array empty");
    check(a.resize(5), "resize 5");
    check(a.size() == 5 && a.is_list() && !a.in_psram(), "5-elem SRAM list");
    for (int i = 0; i < 5; ++i) {
        a.set(i, i * 1.5);
    }
    check_near(a.get(3), 4.5, "get(3)");
    check(std::isnan(a.get(5)), "OOB get is NaN");
    check(std::isnan(a.get(-1)), "negative get is NaN");
    a.set(99, 7);  // No-op
    check_near(a.get(4), 6.0, "OOB set is a no-op");

    check(a.resize(8), "grow to 8");
    check_near(a.get(4), 6.0, "grow preserves data");
    check_near(a.get(7), 0.0, "grow zero-fills");

    check(a.dtype() == math::Dtype::kDouble, "dtype tag is double");

    check(!a.resize(math::Array::kMaxElements + 1), "cap 10000 enforced");
    check(a.size() == 8, "failed resize leaves array unchanged");

    // 2-D
    math::Array m;
    check(m.resize(3, 4), "3x4 matrix");
    check(m.is_matrix() && m.size() == 12 && m.dim(0) == 3 && m.dim(1) == 4, "matrix shape");
    m.set(2, 3, 42.0);
    check_near(m.get(2, 3), 42.0, "matrix get(r,c)");
    check_near(m.get(11), 42.0, "row-major flat index");
    check(std::isnan(m.get(1, 4)), "matrix OOB col");

    m.fill(2.5);
    check_near(m.get(0, 0) + m.get(2, 3), 5.0, "fill");
    m.clear();
    check(m.size() == 0, "clear empties");
}

void test_array_tiers() {
    math::Array a;
    check(a.resize(256) && !a.in_psram(), "256 stays SRAM");
    for (int i = 0; i < 256; ++i) {
        a.set(i, i);
    }
    check(a.resize(257) && a.in_psram(), "257 promotes to PSRAM");
    check_near(a.get(200), 200.0, "promotion preserves data");
    check_near(a.get(256), 0.0, "promotion zero-fills growth");
    a.set(256, -1);
    check(a.resize(10000), "grow to cap in PSRAM");
    check_near(a.get(256), -1.0, "in-region growth preserves");
    check(a.resize(100) && !a.in_psram(), "shrink demotes to SRAM");
    check_near(a.get(99), 99.0, "demotion preserves data");

    // Bulk range across chunk boundaries.
    math::Array b;
    b.resize(600);
    double chunk[600];
    for (int i = 0; i < 600; ++i) {
        chunk[i] = i * 0.5;
    }
    b.write_range(0, 600, chunk);
    double back[600] = {};
    b.read_range(0, 600, back);
    bool ok = true;
    for (int i = 0; i < 600; ++i) {
        ok = ok && back[i] == i * 0.5;
    }
    check(ok, "600-elem range roundtrip (PSRAM)");

    // Slab/region recycling: more create/destroy cycles than pool slots.
    const size_t psram_before = math::array_store().psram_used();
    for (int r = 0; r < 40; ++r) {
        math::Array t;
        t.resize(300);
        t.set(299, r);
    }
    check(math::array_store().psram_used() == psram_before, "regions recycle");
    const size_t sram_before = math::array_store().sram_used();
    for (int r = 0; r < 40; ++r) {
        math::Array t;
        t.resize(10);
    }
    check(math::array_store().sram_used() == sram_before, "slabs recycle");
}

void test_list_ops() {
    using namespace math;

    Array a;
    a.resize(4);
    const double v[] = {3, 1, 4, 1.5};
    a.write_range(0, 4, v);
    check_near(listops::sum(a), 9.5, "sum");
    check_near(listops::prod(a), 18.0, "prod");

    check(listops::sort_asc(a), "sort_asc small");
    check(a.get(0) == 1 && a.get(1) == 1.5 && a.get(2) == 3 && a.get(3) == 4, "sorted asc");
    check(listops::sort_desc(a), "sort_desc small");
    check(a.get(0) == 4 && a.get(3) == 1, "sorted desc");

    // NaN sorts last (ascending).
    Array nn;
    nn.resize(3);
    nn.set(0, 2);
    nn.set(1, std::nan(""));
    nn.set(2, 1);
    listops::sort_asc(nn);
    check(nn.get(0) == 1 && nn.get(1) == 2 && std::isnan(nn.get(2)), "NaN sorts last");

    // Large PSRAM-tier sort (external merge, non-power-of-two size).
    Array big;
    const int n = 5000;
    big.resize(n);
    for (int i = 0; i < n; ++i) {
        big.set(i, static_cast<double>((i * 7919) % 10007));
    }
    const double big_sum = listops::sum(big);
    check(big.in_psram(), "5000-elem list is PSRAM tier");
    check(listops::sort_asc(big), "external merge sort runs");
    bool monotonic = true;
    double prev = big.get(0);
    for (int i = 1; i < n; ++i) {
        const double cur = big.get(i);
        monotonic = monotonic && cur >= prev;
        prev = cur;
    }
    check(monotonic, "external sort is ordered");
    check_near(listops::sum(big), big_sum, "external sort preserves elements", 1e-6);

    // Boundary size: one merge level.
    Array edge;
    edge.resize(257);
    for (int i = 0; i < 257; ++i) {
        edge.set(i, 256.0 - i);
    }
    listops::sort_asc(edge);
    check(edge.get(0) == 0.0 && edge.get(256) == 256.0, "257-elem sort");

    Array out;
    a.resize(4);
    a.write_range(0, 4, v);
    check(listops::cumsum(a, out), "cumsum");
    check(out.size() == 4 && out.get(0) == 3 && out.get(3) == 9.5, "cumsum values");
    check(listops::delta_list(a, out), "delta_list");
    check(out.size() == 3 && out.get(0) == -2 && out.get(1) == 3 && out.get(2) == -2.5,
          "delta values");
    Array one;
    one.resize(1);
    check(listops::delta_list(one, out) && out.size() == 0, "delta of 1 elem is empty");
    check(!listops::cumsum(a, a), "cumsum rejects aliasing");

    const char* err = nullptr;
    check(listops::seq("x^2", 'x' - 'a', 1, 5, 1, out, &err), "seq compiles");
    check(out.size() == 5 && out.get(0) == 1 && out.get(4) == 25, "seq values");
    check(listops::seq("x", 'x' - 'a', 1, 2, 0.1, out, &err) && out.size() == 11,
          "seq 0.1 step hits endpoint");
    check(!listops::seq("x", 'x' - 'a', 1, 5, 0, out, &err), "seq step 0 fails");
    check(!listops::seq("x", 'x' - 'a', 5, 1, 1, out, &err), "seq wrong direction fails");
    check(!listops::seq("x", 'x' - 'a', 0, 20000, 1, out, &err), "seq past cap fails");
    check(!listops::seq("x+*2", 'x' - 'a', 1, 5, 1, out, &err), "seq syntax error fails");

    // seq must not clobber the engine variable.
    math::engine().vars()['x'] = 123.0;
    listops::seq("x", 'x' - 'a', 1, 3, 1, out, &err);
    check_near(math::engine().vars()['x'], 123.0, "seq restores var");

    Array copy_dst;
    check(listops::copy(big, copy_dst) && copy_dst.size() == n, "copy large");
    check_near(copy_dst.get(4999), big.get(4999), "copy values");
}

void test_list_expr() {
    using math::listexpr::Kind;

    // Reset l1..l6.
    for (int i = 0; i < math::ListStore::kCount; ++i) {
        math::lists().list(i).resize(0);
    }

    // Non-list inputs fall through.
    check(eval_list("2+2").kind == Kind::kNone, "scalar is kNone");
    check(eval_list("ln(2)").kind == Kind::kNone, "ln(2) is not a list token");
    check(eval_list("sin(x)+1").kind == Kind::kNone, "plain expr is kNone");

    const double lit[] = {1, 2, 3};
    check_list_result("{1, 2, 3}", lit, 3);

    // Store + reference.
    auto res = eval_list("{1,2,3}->l1");
    check(res.kind == Kind::kList && res.stored_list == 0 && res.lists_modified,
          "literal stores to l1");
    check(math::lists().list(0).size() == 3, "l1 has 3 elements");
    check_list_result("l1", lit, 3);

    const double dbl[] = {2, 4, 6};
    check_list_result("l1*2", dbl, 3);
    check_list_result("l1+l1", dbl, 3);

    res = eval_list("l1*2->l2");
    check(res.kind == Kind::kList && res.stored_list == 1, "elementwise store");
    const double sum12[] = {3, 6, 9};
    check_list_result("l1+l2", sum12, 3);

    res = eval_list("l1->l3");
    check(res.kind == Kind::kList && res.stored_list == 2 &&
              math::lists().list(2).size() == 3,
          "list copy via ->");

    // Literal elements are full expressions.
    const double pis[] = {3.141592653589793, 6.283185307179586};
    check_list_result("{pi, 2*pi}", pis, 2);

    // Reductions.
    res = eval_list("sum(l1)");
    check(res.kind == Kind::kScalar && res.scalar.ok, "sum(l1) is scalar");
    check_near(res.scalar.value, 6.0, "sum(l1) value");
    res = eval_list("1+sum(l1)*2");
    check_near(res.scalar.value, 13.0, "sum embeds in scalar expr");
    res = eval_list("prod(l1)");
    check_near(res.scalar.value, 6.0, "prod(l1)");
    res = eval_list("length(l1)");
    check_near(res.scalar.value, 3.0, "length(l1)");
    res = eval_list("sum(l1)->a");
    check(res.kind == Kind::kScalar && res.scalar.stored_var == 0, "scalar store works");
    check_near(math::engine().vars()['a'], 6.0, "a holds sum");

    // Reductions inside element-wise expressions.
    const double norm[] = {1.0 / 6, 2.0 / 6, 3.0 / 6};
    check_list_result("l1/sum(l1)", norm, 3);

    // Wrappers.
    eval_list("{3,1,2}->l4");
    res = eval_list("sort_asc(l4)");
    check(res.kind == Kind::kList && res.lists_modified, "in-place sort flags modify");
    check(math::lists().list(3).get(0) == 1 && math::lists().list(3).get(2) == 3,
          "sort_asc(l4) sorted in place");
    eval_list("{3,1,2}->l4");
    res = eval_list("sort_desc(l4+0)");
    check(res.kind == Kind::kList && !res.lists_modified, "compound sort is by value");
    check(math::lists().list(3).get(0) == 3.0, "l4 unchanged by value sort");
    check(res.list->get(0) == 3 && res.list->get(2) == 1, "value sort result");

    const double cs[] = {1, 3, 6};
    check_list_result("cumsum(l1)", cs, 3);
    const double dl[] = {1, 1};
    check_list_result("delta_list(l1)", dl, 2);
    const double nested[] = {1, 3, 6};
    check_list_result("cumsum(sort_asc({3,1,2}))", nested, 3);

    const double sq[] = {1, 4, 9, 16, 25};
    check_list_result("seq(x^2, x, 1, 5, 1)", sq, 5);
    res = eval_list("seq(x^2,x,1,5,1)->l5");
    check(res.stored_list == 4 && math::lists().list(4).size() == 5, "seq stores");

    // Errors.
    check_list_error("5->l1", "Store to l1-l6 needs a list");
    check_list_error("sum(l1)->l1", "Store to l1-l6 needs a list");
    eval_list("{1,2}->l6");
    check_list_error("l1+l6", "List length mismatch");
    check_list_error("seq(x,x,1,5)", "seq needs (expr, var, lo, hi, step)");
    check_list_error("seq(x,e,1,5,1)", "seq var must be a-z (not e/i) or theta");
    check_list_error("{1,foo}", "Bad list element");
    check_list_error("l1+*2", "Syntax error");

    // Empty lists.
    math::lists().list(5).resize(0);
    check_list_result("l6", nullptr, 0);
    check_list_result("l6*2", nullptr, 0);

    // Formatting.
    char buf[48];
    eval_list("{1,2.5,3}->l6");
    math::listexpr::format_list(math::lists().list(5), buf, sizeof(buf));
    check(std::strcmp(buf, "{1,2.5,3}") == 0, "format_list");
    math::lists().list(5).resize(0);
    math::listexpr::format_list(math::lists().list(5), buf, sizeof(buf));
    check(std::strcmp(buf, "{}") == 0, "format_list empty");
    char tiny[12];
    eval_list("{111111,222222,333333}->l6");
    math::listexpr::format_list(math::lists().list(5), tiny, sizeof(tiny));
    check(std::strchr(tiny, math::kEllipsisGlyph) != nullptr, "format_list truncates");

    // Compact per-element formatting (testdrive 2026-07-20): fractional
    // values round to 4 significant figures so more fit before truncation;
    // integers and short decimals are unchanged.
    eval_list("{1/3,2/3,4}->l6");
    math::listexpr::format_list(math::lists().list(5), buf, sizeof(buf));
    check(std::strcmp(buf, "{0.3333,0.6667,4}") == 0, "format_list compact float");

    // PSRAM-tier lists through the lift path.
    eval_list("seq(x, x, 1, 1000, 1)->l1");
    check(math::lists().list(0).in_psram(), "1000-elem l1 in PSRAM");
    res = eval_list("l1*3->l2");
    check(res.kind == Kind::kList, "large elementwise evaluates");
    check_near(math::lists().list(1).get(999), 3000.0, "large lift values");
    res = eval_list("sum(l1)");
    check_near(res.scalar.value, 500500.0, "large sum");
}

// D24: brace literals and wrapper calls as lift operands, range(), the
// mean/median/stdev reductions, and general reduction arguments.
void test_list_expr_d24() {
    using math::listexpr::Kind;
    eval_list("{1,2,3}->l1");

    // Brace literals broadcast (HW 2026-07-19 bug: "Expected a list" /
    // "Bad list element").
    {
        const double e[] = {3, 4, 5};
        check_list_result("{1,2,3}+2", e, 3);
    }
    {
        const double e[] = {2, 4, 6};
        check_list_result("{1,2,3}+{1,2,3}", e, 3);
    }
    {
        const double e[] = {11, 12, 13};
        check_list_result("10+{1,2,3}", e, 3);
    }
    {
        const double e[] = {2, 3, 4};
        check_list_result("l1+{1,1,1}", e, 3);
    }
    {
        const double e[] = {0, -1};
        check_list_result("{-1,-2}+1", e, 2);
    }
    {
        const double e[] = {2, 3, 4};
        check_list_result("{1,2,3}+1->l3", e, 3);
    }

    // range(lo, hi[, step]) — inclusive, default step toward hi.
    {
        const double e[] = {1, 2, 3, 4, 5};
        check_list_result("range(1,5)", e, 5);
    }
    {
        const double e[] = {5, 4, 3, 2, 1};
        check_list_result("range(5,1)", e, 5);
    }
    {
        const double e[] = {0, 0.5, 1};
        check_list_result("range(0,1,0.5)", e, 3);
    }

    // Wrapper calls compose inside expressions (lift operands).
    {
        const double e[] = {2, 4, 6};
        check_list_result("range(1,3)*2", e, 3);
    }
    {
        const double e[] = {2, 4, 7, 11};
        check_list_result("cumsum(range(1,4))+1", e, 4);
    }
    {
        const auto res = eval_list("range(1,100)->l2");
        check(res.kind == Kind::kList && res.stored_list == 1, "range store");
        check_near(math::lists().list(1).get(99), 100.0, "range store values");
    }

    // New reductions (bare args).
    check_near(eval_list("mean(l1)").scalar.value, 2.0, "mean(l1)");
    check_near(eval_list("median(l1)").scalar.value, 2.0, "median(l1)");
    check_near(eval_list("stdev(l1)").scalar.value, 1.0, "stdev(l1)");
    check_near(eval_list("std(l1)").scalar.value, 1.0, "std alias");

    // General reduction arguments (D22 bare-arg limitation lifted).
    check_near(eval_list("sum(range(1,100))").scalar.value, 5050.0, "sum(range)");
    check_near(eval_list("mean({1,2,3,4})").scalar.value, 2.5, "mean(literal)");
    check_near(eval_list("sum(l1*2)").scalar.value, 12.0, "sum(list expr)");
    check_near(eval_list("mean(l1)+sum(l1)").scalar.value, 8.0, "mixed reductions");
    check_near(eval_list("sum(cumsum(l1))").scalar.value, 10.0, "sum(cumsum(l1))");

    // Errors.
    check_list_error("{1,2}+{1,2,3}", "List length mismatch");
    check_list_error("range(1,2,-1)", "Bad seq range");
    check_list_error("range(1)", "range needs (lo, hi[, step])");
    check_list_error("{1}+{2}+{3}+{4}+{5}", "Too many list terms");
    check_list_error("stdev({5})", "Undefined result");
}

}  // namespace

int main() {
    test_array_basics();
    test_array_tiers();
    test_list_ops();
    test_list_expr();
    test_list_expr_d24();

    std::printf("test_lists: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
