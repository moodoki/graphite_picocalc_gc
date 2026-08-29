// Host-side tests for the Phase 4A matrix stack: matops over 2-D
// Arrays and MatrixStore. The PSRAM tier runs against the malloc shim
// in host_psram_backend.cpp.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/array.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/lists.hpp"
#include "eval_shim.hpp"
#include "math/array_format.hpp"
#include "math/matrix.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

void check_near(double got, double expected, const char* what, double tol = 1e-9) {
    ++g_checks;
    if (std::isnan(got) || std::fabs(got - expected) > tol) {
        std::printf("FAIL: %s -> %.12g (expected %.12g)\n", what, got, expected);
        ++g_failures;
    }
}

// NB: call the operation on its OWN line and pass the result in, never
// check_err(matops::foo(..., &err), err, ...). The call writes `err` and
// the next argument reads it, and argument evaluation order is
// UNSPECIFIED in C++ -- so that spelling asks the compiler which it feels
// like doing. It passed under clang and failed all 20 of these checks
// under GCC, and went unnoticed for as long as it did because CI never ran
// this suite (fixed in Phase 6.4.6).
void check_err(bool ok, const char* err, const char* expected, const char* what) {
    ++g_checks;
    if (ok || err == nullptr || std::strcmp(err, expected) != 0) {
        std::printf("FAIL: %s -> ok=%d err='%s' (expected '%s')\n", what, ok ? 1 : 0,
                    err != nullptr ? err : "-", expected);
        ++g_failures;
    }
}

// Fill a rows x cols matrix from a flat row-major initializer (also
// reverts a complex-retyped Array back to the real tier).
bool fill(math::Array& m, int rows, int cols, const double* vals) {
    m.clear();
    if (!m.set_dtype(math::Dtype::kDouble) || !m.resize(rows, cols)) {
        return false;
    }
    m.write_range(0, rows * cols, vals);
    return true;
}

// Complex fill (4D.25).
bool cfill(math::Array& m, int rows, int cols, const math::Complex* vals) {
    m.clear();
    if (!m.set_dtype(math::Dtype::kComplex) || !m.resize(rows, cols)) {
        return false;
    }
    m.write_range_c(0, rows * cols, vals);
    return true;
}

void check_cnear(const math::Complex& got, double re, double im, const char* what,
                 double tol = 1e-9) {
    ++g_checks;
    if (std::isnan(got.re) || std::isnan(got.im) || std::fabs(got.re - re) > tol ||
        std::fabs(got.im - im) > tol) {
        std::printf("FAIL: %s -> (%.12g,%.12g) (expected (%.12g,%.12g))\n", what, got.re, got.im,
                    re, im);
        ++g_failures;
    }
}

void check_matrix(const math::Array& m, int rows, int cols, const double* expected,
                  const char* what, double tol = 1e-9) {
    ++g_checks;
    if (m.dim(0) != rows || m.dim(1) != cols) {
        std::printf("FAIL: %s -> %dx%d (expected %dx%d)\n", what, m.dim(0), m.dim(1), rows, cols);
        ++g_failures;
        return;
    }
    for (int i = 0; i < rows * cols; ++i) {
        const double got = m.get(i);
        if (std::isnan(got) || std::fabs(got - expected[i]) > tol) {
            std::printf("FAIL: %s [%d] -> %.12g (expected %.12g)\n", what, i, got, expected[i]);
            ++g_failures;
            return;
        }
    }
}

void test_arithmetic() {
    using namespace math;
    Array a;
    Array b;
    Array out;
    const char* err = nullptr;
    bool ok = false;

    const double va[6] = {1, 2, 3, 4, 5, 6};
    const double vb[6] = {10, 20, 30, 40, 50, 60};
    check(fill(a, 2, 3, va) && fill(b, 2, 3, vb), "fill 2x3");

    check(matops::add(a, b, out, &err), "add ok");
    const double vadd[6] = {11, 22, 33, 44, 55, 66};
    check_matrix(out, 2, 3, vadd, "add values");

    check(matops::sub(b, a, out, &err), "sub ok");
    const double vsub[6] = {9, 18, 27, 36, 45, 54};
    check_matrix(out, 2, 3, vsub, "sub values");

    check(matops::scalar_mul(a, 3, out, &err), "scalar_mul ok");
    const double vsm[6] = {3, 6, 9, 12, 15, 18};
    check_matrix(out, 2, 3, vsm, "scalar_mul values");

    check(matops::transpose(a, out, &err), "transpose ok");
    const double vt[6] = {1, 4, 2, 5, 3, 6};
    check_matrix(out, 3, 2, vt, "transpose values");

    // (2x3)*(3x2) and the known TI-style example
    Array c;
    const double vc[6] = {7, 8, 9, 10, 11, 12};
    check(fill(c, 3, 2, vc), "fill 3x2");
    check(matops::mul(a, c, out, &err), "mul ok");
    const double vmul[4] = {58, 64, 139, 154};
    check_matrix(out, 2, 2, vmul, "mul values");

    // Dim mismatches
    err = nullptr;
    ok = matops::add(a, c, out, &err);
    check_err(ok, err, "Dim mismatch", "add dim mismatch");
    err = nullptr;
    ok = matops::mul(a, b, out, &err);
    check_err(ok, err, "Dim mismatch", "mul dim mismatch");

    // Empty operand
    Array e;
    err = nullptr;
    ok = matops::add(e, b, out, &err);
    check_err(ok, err, "Not a matrix", "add empty");
}

