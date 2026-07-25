#include "math/units.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/engine.hpp"

namespace math::unitexpr {

namespace {

constexpr size_t kMaxLen = 256;

enum class Family : unsigned char {
    kLength,
    kMass,
    kTime,
    kSpeed,
    kArea,
    kVolume,
    kTemp,
    kEnergy,
    kPower,
    kPressure,
    kForce,
};

// base = value * factor + offset (offset only for temperature).
// Base units: m, kg, s, m/s, m2, m3, K, J, W, Pa, N.
struct Unit {
    const char* name;
    Family family;
    double factor;
    double offset;
};

const Unit kUnits[] = {
    // Length (base m)
    {"mm", Family::kLength, 1e-3, 0},
    {"cm", Family::kLength, 1e-2, 0},
    {"m", Family::kLength, 1.0, 0},
    {"km", Family::kLength, 1e3, 0},
    {"in", Family::kLength, 0.0254, 0},
    {"ft", Family::kLength, 0.3048, 0},
    {"yd", Family::kLength, 0.9144, 0},
    {"mi", Family::kLength, 1609.344, 0},
    {"nmi", Family::kLength, 1852.0, 0},
    // Mass (base kg)
    {"mg", Family::kMass, 1e-6, 0},
    {"g", Family::kMass, 1e-3, 0},
    {"kg", Family::kMass, 1.0, 0},
    {"tonne", Family::kMass, 1000.0, 0},
    {"oz", Family::kMass, 0.028349523125, 0},
    {"lb", Family::kMass, 0.45359237, 0},
    // Time (base s)
    {"ms", Family::kTime, 1e-3, 0},
    {"s", Family::kTime, 1.0, 0},
    {"min", Family::kTime, 60.0, 0},
    {"hr", Family::kTime, 3600.0, 0},
    {"day", Family::kTime, 86400.0, 0},
    {"week", Family::kTime, 604800.0, 0},
    {"yr", Family::kTime, 31557600.0, 0},  // Julian year
    // Speed (base m/s)
    {"m/s", Family::kSpeed, 1.0, 0},
    {"km/h", Family::kSpeed, 1.0 / 3.6, 0},
    {"mph", Family::kSpeed, 0.44704, 0},
    {"knot", Family::kSpeed, 1852.0 / 3600.0, 0},
    {"ft/s", Family::kSpeed, 0.3048, 0},
    // Area (base m2)
    {"cm2", Family::kArea, 1e-4, 0},
    {"m2", Family::kArea, 1.0, 0},
    {"km2", Family::kArea, 1e6, 0},
    {"ft2", Family::kArea, 0.09290304, 0},
    {"acre", Family::kArea, 4046.8564224, 0},
    {"ha", Family::kArea, 1e4, 0},
    // Volume (base m3)
    {"ml", Family::kVolume, 1e-6, 0},
    {"l", Family::kVolume, 1e-3, 0},
    {"m3", Family::kVolume, 1.0, 0},
    {"tsp", Family::kVolume, 4.92892159375e-6, 0},
    {"tbsp", Family::kVolume, 1.478676478125e-5, 0},
    {"floz", Family::kVolume, 2.95735295625e-5, 0},
    {"cup", Family::kVolume, 2.365882365e-4, 0},
    {"pt", Family::kVolume, 4.73176473e-4, 0},
    {"qt", Family::kVolume, 9.46352946e-4, 0},
    {"gal", Family::kVolume, 3.785411784e-3, 0},
    // Temperature (base K): K = v*factor + offset
    {"c", Family::kTemp, 1.0, 273.15},
    {"f", Family::kTemp, 5.0 / 9.0, 459.67 * 5.0 / 9.0},
    {"k", Family::kTemp, 1.0, 0},
    // Energy (base J)
    {"j", Family::kEnergy, 1.0, 0},
    {"kj", Family::kEnergy, 1e3, 0},
    {"cal", Family::kEnergy, 4.184, 0},
    {"kcal", Family::kEnergy, 4184.0, 0},
    {"btu", Family::kEnergy, 1055.05585262, 0},
    {"kwh", Family::kEnergy, 3.6e6, 0},
    {"ev", Family::kEnergy, 1.602176634e-19, 0},
    // Power (base W)
    {"w", Family::kPower, 1.0, 0},
    {"kw", Family::kPower, 1e3, 0},
    {"hp", Family::kPower, 745.69987158227022, 0},
    // Pressure (base Pa)
    {"pa", Family::kPressure, 1.0, 0},
    {"kpa", Family::kPressure, 1e3, 0},
    {"bar", Family::kPressure, 1e5, 0},
    {"atm", Family::kPressure, 101325.0, 0},
    {"mmhg", Family::kPressure, 133.322387415, 0},
    {"psi", Family::kPressure, 6894.757293168, 0},
    // Force (base N)
    {"n", Family::kForce, 1.0, 0},
    {"kn", Family::kForce, 1e3, 0},
    {"lbf", Family::kForce, 4.4482216152605, 0},
};

bool ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Trim spaces and surrounding quotes, lowercase into out.
void clean_unit(const char* s, size_t n, char* out, size_t cap) {
    while (n > 0 && (*s == ' ' || *s == '"')) {
        ++s;
        --n;
    }
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '"')) {
        --n;
    }
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < cap; ++i) {
        out[o++] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    }
    out[o] = 0;
}

