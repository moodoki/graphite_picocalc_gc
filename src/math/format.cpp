#include "math/format.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace math {

namespace {

// Strip trailing zeros (and a trailing '.') from the fraction part of
// the number that ends at buf[len]. Works in place, returns new length.
int strip_zeros(char* buf) {
    char* dot = std::strchr(buf, '.');
    if (dot == nullptr) {
        return static_cast<int>(std::strlen(buf));
    }
    // Handle a possible exponent suffix.
    char* e = std::strchr(dot, 'e');
    char exp_part[16] = {};
    if (e != nullptr) {
        std::strncpy(exp_part, e, sizeof(exp_part) - 1);
        *e = 0;
    }
    char* end = buf + std::strlen(buf);
    while (end > dot + 1 && end[-1] == '0') {
        --end;
    }
    if (end == dot + 1) {
        --end;  // Also drop the '.'
    }
    *end = 0;
    if (exp_part[0] != 0) {
        std::strcat(buf, exp_part);
    }
    return static_cast<int>(std::strlen(buf));
}

DisplayMode g_mode = DisplayMode::kFloat;
int g_fix_digits = 2;

}  // namespace

DisplayMode display_mode() {
    return g_mode;
}
void set_display_mode(DisplayMode m) {
    g_mode = m;
}
int fix_digits() {
    return g_fix_digits;
}
void set_fix_digits(int n) {
    g_fix_digits = n < 0 ? 0 : (n > 9 ? 9 : n);
}

int format_number(calc_t x, char* buf, size_t buf_len) {
    if (buf_len == 0) {
        return 0;
    }
    if (std::isnan(x)) {
        return std::snprintf(buf, buf_len, "NaN");
    }
    if (std::isinf(x)) {
        return std::snprintf(buf, buf_len, x > 0 ? "Inf" : "-Inf");
    }

    if (g_mode == DisplayMode::kFix) {
        return std::snprintf(buf, buf_len, "%.*f", g_fix_digits, x);
    }
    if (g_mode == DisplayMode::kSci) {
        char tmp[40];
        std::snprintf(tmp, sizeof(tmp), "%.*e", g_fix_digits, x);
        char* e = std::strchr(tmp, 'e');
        int exponent = 0;
        if (e != nullptr) {
            exponent = std::atoi(e + 1);
            *e = 0;
        }
        return std::snprintf(buf, buf_len, "%se%d", tmp, exponent);
    }

    const double ax = std::fabs(x);

    // Integer display
    if (x == std::floor(x) && ax < 1e10) {
        return std::snprintf(buf, buf_len, "%.0f", x);
    }

    if (ax >= 1e10 || (ax > 0 && ax < 1e-4)) {
        // Scientific, 10 significant figures, then strip zeros; use a
        // compact exponent ("1.5e-7", not "1.50000e-07").
        char tmp[40];
        std::snprintf(tmp, sizeof(tmp), "%.9e", x);
        // Split mantissa / exponent
        char* e = std::strchr(tmp, 'e');
        int exponent = 0;
        if (e != nullptr) {
            exponent = std::atoi(e + 1);
            *e = 0;
        }
        strip_zeros(tmp);
        return std::snprintf(buf, buf_len, "%se%d", tmp, exponent);
    }

    std::snprintf(buf, buf_len, "%.10g", x);
    return strip_zeros(buf);
}

}  // namespace math
