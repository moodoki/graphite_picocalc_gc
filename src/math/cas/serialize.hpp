#pragma once

#include <cstddef>

#include "math/cas/expr.hpp"

// Serialize an Expr tree back to a human-readable infix string (Phase 5,
// phase5-spec.md §4, task 4D.3). Output re-parses (via parse_expr) to a
// structurally equal tree. Subtraction (ADD with a NEG child) and division
// (MUL with a POW of negative exponent) are rendered with '-' and '/' rather
// than their internal ADD/NEG and MUL/POW encodings.
//
// (expr_to_layout — the 2D natural-math render — is a separate Stage 3
// function; this is the flat-string form used for history text and tests.)
namespace math::cas {

// Writes a NUL-terminated string into buf; returns the number of characters
// written (excluding the NUL), or 0 for a null expr. Output is truncated to
// fit buf_len.
std::size_t expr_to_string(const Expr* expr, char* buf, std::size_t buf_len);

}  // namespace math::cas
