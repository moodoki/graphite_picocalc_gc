#pragma once

#include <cstddef>

// Typed unit conversions (4D.18, D38: `convert()` only — no picker
// screen in v1). Unit names are strings, which can't ride tinyexpr, so
// — mirroring solveexpr — each convert(...) call is replaced by a
// numeric literal before the expression continues down the normal
// evaluation pipeline (stores and composition work for free):
//
//   convert(1, "mi", "km")     -> 1.609344
//   convert(100, c, f)         -> 212  (quotes optional)
//   2*convert(1,"hp","w")      composes like any literal
//
// Families: length, mass, time, speed, area, volume, temperature,
// energy, power, pressure, force. Cross-family conversion errors.
namespace math::unitexpr {

bool contains_convert(const char* s);

// Replace every convert(...) call in buf with its numeric result.
// Returns false and sets *err (static string) on failure. A buf
// without convert() calls is left untouched (returns true).
bool substitute(char* buf, size_t cap, const char** err);

// Direct conversion (the picker-less core; also host-testable).
bool convert_value(double value, const char* from, const char* to, double* out, const char** err);

}  // namespace math::unitexpr
