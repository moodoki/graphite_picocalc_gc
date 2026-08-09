#include "math/array_format.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/complex.hpp"
#include "math/format.hpp"
#include "math/frac.hpp"
#include "math/types.hpp"

// Moved verbatim in 5.2.11 from list_expr.cpp and mat_expr.cpp. See the header
// for why display code survives the evaluators it lived in.
namespace math {

namespace {

using RealCellFmt = int (*)(calc_t, char*, size_t);
using CplxCellFmt = int (*)(const Complex&, NumberMode, char*, size_t);

// Fraction cell for >Frac (4D.2, extended to matrices): p/q when a tight
// fraction exists (den <= 10000), else the compact decimal fallback.
int cell_fraction(calc_t v, char* buf, size_t cap) {
    if (frac::format_fraction(v, 10000, buf, cap)) {
        return static_cast<int>(std::strlen(buf));
    }
    return format_number_compact(v, buf, cap);
}

// Shared body: renders "[[a,b][c,d]]" with per-cell formatters, so the
// plain and >Frac variants differ only in how real cells stringify.
// (Complex cells never convert to fractions — they keep the compact form.)
void format_matrix_impl(const Array& m, char* buf, size_t buf_len, RealCellFmt real_fmt,
                        CplxCellFmt cplx_fmt) {
    if (buf_len < 12) {
        if (buf_len > 0) {
            buf[0] = 0;
        }
        return;
    }
    size_t pos = 0;
    buf[pos++] = '[';
    const int rows = m.dim(0);
    const int cols = m.dim(1);
    bool truncated = false;
    const bool cplx = m.dtype() == Dtype::kComplex;

    // Near-zero cleanup: find the largest cell magnitude, then display any
    // cell more than ~12 orders smaller as a clean "0". This snaps the
    // floating-point roundoff that shows up in e.g. [A]^-1*[A] (off-diagonal
    // 2.22e-16) to an exact identity instead of scientific noise. Relative
    // to the matrix's own scale, so a genuinely tiny-magnitude matrix is
    // preserved (its own max sets the threshold). A cell exactly 0 already
    // prints "0"; this just extends that to sub-tolerance roundoff.
    calc_t maxmag = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const calc_t mag = cplx ? m.cget(r, c).modulus() : std::fabs(m.get(r, c));
            maxmag = mag > maxmag ? mag : maxmag;
        }
    }
    const calc_t zero_tol = maxmag * 1e-12;

    for (int r = 0; r < rows && !truncated; ++r) {
        buf[pos++] = '[';
        for (int c = 0; c < cols; ++c) {
            char num[48];
            const calc_t mag = cplx ? m.cget(r, c).modulus() : std::fabs(m.get(r, c));
            if (maxmag > 0 && mag <= zero_tol) {
                num[0] = '0';
                num[1] = 0;
            } else if (cplx) {
                cplx_fmt(m.cget(r, c), number_mode(), num, sizeof(num));
            } else {
                real_fmt(m.get(r, c), num, sizeof(num));
            }
            const size_t need = std::strlen(num) + (c > 0 ? 1 : 0);
            // Room for the number plus "...]]" + NUL in the worst case
            if (pos + need + 7 > buf_len) {
                buf[pos++] = kEllipsisGlyph;
                truncated = true;
                break;
            }
            if (c > 0) {
                buf[pos++] = ',';
            }
            std::memcpy(buf + pos, num, std::strlen(num));
            pos += std::strlen(num);
        }
        buf[pos++] = ']';
    }
    buf[pos++] = ']';
    buf[pos] = 0;
}

}  // namespace

void format_list(const Array& a, char* buf, size_t buf_len) {
    if (buf_len < 8) {
        if (buf_len > 0) {
            buf[0] = 0;
        }
        return;
    }
    size_t pos = 0;
    buf[pos++] = '{';
    const int n = a.size();
    const bool complex = a.dtype() == Dtype::kComplex;

    // Real elements are pulled in blocks through read_range rather than one at
    // a time through get() (D53). On a PSRAM-backed array — anything past
    // kSlabBytes, i.e. 256 doubles — a per-element get() intermittently
    // returned a wrong value on the Pico 1: `l1/499500` rendered element 1 as
    // 4.004007642e-6 instead of 4.004004004e-6 on ~8 runs in 30, which is
    // l1[1] read as 2.0000018 rather than 2. The bulk path is clean under the
    // same conditions (sum(l1)-499500 is exactly 0 on 25/25, a test that would
    // show an error that size), and it is what every compute path already
    // uses, so this mirrors it rather than inventing a third access pattern.
    //
    // The buffer is static, like the compute path's, and deliberately not a
    // local: destination-in-bss vs destination-on-the-stack is one of the two
    // differences between the clean path and the broken one, and the root
    // cause is NOT yet established. See D53 — this removes the defect from the
    // one path that made it visible; it does not close the underlying bug, and
    // ~20 other get() call sites still carry it.
    constexpr int kBlock = 32;  // Far more than the buffer can ever print
    static calc_t g_block[kBlock];
    int block_first = -1;

    for (int i = 0; i < n; ++i) {
        char num[48];
        // Compact per-element formatting so more values fit before the
        // ",..." cutoff (testdrive 2026-07-20); the home screen lets you
        // pan the full list with LEFT/RIGHT.
        if (complex) {
            format_complex(a.cget(i), number_mode(), num, sizeof(num));
        } else {
            if (block_first < 0 || i >= block_first + kBlock) {
                block_first = i;
                const int count = n - i < kBlock ? n - i : kBlock;
                a.read_range(block_first, count, g_block);
            }
            format_number_compact(g_block[i - block_first], num, sizeof(num));
        }
        const size_t need = std::strlen(num) + (i > 0 ? 1 : 0);
        if (pos + need + 6 > buf_len) {  // Room for ",<ellipsis>}" + NUL
            buf[pos++] = ',';
            buf[pos++] = kEllipsisGlyph;
            break;
        }
        if (i > 0) {
            buf[pos++] = ',';
        }
        std::memcpy(buf + pos, num, std::strlen(num));
        pos += std::strlen(num);
    }
    buf[pos++] = '}';
    buf[pos] = 0;
}

void format_matrix(const Array& m, char* buf, size_t buf_len) {
    format_matrix_impl(m, buf, buf_len, format_number_compact, format_complex_compact);
}

void format_matrix_frac(const Array& m, char* buf, size_t buf_len) {
    format_matrix_impl(m, buf, buf_len, cell_fraction, format_complex_compact);
}

}  // namespace math