void test_identity_augment() {
    using namespace math;
    Array a;
    Array b;
    Array out;
    const char* err = nullptr;
    bool ok = false;

    check(matops::identity(3, out, &err), "identity ok");
    const double vi[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    check_matrix(out, 3, 3, vi, "identity values");

    const double va[4] = {1, 2, 3, 4};
    const double vb[2] = {9, 8};
    check(fill(a, 2, 2, va) && fill(b, 2, 1, vb), "fill augment inputs");
    check(matops::augment(a, b, out, &err), "augment ok");
    const double vaug[6] = {1, 2, 9, 3, 4, 8};
    check_matrix(out, 2, 3, vaug, "augment values");

    Array c;
    const double vc[3] = {1, 2, 3};
    check(fill(c, 3, 1, vc), "fill 3x1");
    err = nullptr;
    ok = matops::augment(a, c, out, &err);
    check_err(ok, err, "Dim mismatch", "augment row mismatch");
}

void test_determinant() {
    using namespace math;
    Array a;
    Array out;
    const char* err = nullptr;
    bool ok = false;
    calc_t det = 0;

    check(matops::identity(5, out, &err) && matops::determinant(out, &det, &err),
          "det(identity(5)) ok");
    check_near(det, 1.0, "det(identity(5))");

    const double v2[4] = {3, 8, 4, 6};
    check(fill(a, 2, 2, v2), "fill 2x2");
    check(matops::determinant(a, &det, &err), "det 2x2 ok");
    check_near(det, -14.0, "det 2x2");

    const double v3[9] = {6, 1, 1, 4, -2, 5, 2, 8, 7};
    check(fill(a, 3, 3, v3), "fill 3x3");
    check(matops::determinant(a, &det, &err), "det 3x3 ok");
    check_near(det, -306.0, "det 3x3");

    // 4x4 exercises the LU path (det = 30: block upper-triangular).
    const double v4[16] = {2, 1, 0, 0, 1, 2, 0, 0, 5, 6, 3, 1, 7, 8, 2, 4};
    check(fill(a, 4, 4, v4), "fill 4x4");
    check(matops::determinant(a, &det, &err), "det 4x4 ok");
    check_near(det, 30.0, "det 4x4 (LU)");

    // Singular: rank-deficient rows
    const double vs[16] = {1, 2, 3, 4, 2, 4, 6, 8, 1, 0, 1, 0, 0, 1, 0, 1};
    check(fill(a, 4, 4, vs), "fill singular 4x4");
    check(matops::determinant(a, &det, &err), "det singular ok");
    check_near(det, 0.0, "det singular = 0");

    // Non-square
    const double vr[6] = {1, 2, 3, 4, 5, 6};
    check(fill(a, 2, 3, vr), "fill 2x3");
    err = nullptr;
    ok = matops::determinant(a, &det, &err);
    check_err(ok, err, "Not square", "det non-square");
}

void test_inverse() {
    using namespace math;
    Array a;
    Array inv;
    Array prod;
    const char* err = nullptr;
    bool ok = false;

    const double v3[9] = {2, -1, 0, -1, 2, -1, 0, -1, 2};
    check(fill(a, 3, 3, v3), "fill spd 3x3");
    check(matops::inverse(a, inv, &err), "inverse ok");
    check(matops::mul(a, inv, prod, &err), "a*inv ok");
    const double vi[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    check_matrix(prod, 3, 3, vi, "a*inverse(a) = I", 1e-9);

    // [A]^-1 via power(-1) matches
    Array pinv;
    check(matops::power(a, -1, pinv, &err), "power -1 ok");
    check(matops::mul(a, pinv, prod, &err), "a*pow-1 ok");
    check_matrix(prod, 3, 3, vi, "a*(a^-1) = I", 1e-9);

    const double vs[4] = {1, 2, 2, 4};
    check(fill(a, 2, 2, vs), "fill singular 2x2");
    err = nullptr;
    ok = matops::inverse(a, inv, &err);
    check_err(ok, err, "Singular matrix", "inverse singular");
}

void test_rref_rank() {
    using namespace math;
    Array a;
    Array out;
    const char* err = nullptr;
    int rk = 0;

    // Augmented system [A|b]: x=2, y=3, z=-1
    const double v[12] = {1, 1, 1, 4, 2, -1, 3, -2, 3, 1, -2, 11};
    check(fill(a, 3, 4, v), "fill 3x4 system");
    check(matops::rref(a, out, &err), "rref ok");
    const double vr[12] = {1, 0, 0, 2, 0, 1, 0, 3, 0, 0, 1, -1};
    check_matrix(out, 3, 4, vr, "rref solves system", 1e-9);

    check(matops::rank(a, &rk, &err), "rank ok");
    check(rk == 3, "rank full");

    const double vd[9] = {1, 2, 3, 2, 4, 6, 3, 6, 9};
    check(fill(a, 3, 3, vd), "fill rank-1");
    check(matops::rank(a, &rk, &err), "rank-1 ok");
    check(rk == 1, "rank deficient");

    check(matops::ref(a, out, &err), "ref ok");
    check_near(out.get(1, 0), 0.0, "ref lower zero (1,0)");
    check_near(out.get(2, 0), 0.0, "ref lower zero (2,0)");
    check_near(out.get(2, 1), 0.0, "ref lower zero (2,1)");
}

void test_reshape() {
    using namespace math;
    Array a;
    Array out;
    const char* err = nullptr;
    bool ok = false;

    const double v[6] = {1, 2, 3, 4, 5, 6};
    check(fill(a, 2, 3, v), "reshape fill 2x3");

    // Grow: overlap preserved by row/col position, new cells zero.
    check(matops::reshape(a, 3, 4, out, &err), "reshape grow ok");
    const double vg[12] = {1, 2, 3, 0, 4, 5, 6, 0, 0, 0, 0, 0};
    check_matrix(out, 3, 4, vg, "reshape grow values");

    // Shrink: truncate rows and cols.
    check(matops::reshape(a, 1, 2, out, &err), "reshape shrink ok");
    const double vs[2] = {1, 2};
    check_matrix(out, 1, 2, vs, "reshape shrink values");

    // From empty: fresh zero matrix.
    Array e;
    check(matops::reshape(e, 2, 2, out, &err), "reshape from empty ok");
    const double vz[4] = {0, 0, 0, 0};
    check_matrix(out, 2, 2, vz, "reshape from empty zeros");

    err = nullptr;
    ok = matops::reshape(a, 0, 3, out, &err);
    check_err(ok, err, "Dim mismatch", "reshape zero rows");
    err = nullptr;
    ok = matops::reshape(a, 100, 3, out, &err);
    check_err(ok, err, "Dim mismatch", "reshape over cap");
}

void test_power() {
    using namespace math;
    Array a;
    Array out;
    const char* err = nullptr;
    bool ok = false;

    const double v[4] = {1, 1, 0, 1};
    check(fill(a, 2, 2, v), "fill shear");
    check(matops::power(a, 0, out, &err), "power 0 ok");
    const double vi[4] = {1, 0, 0, 1};
    check_matrix(out, 2, 2, vi, "a^0 = I");

    check(matops::power(a, 5, out, &err), "power 5 ok");
    const double v5[4] = {1, 5, 0, 1};
    check_matrix(out, 2, 2, v5, "shear^5");

    err = nullptr;
    ok = matops::power(a, 101, out, &err);
    check_err(ok, err, "Exponent out of range", "power cap");

    Array r;
    const double vr[6] = {1, 2, 3, 4, 5, 6};
    check(fill(r, 2, 3, vr), "fill 2x3");
    err = nullptr;
    ok = matops::power(r, 2, out, &err);
    check_err(ok, err, "Not square", "power non-square");
}

void test_eigenvalues() {
    using namespace math;
    Array a;
    Array out;
    const char* err = nullptr;
    bool ok = false;

    // Diagonal: eigenvalues are the diagonal, sorted descending.
    const double vd[9] = {3, 0, 0, 0, 7, 0, 0, 0, -2};
    check(fill(a, 3, 3, vd), "fill diagonal");
    check(matops::eigenvalues(a, out, &err), "eigen diagonal ok");
    check(out.size() == 3, "eigen diagonal count");
    check_near(out.get(0), 7.0, "eigen diag [0]", 1e-8);
    check_near(out.get(1), 3.0, "eigen diag [1]", 1e-8);
    check_near(out.get(2), -2.0, "eigen diag [2]", 1e-8);

    // Symmetric tridiagonal: 2 + sqrt(2), 2, 2 - sqrt(2)
    const double vs[9] = {2, -1, 0, -1, 2, -1, 0, -1, 2};
    check(fill(a, 3, 3, vs), "fill sym tridiag");
    check(matops::eigenvalues(a, out, &err), "eigen sym ok");
    check_near(out.get(0), 2.0 + std::sqrt(2.0), "eigen sym [0]", 1e-8);
    check_near(out.get(1), 2.0, "eigen sym [1]", 1e-8);
    check_near(out.get(2), 2.0 - std::sqrt(2.0), "eigen sym [2]", 1e-8);

    // Non-symmetric, real spectrum: eigen(4,1;2,3) = 5, 2
    const double vn[4] = {4, 1, 2, 3};
    check(fill(a, 2, 2, vn), "fill nonsym");
    check(matops::eigenvalues(a, out, &err), "eigen nonsym ok");
    check_near(out.get(0), 5.0, "eigen nonsym [0]", 1e-8);
    check_near(out.get(1), 2.0, "eigen nonsym [1]", 1e-8);

    // Defective (repeated eigenvalue 1, one eigenvector)
    const double vdef[4] = {1, 1, 0, 1};
    check(fill(a, 2, 2, vdef), "fill defective");
    check(matops::eigenvalues(a, out, &err), "eigen defective ok");
    check_near(out.get(0), 1.0, "eigen defective [0]", 1e-6);
    check_near(out.get(1), 1.0, "eigen defective [1]", 1e-6);

    // Rotation matrix: complex pair -> error by decision
    const double vrot[4] = {0, -1, 1, 0};
    check(fill(a, 2, 2, vrot), "fill rotation");
    err = nullptr;
    ok = matops::eigenvalues(a, out, &err);
    check_err(ok, err, "Complex eigenvalues", "eigen complex");

    // 5x5 with a complex pair buried in a real spectrum: block diag
    // of rotation(embedded) and diag(1,2,3) -> still an error.
    const double v5[25] = {0, -1, 0, 0, 0, 1, 0,  0, 0, 0, 0, 0, 1,
                           0, 0,  0, 0, 0, 2, 0,  0, 0, 0, 0, 3};
    check(fill(a, 5, 5, v5), "fill mixed 5x5");
    err = nullptr;
    ok = matops::eigenvalues(a, out, &err);
    check_err(ok, err, "Complex eigenvalues", "eigen mixed complex");

    // eigenvalues_complex (Phase 4C, D30/P4-7): the full spectrum,
    // conjugate pairs included, on the same fixtures above.
    {
        Complex ceig[matops::kMaxEigen];
        int ccount = 0;
        check(fill(a, 2, 2, vrot), "fill rotation (complex)");
        err = nullptr;
        check(matops::eigenvalues_complex(a, ceig, &ccount, &err), "eigen_c rotation ok");
        check(ccount == 2, "eigen_c rotation count");
        check_near(ceig[0].re, 0.0, "eigen_c rotation [0].re");
        check_near(ceig[0].im, 1.0, "eigen_c rotation [0].im");
        check_near(ceig[1].re, 0.0, "eigen_c rotation [1].re");
        check_near(ceig[1].im, -1.0, "eigen_c rotation [1].im");

        check(fill(a, 5, 5, v5), "fill mixed 5x5 (complex)");
        err = nullptr;
        check(matops::eigenvalues_complex(a, ceig, &ccount, &err), "eigen_c mixed ok");
        check(ccount == 5, "eigen_c mixed count");
        // Descending by real part; the conjugate pair (re=0) sorts
        // after the positive reals and before/among the rest by im.
        int real_count = 0;
        int complex_count = 0;
        for (int i = 0; i < ccount; ++i) {
            if (ceig[i].is_real()) {
                ++real_count;
            } else {
                ++complex_count;
            }
        }
        check(real_count == 3 && complex_count == 2, "eigen_c mixed real/complex split");

        // All-real input still yields an all-real spectrum via the
        // complex-capable entry point too.
        check(fill(a, 2, 2, vn), "fill nonsym (complex)");
        err = nullptr;
        check(matops::eigenvalues_complex(a, ceig, &ccount, &err), "eigen_c nonsym ok");
        check(ccount == 2 && ceig[0].is_real() && ceig[1].is_real(), "eigen_c nonsym all real");
        check_near(ceig[0].re, 5.0, "eigen_c nonsym [0]", 1e-8);
        check_near(ceig[1].re, 2.0, "eigen_c nonsym [1]", 1e-8);
    }

    // Size cap
    Array big;
    check(big.resize(11, 11), "resize 11x11");
    err = nullptr;
    ok = matops::eigenvalues(big, out, &err);
    check_err(ok, err, "Eigen limit is 10x10", "eigen cap");

    // 1x1
    const double v1[1] = {42};
    check(fill(a, 1, 1, v1), "fill 1x1");
    check(matops::eigenvalues(a, out, &err), "eigen 1x1 ok");
    check_near(out.get(0), 42.0, "eigen 1x1 value");

    // 8x8 symmetric with known spectrum: second-difference matrix,
    // eigenvalues 2 - 2*cos(k*pi/9), k = 1..8.
    Array t;
    check(t.resize(8, 8), "resize 8x8");
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            t.set(r, c, r == c ? 2.0 : (std::abs(r - c) == 1 ? -1.0 : 0.0));
        }
    }
    check(matops::eigenvalues(t, out, &err), "eigen 8x8 ok");
    check(out.size() == 8, "eigen 8x8 count");
    for (int k = 0; k < 8; ++k) {
        const double expected = 2.0 - 2.0 * std::cos((8 - k) * M_PI / 9.0);
        char what[48];
        std::snprintf(what, sizeof(what), "eigen 8x8 [%d]", k);
        check_near(out.get(k), expected, what, 1e-7);
    }
}

