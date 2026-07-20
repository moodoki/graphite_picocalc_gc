#pragma once

#include "math/array.hpp"
#include "math/complex.hpp"

namespace platform {
class Storage;
}

// Matrix operations (Phase 4A, phase4-spec §3). Matrices are 2-D
// Arrays; like listops, everything here is a free function streaming
// rows through fixed SRAM buffers, because Array is get()/set()-based
// (PSRAM is not memory-mapped) and non-copyable — the spec's
// reference-returning Matrix class sketch doesn't fit, the same
// deviation lists made in Phase 3.
//
// All `out` parameters must be distinct Arrays from the inputs; the
// expression layer and editor guarantee this by computing into temps.
// On failure: returns false, `*err` gets a static message, and `out`
// is unspecified (callers display the error, never the value).
namespace math::matops {

// Editor-facing cap (spec: [A]-[J] up to 99x99). Expression results
// (augment, transpose of augment) may go up to kMaxRowElems along one
// axis; the Array::kMaxElements total cap always applies.
constexpr int kMaxDim = 99;
constexpr int kMaxRowElems = 200;  // Streaming row-buffer width
constexpr int kMaxEigen = 10;      // QR eigenvalue size limit
constexpr int kMaxPower = 100;     // [A]^n repeated-multiply cap

// dst := src (2-D resize + chunked copy).
bool copy(const Array& src, Array& dst);

bool add(const Array& a, const Array& b, Array& out, const char** err);
bool sub(const Array& a, const Array& b, Array& out, const char** err);
bool mul(const Array& a, const Array& b, Array& out, const char** err);
bool scalar_mul(const Array& a, calc_t k, Array& out, const char** err);
bool transpose(const Array& a, Array& out, const char** err);
bool identity(int n, Array& out, const char** err);
// Horizontal concatenation [a | b] (row counts must match).
bool augment(const Array& a, const Array& b, Array& out, const char** err);
// Resize preserving the row/col overlap (new cells zero) — a flat
// Array::resize would scramble rows when the column count changes.
// Accepts an empty `a` (creates a zero matrix). Editor DIM uses this.
bool reshape(const Array& a, int rows, int cols, Array& out, const char** err);

// LU with partial pivoting (direct expansion for 1x1/2x2/3x3). A
// singular matrix yields *out = 0, not an error.
bool determinant(const Array& a, calc_t* out, const char** err);
// Gauss-Jordan with partial pivoting.
bool inverse(const Array& a, Array& out, const char** err);
// (Reduced) row echelon form; non-square accepted.
bool rref(const Array& a, Array& out, const char** err);
bool ref(const Array& a, Array& out, const char** err);
bool rank(const Array& a, int* out, const char** err);
// p >= -1: -1 = inverse, 0 = identity, else repeated multiplication.
bool power(const Array& a, int p, Array& out, const char** err);

// Real eigenvalues via Hessenberg + shifted QR, n <= kMaxEigen. `out`
// is a 1-D list (descending), so results flow into l1..l6 and list
// expressions. A complex conjugate pair is an error (D28): lists are
// real-only, so a partial answer would mislead — use
// eigenvalues_complex for the full spectrum (Phase 4C, D30/P4-7).
bool eigenvalues(const Array& a, Array& out, const char** err);

// Full spectrum (real + complex-conjugate pairs) via the same
// Hessenberg + shifted QR core, n <= kMaxEigen. `out` must hold at
// least kMaxEigen entries; *count is the number written, descending by
// real part (a conjugate pair's +i entry precedes its -i entry).
bool eigenvalues_complex(const Array& a, Complex* out, int* count, const char** err);

}  // namespace math::matops

namespace math {

// The ten matrix variables [A]..[J] (spec §3.1). Pure state —
// persistence lives in matrices_persist.cpp (firmware-only TU, like
// lists_persist).
class MatrixStore {
public:
    static constexpr int kCount = 10;

    Array& matrix(int index) { return matrices_[index]; }
    const Array& matrix(int index) const { return matrices_[index]; }

    // Persistence to /picocalc/matrices.dat. load() is all-or-nothing
    // when the PSRAM tier is needed but not up yet (cold boot, D14) —
    // returns false so the late-init loop retries.
    bool save(platform::Storage& storage) const;
    bool load(platform::Storage& storage);

private:
    Array matrices_[kCount];
};

// Singleton accessor (project convention).
MatrixStore& matrices();

}  // namespace math
