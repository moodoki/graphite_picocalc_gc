#pragma once

#include "math/array.hpp"

// List operations (task 3A.5, spec §3.2). All streaming/chunked so a
// 10000-element PSRAM-tier list never needs an 80 KB SRAM staging
// buffer. Host-testable (no platform dependencies beyond the
// psram_backend seam).
namespace math::listops {

// Scalar reductions. Empty lists follow math convention: sum {} = 0,
// prod {} = 1 (the identity elements). Real-only: NaN on a kComplex
// array (callers check dtype and route to csum, 4D.24).
calc_t sum(const Array& a);
calc_t prod(const Array& a);

// Componentwise complex sum (4D.24); promotes a kDouble array's
// elements to {v, 0}.
Complex csum(const Array& a);

// In-place sorts. NaNs order last (ascending) via a total-order
// comparator. SRAM-tier lists sort directly; PSRAM-tier lists run an
// external merge sort through one temp region — returns false if no
// temp region is available.
bool sort_asc(Array& a);
bool sort_desc(Array& a);

// out := cumulative sums of a / consecutive differences of a
// (delta_list length is size-1). out must be a distinct Array.
bool cumsum(const Array& a, Array& out);
bool delta_list(const Array& a, Array& out);

// out := expr evaluated at var = lo, lo+step, ..., hi (inclusive,
// with a half-step tolerance). var_slot indexes math::Variables (its
// value is saved and restored). On failure returns false and sets
// *err to a static message.
bool seq(const char* expr, int var_slot, calc_t lo, calc_t hi, calc_t step, Array& out,
         const char** err);

// dst := src (resize + chunked copy). dst adopts src's dtype (4D.24).
bool copy(const Array& src, Array& dst);

// dst := src as a kComplex array (reals promoted to {v, 0}).
bool copy_complex(const Array& src, Array& dst);

// Migrate `a` to the complex tier in place (reals promoted to {v, 0});
// no-op when already complex. Used when a complex element is stored
// into a real list. Stages through an internal scratch array — returns
// false (a unchanged) when the PSRAM tier is unavailable.
bool make_complex(Array& a);

}  // namespace math::listops