void test_psram_tier() {
    using namespace math;
    Array a;
    Array out;
    const char* err = nullptr;
    calc_t det = 0;

    // 50x50 = 2500 elements: PSRAM tier (over the 256-double slab cap).
    check(matops::identity(50, a, &err), "identity(50) ok");
    check(a.in_psram(), "50x50 is PSRAM tier");
    check(matops::determinant(a, &det, &err), "det identity(50) ok");
    check_near(det, 1.0, "det(identity(50))");

    // Tridiagonal 20x20: inverse round-trip through the PSRAM tier.
    Array t;
    check(t.resize(20, 20), "resize 20x20");
    for (int r = 0; r < 20; ++r) {
        for (int c = 0; c < 20; ++c) {
            t.set(r, c, r == c ? 2.0 : (std::abs(r - c) == 1 ? -1.0 : 0.0));
        }
    }
    check(t.in_psram(), "20x20 is PSRAM tier");
    Array inv;
    Array prod;
    check(matops::inverse(t, inv, &err), "inverse 20x20 ok");
    check(matops::mul(t, inv, prod, &err), "mul 20x20 ok");
    bool is_identity = true;
    for (int r = 0; r < 20 && is_identity; ++r) {
        for (int c = 0; c < 20; ++c) {
            const double expected = r == c ? 1.0 : 0.0;
            if (std::fabs(prod.get(r, c) - expected) > 1e-8) {
                is_identity = false;
                break;
            }
        }
    }
    check(is_identity, "20x20 a*inverse(a) = I");
}

// ---- Expression layer (mat_expr) ----

// 5.2.11: the same checks, the unified evaluator underneath. See eval_shim.hpp.
shim::Result eval_mat(const char* input) {
    return shim::eval(input);
}

void check_mat_result(const char* input, int rows, int cols, const double* expected,
                      const char* what) {
    ++g_checks;
    const auto res = eval_mat(input);
    if (res.kind != shim::Kind::kMatrix) {
        std::printf("FAIL: %s: '%s' -> kind %d, error %s (expected matrix)\n", what, input,
                    static_cast<int>(res.kind), res.error != nullptr ? res.error : "-");
        ++g_failures;
        return;
    }
    if (res.matrix->dim(0) != rows || res.matrix->dim(1) != cols) {
        std::printf("FAIL: %s: '%s' -> %dx%d (expected %dx%d)\n", what, input, res.matrix->dim(0),
                    res.matrix->dim(1), rows, cols);
        ++g_failures;
        return;
    }
    for (int i = 0; i < rows * cols; ++i) {
        if (std::fabs(res.matrix->get(i) - expected[i]) > 1e-9) {
            std::printf("FAIL: %s: '%s' [%d] -> %.12g (expected %.12g)\n", what, input, i,
                        res.matrix->get(i), expected[i]);
            ++g_failures;
            return;
        }
    }
}

void check_mat_scalar(const char* input, double expected, const char* what) {
    ++g_checks;
    const auto res = eval_mat(input);
    if (res.kind != shim::Kind::kScalar || !res.scalar.ok) {
        std::printf("FAIL: %s: '%s' -> kind %d, error %s (expected scalar)\n", what, input,
                    static_cast<int>(res.kind), res.error != nullptr ? res.error : "-");
        ++g_failures;
        return;
    }
    if (std::fabs(res.scalar.value - expected) > 1e-9) {
        std::printf("FAIL: %s: '%s' -> %.12g (expected %.12g)\n", what, input, res.scalar.value,
                    expected);
        ++g_failures;
    }
}

// dim()/eigenvals() compose now (register W7), so the suite needs a list check
// where it only ever needed matrix and scalar ones.
void check_mat_list(const char* input, const double* expected, int n, const char* what) {
    ++g_checks;
    const auto res = eval_mat(input);
    if (res.kind != shim::Kind::kList || res.list == nullptr || res.list->size() != n) {
        std::printf("FAIL: %s: '%s' -> kind %d (expected a list of %d)\n", what, input,
                    static_cast<int>(res.kind), n);
        ++g_failures;
        return;
    }
    for (int i = 0; i < n; ++i) {
        if (std::fabs(res.list->get(i) - expected[i]) > 1e-9) {
            std::printf("FAIL: %s: '%s' [%d] -> %.12g (expected %.12g)\n", what, input, i,
                        res.list->get(i), expected[i]);
            ++g_failures;
            return;
        }
    }
}