const Unit* find_unit(const char* name) {
    for (const Unit& u : kUnits) {
        if (std::strcmp(u.name, name) == 0) {
            return &u;
        }
    }
    return nullptr;
}

// One convert(inner) call -> numeric literal in num_out.
bool eval_convert_call(const char* inner, size_t inner_len, char* num_out, size_t num_cap,
                       const char** err) {
    // Split on top-level commas.
    const char* starts[3];
    size_t lens[3];
    int count = 0;
    int depth = 0;
    size_t arg_start = 0;
    for (size_t i = 0; i <= inner_len; ++i) {
        const char c = i < inner_len ? inner[i] : ',';
        if (c == '(' || c == '{') {
            ++depth;
        } else if (c == ')' || c == '}') {
            --depth;
        } else if (c == ',' && depth == 0) {
            if (count == 3) {
                *err = "convert needs (value, from, to)";
                return false;
            }
            starts[count] = inner + arg_start;
            lens[count] = i - arg_start;
            ++count;
            arg_start = i + 1;
        }
    }
    if (count != 3) {
        *err = "convert needs (value, from, to)";
        return false;
    }

    char vexpr[kMaxLen];
    if (lens[0] >= sizeof(vexpr)) {
        *err = "Expression too long";
        return false;
    }
    std::memcpy(vexpr, starts[0], lens[0]);
    vexpr[lens[0]] = 0;
    double value = 0;
    if (!eval_field(vexpr, &value)) {
        *err = "Bad convert value";
        return false;
    }

    char from[16];
    char to[16];
    clean_unit(starts[1], lens[1], from, sizeof(from));
    clean_unit(starts[2], lens[2], to, sizeof(to));
    double out = 0;
    if (!convert_value(value, from, to, &out, err)) {
        return false;
    }
    std::snprintf(num_out, num_cap, "%.17g", out);
    return true;
}

}  // namespace

bool convert_value(double value, const char* from, const char* to, double* out, const char** err) {
    const Unit* uf = find_unit(from);
    const Unit* ut = find_unit(to);
    if (uf == nullptr || ut == nullptr) {
        *err = "Unknown unit";
        return false;
    }
    if (uf->family != ut->family) {
        *err = "Units don't match";
        return false;
    }
    const double base = value * uf->factor + uf->offset;
    *out = (base - ut->offset) / ut->factor;
    return true;
}

bool contains_convert(const char* s) {
    for (const char* p = s; *p != 0; ++p) {
        if (p > s && ident_char(p[-1])) {
            continue;
        }
        if (std::strncmp(p, "convert", 7) == 0 && p[7] == '(') {
            return true;
        }
    }
    return false;
}

bool substitute(char* buf, size_t cap, const char** err) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (char* p = buf; (p = std::strstr(p, "convert")) != nullptr; ++p) {
            if (p > buf && ident_char(p[-1])) {
                continue;
            }
            char* q = p + 7;
            if (*q != '(') {
                continue;
            }
            const char* close = nullptr;
            int depth = 0;
            for (const char* r = q; *r != 0; ++r) {
                if (*r == '(' || *r == '{') {
                    ++depth;
                } else if (*r == ')' || *r == '}') {
                    --depth;
                    if (depth == 0) {
                        close = r;
                        break;
                    }
                }
            }
            if (close == nullptr) {
                *err = "Syntax error";
                return false;
            }
            char num[32];
            if (!eval_convert_call(q + 1, static_cast<size_t>(close - q - 1), num, sizeof(num),
                                   err)) {
                return false;
            }
            // Splice: [buf, p) + num + (close, ...]
            char rebuilt[kMaxLen];
            const int wrote = std::snprintf(rebuilt, sizeof(rebuilt), "%.*s%s%s",
                                            static_cast<int>(p - buf), buf, num, close + 1);
            if (wrote < 0 || wrote >= static_cast<int>(sizeof(rebuilt)) ||
                static_cast<size_t>(wrote) >= cap) {
                *err = "Expression too long";
                return false;
            }
            std::memcpy(buf, rebuilt, static_cast<size_t>(wrote) + 1);
            changed = true;
            break;
        }
    }
    return true;
}

}  // namespace math::unitexpr
