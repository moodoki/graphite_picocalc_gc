/* Support shim for the vendored cephes sources (drivers/cephes/ is
 * read-only third-party code, so the fix lives here — AGENTS.md
 * driver-workaround rule): gamma.c declares `extern int
 * isfinite(double)` and calls it as a real function, but both newlib
 * (firmware) and macOS libm (host tests) provide only the <math.h>
 * macro, not a linkable symbol. Compiled into the `cephes` CMake
 * target and into scripts/host-tests.sh alongside the cephes objects.
 * No <math.h> include — the macro would collide with the definition. */
// NOLINTNEXTLINE(misc-use-internal-linkage) — must resolve cephes' extern reference
int isfinite(double x) {
    /* Finite iff the exponent field is not all-ones (Inf/NaN). */
    union {
        double d;
        unsigned long long u;
    } v;
    v.d = x;
    return (v.u & 0x7ff0000000000000ULL) != 0x7ff0000000000000ULL;
}