void check_mat_error(const char* input, const char* expected_err, const char* what) {
    ++g_checks;
    const auto res = eval_mat(input);
    if (res.kind != shim::Kind::kError || res.error == nullptr ||
        std::strcmp(res.error, expected_err) != 0) {
        std::printf("FAIL: %s: '%s' -> kind %d, error '%s' (expected '%s')\n", what, input,
                    static_cast<int>(res.kind), res.error != nullptr ? res.error : "-",
                    expected_err);
        ++g_failures;
    }
}

void test_expr_basics() {
    using namespace math;
    // [A] = [[1,2][3,4]], [B] = [[5,6][7,8]]
    const double va[4] = {1, 2, 3, 4};
    const double vb[4] = {5, 6, 7, 8};
    check(fill(matrices().matrix(0), 2, 2, va), "expr fill [A]");
    check(fill(matrices().matrix(1), 2, 2, vb), "expr fill [B]");

    // kNone was "not my syntax", the signal that drove the dispatch cascade.
    // One evaluator asks no such question (5.2.11): these evaluate.
    check(eval_mat("2+2").kind == shim::Kind::kScalar, "a scalar is just a scalar now");
    check(eval_mat("{1,2}+l1").kind != shim::Kind::kNone, "and list syntax is evaluated, not declined");

    const double vaa[4] = {1, 2, 3, 4};
    check_mat_result("[A]", 2, 2, vaa, "bare ref");
    check_mat_result("[a]", 2, 2, vaa, "lowercase ref");

    const double vsum[4] = {6, 8, 10, 12};
    check_mat_result("[A]+[B]", 2, 2, vsum, "add");
    const double vdiff[4] = {4, 4, 4, 4};
    check_mat_result("[B]-[A]", 2, 2, vdiff, "sub");
    const double vprod[4] = {19, 22, 43, 50};
    check_mat_result("[A]*[B]", 2, 2, vprod, "matrix product");
    const double vscale[4] = {2, 4, 6, 8};
    check_mat_result("2*[A]", 2, 2, vscale, "scalar*matrix");
    check_mat_result("[A]*2", 2, 2, vscale, "matrix*scalar");
    const double vhalf[4] = {0.5, 1, 1.5, 2};
    check_mat_result("[A]/2", 2, 2, vhalf, "matrix/scalar");
    const double vneg[4] = {-1, -2, -3, -4};
    check_mat_result("-[A]", 2, 2, vneg, "unary minus");

    const double vt[4] = {1, 3, 2, 4};
    check_mat_result("[A]^T", 2, 2, vt, "transpose via ^T");
    check_mat_result("transpose([A])", 2, 2, vt, "transpose()");
    const double vsq[4] = {7, 10, 15, 22};
    check_mat_result("[A]^2", 2, 2, vsq, "square");

    // inverse: [A]^-1 * [A] = I
    const double vi[4] = {1, 0, 0, 1};
    check_mat_result("[A]^-1*[A]", 2, 2, vi, "inverse via ^-1");
    check_mat_result("inverse([A])*[A]", 2, 2, vi, "inverse()");

    const double vid3[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    check_mat_result("identity(3)", 3, 3, vid3, "identity (no [X] token)");

    const double vaug[8] = {1, 2, 5, 6, 3, 4, 7, 8};
    check_mat_result("augment([A],[B])", 2, 4, vaug, "augment");

    // Mixed with scalar functions and parens
    const double vmix[4] = {3, 6, 9, 12};
    check_mat_result("(1+2)*[A]", 2, 2, vmix, "paren scalar times matrix");
    check_mat_result("[A]*sin(pi/2)*3", 2, 2, vmix, "scalar fn in matrix expr");

    check_mat_scalar("det([A])", -2.0, "det");
    check_mat_scalar("det([A])*2+1", -3.0, "det in scalar expr");
    check_mat_scalar("rank([A])", 2.0, "rank");
    check_mat_scalar("[A](2,1)", 3.0, "element access");
    check_mat_scalar("[A](1,2)+[B](2,2)", 10.0, "element arithmetic");

    // rref through the expression layer
    const double vr[4] = {1, 0, 0, 1};
    check_mat_result("rref([A])", 2, 2, vr, "rref");
}

void test_expr_store() {
    using namespace math;
    const double va[4] = {1, 2, 3, 4};
    check(fill(matrices().matrix(0), 2, 2, va), "store fill [A]");

    // Matrix store
    auto res = eval_mat("[A]*2 -> [C]");
    check(res.kind == shim::Kind::kMatrix && res.stored_matrix == 2 && res.matrices_modified,
          "store to [C]");
    check_near(matrices().matrix(2).get(1, 1), 8.0, "store [C] value");

    // Copy form
    res = eval_mat("[A] -> [D]");
    check(res.kind == shim::Kind::kMatrix && res.stored_matrix == 3, "copy [A] -> [D]");
    check_near(matrices().matrix(3).get(0, 1), 2.0, "copy [D] value");

    // MatAns holds the last matrix result
    check(mat_ans().dim(0) == 2, "MatAns rows");
    check_near(mat_ans().get(0, 0), 1.0, "MatAns value");

    // Scalar store
    res = eval_mat("det([A]) -> q");
    check(res.kind == shim::Kind::kScalar && res.scalar.stored_var == 'q' - 'a',
          "det -> q stored");
    check_near(engine().vars()['q'], -2.0, "q value");
    check_near(engine().vars().ans(), -2.0, "Ans updated");

    // dim/eigenvals list results
    res = eval_mat("dim([A])");
    check(res.kind == shim::Kind::kList && res.list->size() == 2, "dim kind");
    check_near(res.list->get(0), 2.0, "dim rows");
    check_near(res.list->get(1), 2.0, "dim cols");

    const double vs[9] = {2, -1, 0, -1, 2, -1, 0, -1, 2};
    check(fill(matrices().matrix(4), 3, 3, vs), "fill [E] sym");
    res = eval_mat("eigenvals([E]) -> l2");
    check(res.kind == shim::Kind::kList && res.stored_list == 1 && res.lists_modified,
          "eigenvals -> l2");
    check_near(lists().list(1).get(0), 2.0 + std::sqrt(2.0), "eigenvals l2[0]", 1e-8);

    // eigenvals() with a complex-conjugate pair (Phase 4C, D30/P4-7):
    // Kind::kText instead of Kind::kList, and a store target errors.
    const double vrot2[4] = {0, -1, 1, 0};
    // format_complex emits the font's slanted imaginary-unit glyph, not a
    // literal ASCII 'i' (testdrive 2026-07-20).
    char cpx_pair[12];
    std::snprintf(cpx_pair, sizeof(cpx_pair), "{%c,-%c}", math::kImagUnitGlyph,
                  math::kImagUnitGlyph);
    check(fill(matrices().matrix(4), 2, 2, vrot2), "fill [E] rotation");
    // A complex-conjugate spectrum was unstorable display text because lists
    // were real-only — which 4D.24 changed. It is a complex list now, and it
    // stores. Register W8. In REAL mode it is refused like any complex list,
    // which is where this check runs.
    (void)cpx_pair;
    check_mat_error("eigenvals([E])", "Non-real result", "REAL mode refuses a complex spectrum");
    check_mat_error("eigenvals([E]) -> l3", "Non-real result", "and refuses to store one");
    math::set_number_mode(math::NumberMode::kRectangular);
    res = eval_mat("eigenvals([E])");
    check(res.kind == shim::Kind::kList && res.list->dtype() == math::Dtype::kComplex,
          "in RECT it is a complex list (W8)");
    res = eval_mat("eigenvals([E]) -> l3");
    check(res.kind == shim::Kind::kList && res.stored_list == 2, "and it stores (W8)");
    math::set_number_mode(math::NumberMode::kReal);

    // eig(...) is an alias of eigenvals(...) — same list result, same
    // complex-text form, same "must stand alone" rejection.
    check(fill(matrices().matrix(4), 3, 3, vs), "fill [E] sym (eig alias)");
    res = eval_mat("eig([E]) -> l2");
    check(res.kind == shim::Kind::kList && res.stored_list == 1 && res.lists_modified,
          "eig alias -> l2");
    check_near(lists().list(1).get(0), 2.0 + std::sqrt(2.0), "eig alias l2[0]", 1e-8);
    check(fill(matrices().matrix(4), 2, 2, vrot2), "fill [E] rotation (eig alias)");
    check_mat_error("eig([E])", "Non-real result", "eig alias, same complex rule");
    // "must stand alone" was a consequence of matexpr::Value not holding a
    // list. Register W7.
    // [A] is {{1,2},{3,4}}: eigenvalues (5 +/- sqrt(33))/2, doubled.
    const double eig2[2] = {5.0 + std::sqrt(33.0), 5.0 - std::sqrt(33.0)};
    check_mat_list("2*eig([A])", eig2, 2, "eig composes (W7)");

    // Store mismatches
    check_mat_error("[A] -> l1", "Store target mismatch", "matrix to list target");
    check_mat_error("det([A]) -> [B]", "Store target mismatch", "scalar to matrix target");
    check_mat_error("dim([A]) -> [B]", "Store target mismatch", "list to matrix target");
    check_mat_error("[A] -> [k]", "Bad store target", "bad matrix name");
}

void test_expr_errors() {
    using namespace math;
    const double va[4] = {1, 2, 3, 4};
    const double vc[6] = {1, 2, 3, 4, 5, 6};
    check(fill(matrices().matrix(0), 2, 2, va), "err fill [A]");
    check(fill(matrices().matrix(1), 2, 3, vc), "err fill [B] 2x3");
    matrices().matrix(9).clear();

    check_mat_error("[A]+[B]", "Dim mismatch", "add dim mismatch");
    check_mat_error("[B]*[B]", "Dim mismatch", "mul dim mismatch");
    check_mat_error("[A]+2", "Dim mismatch", "matrix plus scalar");
    check_mat_error("[A]/[B]", "Matrix division: use ^-1", "matrix division");
    check_mat_error("2/[A]", "Matrix division: use ^-1", "scalar over matrix");
    check_mat_error("[J]", "Matrix is empty", "empty slot");
    check_mat_error("[A](3,1)", "Index out of range", "element out of range");
    check_mat_error("[A](1)", "Expected (row, col)", "one-index element");
    check_mat_error("sin([A])", "Matrix not allowed here", "matrix in scalar fn");
    check_mat_error("[A]^1.5", "Bad matrix exponent", "fractional power");
    check_mat_error("[A]^[A]", "Bad matrix exponent", "matrix exponent");
    check_mat_error("det([A]", "Syntax error", "unbalanced");
    check_mat_error("[A]*", "Syntax error", "trailing operator");
    const double dim2[2] = {4, 4};
    check_mat_list("2*dim([A])", dim2, 2, "dim composes (W7)");
    check_mat_error("[B]^T*[Q]", "Syntax error", "bad token");
    check_mat_error("det(identity(2))+", "Syntax error", "trailing plus");
}

// D48: matexpr recurses 808 B/level against core 0's 4 KB, so depth 3 walks
// off the stack. Hardware measured depth 2 at a 3,492 B high-water mark and
// det(([a]*([c]+[d]))+[d]) (depth 4) hard-faulted with SP below
// __StackBottom. These pin both sides of kMaxParseDepth = 2.
void test_expr_depth_cap() {
    using namespace math;
    const double va[4] = {1, 2, 3, 4};
    check(fill(matrices().matrix(0), 2, 2, va), "depth fill [A]");
    check(fill(matrices().matrix(1), 2, 2, va), "depth fill [B]");
    check(fill(matrices().matrix(2), 2, 2, va), "depth fill [C]");

    // Depth 1-3 evaluate. The depth-3 cases are the ones that force the cap
    // to 3: both are shipped behaviour, and both measured 3,940 of 4,096.
    const double vsum[4] = {2, 4, 6, 8};
    const double va2[4] = {1, 2, 3, 4};
    check_mat_scalar("det([A])", -2.0, "depth 1 accepted");
    check_mat_scalar("det([A]*[B]+[C])", -8.0, "depth 2 accepted");
    check_mat_result("([A]+[B])", 2, 2, vsum, "depth 2 via paren accepted");
    check_mat_scalar("det([[1,2][3,4]])", -2.0, "depth 3: literal in function arg");
    check_mat_scalar("det(identity(2))", 1.0, "depth 3: nested function call");
    check_mat_result("(([A]))", 2, 2, va2, "depth 3 via parens accepted");
    check_mat_scalar("det(([A]+[A])*[A])", 16.0, "depth 3 via paren in arg");

    // Depth 4 is rejected rather than faulting — this is the shape that
    // hard-faulted on the Pico 1 with sp below __StackBottom.
    // Depth 4 used to be rejected — the shape that hard-faulted the Pico 1 with
    // sp below __StackBottom (D48). Depth is operand-stack slots now, so all
    // three evaluate. Register W9, and the user-visible payoff of the phase.
    check_mat_scalar("det(([A]*([A]+[A]))+[A])", -6.0, "the HW crash shape now evaluates");
    check_mat_result("((([A])))", 2, 2, va2, "depth 4 paren chain evaluates");
    check_mat_scalar("det(inverse(([A])))", -0.5, "depth 4 nested calls evaluate");

    // The guard is RAII precisely so siblings do not accumulate: parse_term
    // and parse_expr call parse_unary in a loop, and a flat expression of any
    // length stays at depth 1. This is the regression that a naive
    // increment-without-unwind would break.
    check_mat_scalar("det([A])+det([A])+det([A])+det([A])+det([A])+det([A])", -12.0,
                     "flat sibling chain not accumulated");
    check_mat_scalar("det([A])*2+det([B])*2+det([C])*2", -12.0, "flat mixed-operator chain");
    check_mat_scalar("---det([A])", 2.0, "unary sign chain not accumulated");
}

// D48 follow-up: parse_scalar_span must not hand a bare numeric literal to
// eval_field, which drags the whole tinyexpr engine onto the stack at the
// leaf of the recursion (D47's a0939bf, applied to complexexpr but not here).
// The Pico 2 hard-faulted on det([[1,2][3,4]]) and det(identity(2)) at depth
// 3 because of it, while det([a]*[c]+[d]) — no literal at depth — was fine.
// These pin that the fast path parses the same values the engine did, and
// that everything it must *not* swallow still reaches the fallbacks.
void test_scalar_span_fast_path() {
    using namespace math;
    const double va[4] = {1, 2, 3, 4};
    check(fill(matrices().matrix(0), 2, 2, va), "fastpath fill [A]");

    // Plain literals: integer, decimal, leading dot, exponent forms.
    check_mat_scalar("det(2*[A])", -8.0, "integer literal");
    check_mat_scalar("det(0.5*[A])", -0.5, "decimal literal");
    check_mat_scalar("det(.5*[A])", -0.5, "leading-dot literal");
    check_mat_scalar("det(2e0*[A])", -8.0, "exponent literal");
    check_mat_scalar("det(2E+0*[A])", -8.0, "signed-exponent literal");
    const double vlit[4] = {1, 2, 3, 4};
    check_mat_result("[[1,2][3,4]]", 2, 2, vlit, "matrix literal elements");
    check_mat_scalar("det([[1.5,0][0,2]])", 3.0, "decimal literal in matrix literal");

    // Must NOT take the fast path — strtod stops short of the span end, so
    // these still reach eval_field or the complex evaluator.
    check_mat_scalar("det(2*3*[A])", -72.0, "literal arithmetic still evaluated");
    check_mat_scalar("det(pi*0*[A]+[A])", -2.0, "constant folded via eval_field");
    check_mat_scalar("det(sin(0)*[A]+[A])", -2.0, "function call via eval_field");
}

void test_format_matrix() {
    using namespace math;
    Array m;
    const double v[4] = {1, 2.5, -3, 4};
    check(fill(m, 2, 2, v), "format fill");
    char buf[64];
    format_matrix(m, buf, sizeof(buf));
    check(std::strcmp(buf, "[[1,2.5][-3,4]]") == 0, "format small matrix");

    // Cells use the compact formatter: irrational entries cap at 4 sig
    // figs instead of the full 10 (2026-07-27 testdrive readability).
    Array frac;
    const double fv[4] = {1.0 / 3.0, 2.5, -3, 4};
    check(fill(frac, 2, 2, fv), "compact fill");
    format_matrix(frac, buf, sizeof(buf));
    check(std::strcmp(buf, "[[0.3333,2.5][-3,4]]") == 0, "format compact cell");

    // format_matrix_frac (>Frac on matrices): real cells become p/q.
    Array fr;
    const double frv[4] = {0.5, 0.25, 0.75, 0.2};
    check(fill(fr, 2, 2, frv), "frac fill");
    format_matrix_frac(fr, buf, sizeof(buf));
    check(std::strcmp(buf, "[[1/2,1/4][3/4,1/5]]") == 0, "format_matrix_frac");

    // Near-zero cleanup: roundoff cells (2.22e-16 vs a max of 1) snap to a
    // clean "0" — so [A]^-1*[A] prints an exact identity, not scientific
    // noise. Relative to the matrix's own scale.
    Array chop;
    const double cz[4] = {1, 2.220446049250313e-16, 0, 1};
    check(fill(chop, 2, 2, cz), "chop fill");
    format_matrix(chop, buf, sizeof(buf));
    check(std::strcmp(buf, "[[1,0][0,1]]") == 0, "near-zero real cell snaps to 0");

    // A genuinely tiny-magnitude matrix is preserved (its own max sets the
    // threshold, so nothing is 12 orders below it).
    Array tiny;
    const double tv[4] = {1e-15, 2e-15, 3e-15, 4e-15};
    check(fill(tiny, 2, 2, tv), "tiny fill");
    format_matrix(tiny, buf, sizeof(buf));
    check(std::strstr(buf, "1e-15") != nullptr && std::strcmp(buf, "[[0,0][0,0]]") != 0,
          "tiny-magnitude matrix not chopped");

    // Complex: a roundoff-magnitude cell snaps to 0 too (|z| relative).
    Array cchop;
    const Complex ccz[4] = {Complex(1, 0), Complex(2.22e-16, -4.44e-16), Complex(0, 0),
                            Complex(1, 0)};
    check(cfill(cchop, 2, 2, ccz), "cchop fill");
    format_matrix(cchop, buf, sizeof(buf));
    check(std::strcmp(buf, "[[1,0][0,1]]") == 0, "near-zero complex cell snaps to 0");

    Array big;
    const char* err = nullptr;
    check(matops::identity(10, big, &err), "identity(10)");
    char small[24];
    format_matrix(big, small, sizeof(small));
    check(std::strchr(small, math::kEllipsisGlyph) != nullptr, "format truncates");
    check(small[std::strlen(small) - 1] == ']', "format truncation closes");
}

void test_store() {
    using namespace math;
    auto& store = matrices();
    check(MatrixStore::kCount == 10, "store has 10 slots");
    const double v[4] = {1, 2, 3, 4};
    check(fill(store.matrix(0), 2, 2, v), "store [A] fill");
    check_near(store.matrix(0).get(1, 1), 4.0, "store [A] readback");
    store.matrix(0).clear();
    check(store.matrix(0).size() == 0, "store [A] cleared");
}

// ---- 4D.25: complex matrices ----

void test_complex_matops() {
    using namespace math;
    Array a;
    Array b;
    Array out;
    const char* err = nullptr;
    bool ok = false;

    // A = [[1+i, 2],[3, 4-i]]
    const Complex va[4] = {{1, 1}, {2, 0}, {3, 0}, {4, -1}};
    check(cfill(a, 2, 2, va), "cfill A");
    check(a.in_psram(), "complex matrix rides the PSRAM tier");

    // det(A) = (1+i)(4-i) - 6 = -1+3i
    Complex d;
    check(matops::determinant(a, &d, &err), "complex det ok");
    check_cnear(d, -1, 3, "complex det value");
    // The real-only det entry point refuses complex input (D37).
    calc_t rd = 0;
    err = nullptr;
    ok = matops::determinant(a, &rd, &err);
    check_err(ok, err, "Non-real matrix", "real det on complex");

    // A * A^-1 = I
    check(matops::inverse(a, out, &err), "complex inverse ok");
    Array prod;
    check(matops::mul(a, out, prod, &err), "A * Ainv ok");
    check_cnear(prod.cget(0, 0), 1, 0, "A*Ainv [0,0]");
    check_cnear(prod.cget(0, 1), 0, 0, "A*Ainv [0,1]");
    check_cnear(prod.cget(1, 0), 0, 0, "A*Ainv [1,0]");
    check_cnear(prod.cget(1, 1), 1, 0, "A*Ainv [1,1]");

    // Complex scalar times a real matrix promotes.
    Array r;
    const double vr[4] = {1, 2, 3, 4};
    check(fill(r, 2, 2, vr), "fill real R");
    check(matops::scalar_mul(r, Complex(0, 1), out, &err), "i * real matrix ok");
    check(out.dtype() == Dtype::kComplex, "i * real matrix is complex");
    check_cnear(out.cget(1, 0), 0, 3, "i*3 = 3i");

    // Mixed real/complex elementwise + augment promote too.
    check(matops::add(a, r, out, &err), "mixed add ok");
    check_cnear(out.cget(0, 0), 2, 1, "mixed add [0,0]");
    check_cnear(out.cget(1, 1), 8, -1, "mixed add [1,1]");
    check(matops::augment(r, a, out, &err), "mixed augment ok");
    check(out.dim(0) == 2 && out.dim(1) == 4 && out.dtype() == Dtype::kComplex,
          "mixed augment shape/dtype");
    check_cnear(out.cget(0, 2), 1, 1, "mixed augment value");

    // Transpose.
    check(matops::transpose(a, out, &err), "complex transpose ok");
    check_cnear(out.cget(0, 1), 3, 0, "transpose [0,1]");
    check_cnear(out.cget(1, 0), 2, 0, "transpose [1,0]");
    check_cnear(out.cget(1, 1), 4, -1, "transpose [1,1]");

    // (i*I)^2 = -I.
    const Complex vj[4] = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
    check(cfill(b, 2, 2, vj), "cfill iI");
    check(matops::power(b, 2, out, &err), "complex power ok");
    check_cnear(out.cget(0, 0), -1, 0, "power [0,0]");
    check_cnear(out.cget(0, 1), 0, 0, "power [0,1]");

    // rref([[i,2i],[1,3]]) = I; rank 2.
    const Complex vc2[4] = {{0, 1}, {0, 2}, {1, 0}, {3, 0}};
    check(cfill(b, 2, 2, vc2), "cfill rref input");
    check(matops::rref(b, out, &err), "complex rref ok");
    check_cnear(out.cget(0, 0), 1, 0, "rref [0,0]");
    check_cnear(out.cget(0, 1), 0, 0, "rref [0,1]");
    check_cnear(out.cget(1, 1), 1, 0, "rref [1,1]");
    int rk = 0;
    check(matops::rank(b, &rk, &err) && rk == 2, "complex rank 2");

    // Rank-deficient: [[i,2i],[2i,4i]] — rank 1, det 0, singular inverse.
    const Complex vr1[4] = {{0, 1}, {0, 2}, {0, 2}, {0, 4}};
    check(cfill(b, 2, 2, vr1), "cfill rank-1");
    check(matops::rank(b, &rk, &err) && rk == 1, "complex rank 1");
    check(matops::determinant(b, &d, &err), "singular complex det ok");
    check_cnear(d, 0, 0, "singular complex det 0");
    err = nullptr;
    ok = matops::inverse(b, out, &err);
    check_err(ok, err, "Singular matrix", "complex singular inverse");

    // Eigen stays real-input (D37).
    Array eout;
    err = nullptr;
    ok = matops::eigenvalues(b, eout, &err);
    check_err(ok, err, "Non-real matrix", "eigenvalues complex");
    Complex ce[matops::kMaxEigen];
    int cnt = 0;
    err = nullptr;
    ok = matops::eigenvalues_complex(b, ce, &cnt, &err);
    check_err(ok, err, "Non-real matrix",
              "eigenvalues_complex complex");

    // copy adopts the source dtype.
    Array dst;
    check(fill(dst, 1, 1, vr), "fill dst real");
    check(matops::copy(a, dst), "copy complex ok");
    check(dst.dtype() == Dtype::kComplex, "copy adopts complex");
    check_cnear(dst.cget(1, 1), 4, -1, "copy value");

    // make_complex migrates in place, preserving shape and values.
    Array mr;
    const double vmr[6] = {1, 2, 3, 4, 5, 6};
    check(fill(mr, 2, 3, vmr), "fill 2x3 for migration");
    check(matops::make_complex(mr), "make_complex ok");
    check(mr.dtype() == Dtype::kComplex && mr.dim(0) == 2 && mr.dim(1) == 3,
          "make_complex shape/dtype");
    check_cnear(mr.cget(1, 2), 6, 0, "make_complex value");
    check(matops::make_complex(mr), "make_complex idempotent");

    // reshape preserves dtype; new cells are complex zero.
    check(matops::reshape(a, 3, 3, out, &err), "complex reshape ok");
    check(out.dtype() == Dtype::kComplex, "reshape keeps complex");
    check_cnear(out.cget(0, 0), 1, 1, "reshape kept value");
    check_cnear(out.cget(2, 2), 0, 0, "reshape zero fill");

    // identity into a previously-complex temp retypes to real.
    check(matops::identity(2, out, &err), "identity after complex ok");
    check(out.dtype() == Dtype::kDouble, "identity retypes real");
    check_near(out.get(1, 1), 1.0, "identity value");
}

void test_complex_expr_layer() {
    using namespace math;
    set_number_mode(NumberMode::kRectangular);

    // [A] complex, [B] real.
    const Complex va[4] = {{1, 1}, {2, 0}, {3, 0}, {4, -1}};
    check(cfill(matrices().matrix(0), 2, 2, va), "cexpr fill [A]");
    const double vb[4] = {5, 6, 7, 8};
    check(fill(matrices().matrix(1), 2, 2, vb), "cexpr fill [B]");

    // det([A]) is a complex scalar; Ans commits complex.
    auto res = eval_mat("det([A])");
    check(res.kind == shim::Kind::kScalar && res.scalar_complex, "det([A]) complex scalar");
    check_cnear(res.cvalue, -1, 3, "det([A]) value");
    check(engine().vars().is_complex(Variables::kAns), "complex Ans committed");

    // Element access promotes.
    res = eval_mat("[A](1,1)");
    check(res.kind == shim::Kind::kScalar && res.scalar_complex, "[A](1,1) complex");
    check_cnear(res.cvalue, 1, 1, "[A](1,1) value");

    // Complex scalar subterms scale a real matrix.
    res = eval_mat("i*[B]");
    check(res.kind == shim::Kind::kMatrix && res.matrix->dtype() == Dtype::kComplex,
          "i*[B] complex matrix");
    check_cnear(res.matrix->cget(0, 0), 0, 5, "i*[B] value");
    res = eval_mat("2i*[B]");
    check(res.kind == shim::Kind::kMatrix, "2i shorthand parses");
    check_cnear(res.matrix->cget(0, 1), 0, 12, "2i*[B] value");

    // Mixed matrix arithmetic.
    res = eval_mat("[A]+[B]");
    check(res.kind == shim::Kind::kMatrix, "[A]+[B] ok");
    check_cnear(res.matrix->cget(0, 0), 6, 1, "[A]+[B] value");

    // Stores preserve the dtype (and complex scalars store to vars).
    res = eval_mat("[A]->[C]");
    check(res.kind == shim::Kind::kMatrix && res.stored_matrix == 2 &&
              matrices().matrix(2).dtype() == Dtype::kComplex,
          "[A]->[C] keeps complex");
    res = eval_mat("[A](1,1)->z");
    check(res.kind == shim::Kind::kScalar && res.scalar_complex &&
              engine().vars().is_complex('z' - 'a'),
          "complex scalar var store");
    engine().vars().set_real('z' - 'a', 0);

    // format_matrix uses the complex formatter.
    char buf[96];
    format_matrix(matrices().matrix(0), buf, sizeof(buf));
    check(std::strchr(buf, kImagUnitGlyph) != nullptr, "format_matrix complex glyph");

    // Complex cells use the compact per-component formatter too: a
    // fractional real/imag part caps at 4 sig figs, not the full 10.
    Array cfrac;
    const Complex cv[1] = {{1.0 / 3.0, 1.0 / 3.0}};
    check(cfill(cfrac, 1, 1, cv), "compact complex fill");
    format_matrix(cfrac, buf, sizeof(buf));
    check(std::strstr(buf, "0.3333") != nullptr && std::strstr(buf, "0.3333333") == nullptr,
          "format_matrix compact complex cell");

    // eigenvals of a complex matrix errors (D37).
    check_mat_error("eigenvals([A])", "Non-real matrix", "eigenvals complex errors");

    // REAL mode gates every complex touch (Batch 1 rule).
    set_number_mode(NumberMode::kReal);
    check_mat_error("[A]+[A]", "Non-real result", "REAL gate on complex [X]");
    check_mat_error("det([A])", "Non-real result", "REAL gate on det");
    check_mat_error("i*[B]", "Non-real result", "REAL gate on complex scalar");
    check_mat_scalar("det([B])", -2.0, "real det still fine in REAL mode");

    // Cleanup: real store slots, real Ans, REAL mode.
    check(fill(matrices().matrix(0), 1, 1, vb), "cexpr cleanup [A]");
    check(fill(matrices().matrix(2), 1, 1, vb), "cexpr cleanup [C]");
    engine().vars().set_real(Variables::kAns, 0);
}

// ---- Batch 5 (4D.12/14/22): literals, MatAns, list<->matrix, norm ----

void test_matrix_literals() {
    using namespace math;
    const double vaa[4] = {1, 2, 3, 4};
    auto res = eval_mat("[[1,2][3,4]]");
    check(res.kind == shim::Kind::kMatrix, "literal evaluates");
    check_matrix(*res.matrix, 2, 2, vaa, "literal values");

    const double vrow[3] = {5, 7, 11};
    check_mat_result("[[2+3,7,11]]", 1, 3, vrow, "1-row literal with exprs");

    const double vsum[4] = {2, 4, 6, 8};
    check_mat_result("[[1,2][3,4]]+[[1,2][3,4]]", 2, 2, vsum, "literal arithmetic");
    check_mat_scalar("det([[1,2][3,4]])", -2.0, "det of literal");

    res = eval_mat("[[1,2][3,4]] -> [E]");
    check(res.kind == shim::Kind::kMatrix && res.stored_matrix == 4, "literal store");
    check_near(matrices().matrix(4).get(1, 0), 3.0, "stored literal value");

    check_mat_error("[[1,2][3]]", "Dim mismatch", "ragged literal errors");
    check_mat_error("[[1,2][3,4]", "Syntax error", "unterminated literal");

    // Complex literal in RECT mode; gated in REAL.
    set_number_mode(NumberMode::kRectangular);
    res = eval_mat("[[i,0][0,1]]");
    check(res.kind == shim::Kind::kMatrix && res.matrix->dtype() == Dtype::kComplex,
          "complex literal");
    check_cnear(res.matrix->cget(0, 0), 0, 1, "complex literal value");
    set_number_mode(NumberMode::kReal);
    check_mat_error("[[i,0][0,1]]", "Non-real result", "REAL gates complex literal");
}

void test_matans_token() {
    using namespace math;
    const double vaa[4] = {1, 2, 3, 4};
    check(eval_mat("[[1,2][3,4]]").kind == shim::Kind::kMatrix, "seed MatAns");
    check_mat_result("matans", 2, 2, vaa, "matans recalls");
    const double vdbl[4] = {2, 4, 6, 8};
    check_mat_result("2*matans", 2, 2, vdbl, "matans in expression");
    check_mat_scalar("det(matans)", -8.0, "det(matans) after 2*matans");
    auto res = eval_mat("matans -> [F]");
    check(res.kind == shim::Kind::kMatrix && res.stored_matrix == 5, "matans store");
}

void test_list_matrix_bridge() {
    using namespace math;
    // l1 = {1,2,3}, l2 = {4,5} (shorter: zero-pads).
    auto& l1 = lists().list(0);
    auto& l2 = lists().list(1);
    l1.clear();
    check(l1.set_dtype(Dtype::kDouble) && l1.resize(3), "l1 resize");
    l1.set(0, 1);
    l1.set(1, 2);
    l1.set(2, 3);
    l2.clear();
    check(l2.set_dtype(Dtype::kDouble) && l2.resize(2), "l2 resize");
    l2.set(0, 4);
    l2.set(1, 5);

    const double vpk[6] = {1, 4, 2, 5, 3, 0};
    check_mat_result("list2mat(l1,l2)", 3, 2, vpk, "list2mat packs columns");
    auto res = eval_mat("list2mat(l1,l2) -> [G]");
    check(res.stored_matrix == 6, "list2mat store");

    // Round-trip back out.
    res = eval_mat("mat2list([G], l3, l4)");
    check(res.kind == shim::Kind::kText, "mat2list returns text");  // "Done (n lists)", D1
    check((res.lists_mask & 0b1100) == 0b1100, "mat2list mask");
    check(lists().list(2).size() == 3 && lists().list(2).get(2) == 3, "l3 column values");
    check(lists().list(3).size() == 3 && lists().list(3).get(1) == 5, "l4 column values");

    check_mat_error("list2mat(x)", "list2mat takes l1-l6 args", "list2mat bad arg");
    check_mat_error("mat2list([G])", "mat2list needs ([A], l1, ...)", "mat2list no targets");
    check_mat_error("mat2list([G], l3) -> [C]", "mat2list must stand alone", "mat2list store");

    l1.clear();
    l2.clear();
    lists().list(2).clear();
    lists().list(3).clear();
}

void test_frobenius_norm() {
    using namespace math;
    const double v[4] = {3, 4, 0, 0};
    check(fill(matrices().matrix(7), 2, 2, v), "norm fill [H]");
    check_mat_scalar("norm([H])", 5.0, "Frobenius norm");
    check_mat_scalar("norm([H])^2", 25.0, "norm in expression");
    // Complex: sqrt(|i|^2 + |2|^2) = sqrt(5).
    set_number_mode(NumberMode::kRectangular);
    const Complex vc[2] = {{0, 1}, {2, 0}};
    check(cfill(matrices().matrix(7), 1, 2, vc), "norm cfill");
    check_mat_scalar("norm([H])", std::sqrt(5.0), "complex Frobenius norm");
    set_number_mode(NumberMode::kReal);
    matrices().matrix(7).clear();
    matrices().matrix(7).set_dtype(Dtype::kDouble);
}

// Eigenvectors (4D.23, Batch 8): rref nullspace of (A - lambda I).
void test_eigenvectors() {
    using namespace math;
    Array a;
    Array v;
    const char* err = nullptr;
    bool ok = false;

    // Diagonal: eigenvalues {3, 1} -> columns e1, e2.
    const double vd[4] = {3, 0, 0, 1};
    check(fill(a, 2, 2, vd), "evec fill diag");
    check(matops::eigenvectors(a, v, &err), "evec diag ok");
    const double ve[4] = {1, 0, 0, 1};
    check_matrix(v, 2, 2, ve, "evec diag columns", 1e-8);

    // Symmetric [[2,1],[1,2]]: lambda 3 -> [1,1]/sqrt2, lambda 1 -> [1,-1]/sqrt2.
    const double vs[4] = {2, 1, 1, 2};
    check(fill(a, 2, 2, vs), "evec fill sym");
    check(matops::eigenvectors(a, v, &err), "evec sym ok");
    const double s = std::sqrt(0.5);
    check_near(v.get(0, 0), s, "sym v1[0]", 1e-8);
    check_near(v.get(1, 0), s, "sym v1[1]", 1e-8);
    check_near(std::fabs(v.get(0, 1)), s, "sym v2 magnitude", 1e-8);
    check_near(v.get(0, 1) + v.get(1, 1), 0.0, "sym v2 antisymmetric", 1e-8);

    // A*V == V*D column-wise for a general 3x3.
    const double vg[9] = {4, 1, 0, 1, 3, 1, 0, 1, 2};
    check(fill(a, 3, 3, vg), "evec fill 3x3");
    Array evals;
    check(matops::eigenvalues(a, evals, &err), "evec 3x3 eigenvalues");
    check(matops::eigenvectors(a, v, &err), "evec 3x3 ok");
    for (int k = 0; k < 3; ++k) {
        const double lambda = evals.get(k);
        for (int i = 0; i < 3; ++i) {
            double av = 0;
            for (int j = 0; j < 3; ++j) {
                av += a.get(i, j) * v.get(j, k);
            }
            check_near(av, lambda * v.get(i, k), "evec 3x3 A*v = lambda*v", 1e-7);
        }
    }

    // Defective (Jordan block) and repeated spectra refuse.
    const double vj[4] = {2, 1, 0, 2};
    check(fill(a, 2, 2, vj), "evec fill jordan");
    err = nullptr;
    ok = matops::eigenvectors(a, v, &err);
    check_err(ok, err, "No unique eigenvector", "evec defective");
    const double vi[4] = {1, 0, 0, 1};
    check(fill(a, 2, 2, vi), "evec fill identity");
    err = nullptr;
    ok = matops::eigenvectors(a, v, &err);
    check_err(ok, err, "No unique eigenvector", "evec repeated");

    // Rotation: complex pair refuses.
    const double vr[4] = {0, -1, 1, 0};
    check(fill(a, 2, 2, vr), "evec fill rotation");
    err = nullptr;
    ok = matops::eigenvectors(a, v, &err);
    check_err(ok, err, "Complex eigenvalues", "evec complex");

    // matexpr exposure: composable matrix result.
    check(fill(matrices().matrix(8), 2, 2, vd), "evec fill [I]");
    check_mat_result("eigenvec([I])", 2, 2, ve, "eigenvec([I])");
    const double v2e[4] = {2, 0, 0, 2};
    check_mat_result("2*eigenvec([I])", 2, 2, v2e, "2*eigenvec composes");
    matrices().matrix(8).clear();
}

}  // namespace

int main() {
    test_arithmetic();
    test_identity_augment();
    test_determinant();
    test_inverse();
    test_rref_rank();
    test_reshape();
    test_power();
    test_eigenvalues();
    test_psram_tier();
    test_expr_basics();
    test_expr_store();
    test_expr_errors();
    test_expr_depth_cap();
    test_scalar_span_fast_path();
    test_format_matrix();
    test_store();
    test_complex_matops();
    test_complex_expr_layer();
    test_matrix_literals();
    test_matans_token();
    test_list_matrix_bridge();
    test_frobenius_norm();
    test_eigenvectors();

    std::printf("test_matrix: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
