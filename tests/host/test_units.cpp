// Host-side tests for unit conversions (math::unitexpr, 4D.18) and the
// scientific-constants engine binding (4D.17).

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/catalog.hpp"
#include "math/engine.hpp"
#include "math/units.hpp"

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

void check_convert(double v, const char* from, const char* to, double expected, const char* what,
                   double tol = 1e-9) {
    ++g_checks;
    double out = 0;
    const char* err = nullptr;
    if (!math::unitexpr::convert_value(v, from, to, &out, &err) ||
        std::fabs(out - expected) > tol) {
        std::printf("FAIL: %s -> %.12g err='%s' (expected %.12g)\n", what, out,
                    err != nullptr ? err : "-", expected);
        ++g_failures;
    }
}

void check_sub(const char* input, const char* expect_prefix, const char* what) {
    ++g_checks;
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s", input);
    const char* err = nullptr;
    if (!math::unitexpr::substitute(buf, sizeof(buf), &err)) {
        std::printf("FAIL: %s: '%s' -> err '%s'\n", what, input, err != nullptr ? err : "-");
        ++g_failures;
        return;
    }
    if (std::strncmp(buf, expect_prefix, std::strlen(expect_prefix)) != 0) {
        std::printf("FAIL: %s: '%s' -> '%s' (expected prefix '%s')\n", what, input, buf,
                    expect_prefix);
        ++g_failures;
    }
}

void test_convert_value() {
    check_convert(1, "mi", "km", 1.609344, "mi -> km");
    check_convert(1, "km", "mi", 1.0 / 1.609344, "km -> mi");
    check_convert(12, "in", "ft", 1.0, "in -> ft");
    check_convert(1, "kg", "lb", 2.2046226218487757, "kg -> lb", 1e-12);
    check_convert(1, "hr", "min", 60, "hr -> min");
    check_convert(100, "km/h", "m/s", 27.777777777777779, "km/h -> m/s", 1e-9);
    check_convert(1, "acre", "m2", 4046.8564224, "acre -> m2");
    check_convert(1, "gal", "l", 3.785411784, "gal -> l");
    check_convert(100, "c", "f", 212, "100C -> F", 1e-9);
    check_convert(32, "f", "c", 0, "32F -> C", 1e-9);
    check_convert(0, "c", "k", 273.15, "0C -> K");
    check_convert(1, "kwh", "j", 3.6e6, "kWh -> J");
    check_convert(1, "hp", "w", 745.69987158227022, "hp -> W");
    check_convert(1, "atm", "psi", 14.695948775513449, "atm -> psi", 1e-9);
    check_convert(1, "lbf", "n", 4.4482216152605, "lbf -> N");

    double out = 0;
    const char* err = nullptr;
    check(!math::unitexpr::convert_value(1, "mi", "kg", &out, &err) &&
              std::strcmp(err, "Units don't match") == 0,
          "cross-family errors");
    err = nullptr;
    check(!math::unitexpr::convert_value(1, "bogus", "km", &out, &err) &&
              std::strcmp(err, "Unknown unit") == 0,
          "unknown unit errors");
}

void test_substitute() {
    check(!math::unitexpr::contains_convert("2+2"), "no convert call");
    check(math::unitexpr::contains_convert("convert(1,\"mi\",\"km\")"), "detects convert");
    check(!math::unitexpr::contains_convert("myconvert(1)"), "ident-prefix miss");

    check_sub("convert(1,\"mi\",\"km\")", "1.609344", "quoted units");
    check_sub("convert(1, mi, km)", "1.609344", "bare units");
    check_sub("convert(1, MI, KM)", "1.609344", "case-insensitive");
    check_sub("convert(2*3, hr, min)", "360", "value is an expression");
    check_sub("1+convert(1,\"m\",\"cm\")", "1+100", "composes inline");

    char buf[256];
    std::snprintf(buf, sizeof(buf), "convert(1,\"mi\")");
    const char* err = nullptr;
    check(!math::unitexpr::substitute(buf, sizeof(buf), &err) &&
              std::strcmp(err, "convert needs (value, from, to)") == 0,
          "two-arg errors");
}

void test_constants() {
    // Engine-bound identifiers (4D.17).
    auto r = math::engine().evaluate("clight");
    check(r.ok, "clight evaluates");
    check_near(r.value, 299792458.0, "speed of light");
    r = math::engine().evaluate("navo/1e23");
    check(r.ok && std::fabs(r.value - 6.02214076) < 1e-8, "avogadro in expression");
    r = math::engine().evaluate("rgas");
    check_near(r.value, 8.314462618, "gas constant");
    // Catalog exposes the same table.
    int count = 0;
    const auto* cs = math::constants(&count);
    check(count >= 16, "constants table size");
    check(std::strcmp(cs[0].name, "clight") == 0, "first constant name");
}

}  // namespace

int main() {
    test_convert_value();
    test_substitute();
    test_constants();

    std::printf("test_units: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
