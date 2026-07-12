#include "math/catalog.hpp"

#include "math/functions.hpp"

namespace math {

namespace {

const void* fp1(double (*f)(double)) {
    return reinterpret_cast<const void*>(f);
}
const void* fp2(double (*f)(double, double)) {
    return reinterpret_cast<const void*>(f);
}
const void* fp0(double (*f)()) {
    return reinterpret_cast<const void*>(f);
}

// Display order: trig, logs, combinatorics, misc. Summaries must fit
// the help screen's ~26-char column.
const FnDescriptor kCatalog[] = {
    {"sin", "sin(x)", "Sine (angle mode)", fp1(fn::sin_am), 1},
    {"cos", "cos(x)", "Cosine (angle mode)", fp1(fn::cos_am), 1},
    {"tan", "tan(x)", "Tangent (angle mode)", fp1(fn::tan_am), 1},
    {"asin", "asin(x)", "Inverse sine", fp1(fn::asin_am), 1},
    {"acos", "acos(x)", "Inverse cosine", fp1(fn::acos_am), 1},
    {"atan", "atan(x)", "Inverse tangent", fp1(fn::atan_am), 1},
    {"log", "log(x)", "Log base 10", fp1(fn::log10_ti), 1},
    {"ln", "ln(x)", "Natural log", fp1(fn::ln_nat), 1},
    {"fac", "fac(n)", "Factorial (also n!)", fp1(fn::factorial), 1},
    {"ncr", "ncr(n, r)", "n choose r", fp2(fn::ncr), 2},
    {"npr", "npr(n, r)", "Permutations", fp2(fn::npr), 2},
    {"rand", "rand()", "Random in [0, 1)", fp0(fn::rand01), 0},
    {"round", "round(x, n)", "Round to n decimals", fp2(fn::round_n), 2},
    {"min", "min(a, b)", "Smaller of a and b", fp2(fn::min2), 2},
    {"max", "max(a, b)", "Larger of a and b", fp2(fn::max2), 2},
    {"deg", "deg(x)", "Radians to degrees", fp1(fn::deg), 1},
    {"rad", "rad(x)", "Degrees to radians", fp1(fn::rad), 1},
};

constexpr int kCount = sizeof(kCatalog) / sizeof(kCatalog[0]);
static_assert(kCount <= kMaxCatalogEntries, "grow kMaxCatalogEntries");

}  // namespace

const FnDescriptor* catalog(int* count) {
    *count = kCount;
    return kCatalog;
}

}  // namespace math
