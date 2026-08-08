#pragma once

#include "math/cas/expr.hpp"

// CAS expression parser (Phase 5, phase5-spec.md §4). A recursive-descent
// parser over the same infix grammar the numeric engine accepts, but it
// produces a manipulable Expr tree instead of tinyexpr's opaque eval tree.
// Structurally mirrors math::complexexpr / math::matexpr.
//
// Precedence (low -> high): '=' < '+'/'-' < '*'/'/' < unary '-' < '^' <
// function application. Right-associative '^'. CAS-mode implicit
// multiplication is supported (2x, xy, 2(x+1), (x+1)(x-1)) — spec §4, P5-3:
// enabled here only, not in the global numeric parser.
//
// Encodings (spec §2): subtraction a-b -> ADD(a, NEG(b)); division a/b ->
// MUL(a, POW(b, -1)). The reserved imaginary unit 'i' is a VAR. Named
// constants (pi) are nullary FUNC nodes. Multi-letter runs that are not a
// known function/constant split into single-char variables (xy -> x*y).
namespace math::cas {

// Parse `input` into an Expr tree allocated from g_cas_pool. Returns nullptr
// on error; when `error` is non-null it receives a static message. Does NOT
// reset the pool — the caller owns pool lifetime (reset before a top-level
// operation).
Expr* parse_expr(const char* input, const char** error = nullptr);

}  // namespace math::cas
