// Host-side tests for the Phase 4A matrix stack: matops over 2-D
// Arrays and MatrixStore. The PSRAM tier runs against the malloc shim
// in host_psram_backend.cpp.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/array.hpp"
#include "math/engine.hpp"
#include "math/lists.hpp"
#include "math/mat_expr.hpp"
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

void check_err(bool ok, const char* err, const char* expected, const char* what) {
    ++g_checks;
    if (ok || err == nullptr || std::strcmp(err, expected) != 0) {
        std::printf("FAIL: %s -> ok=%d err='%s' (expected '%s')\n", what, ok ? 1 : 0,
                    err != nullptr ? err : "-", expected);
        ++g_failures;
    }
}

// Fill a rows x cols matrix from a flat row-major initializer.
bool fill(math::Array& m, int rows, int cols, const double* vals) {
    if (!m.resize(rows, cols)) {
        return false;
    }
    m.write_range(0, rows * cols, vals);
    return true;
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
    check_err(matops::add(a, c, out, &err), err, "Dim mismatch", "add dim mismatch");
    err = nullptr;
    check_err(matops::mul(a, b, out, &err), err, "Dim mismatch", "mul dim mismatch");

    // Empty operand
    Array e;
    err = nullptr;
    check_err(matops::add(e, b, out, &err), err, "Not a matrix", "add empty");
}

void test_identity_augment() {
    using namespace math;
    Array a;
    Array b;
    Array out;
    const char* err = nullptr;

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
    check_err(matops::augment(a, c, out, &err), err, "Dim mismatch", "augment row mismatch");
}

void test_determinant() {
    using namespace math;
    Array a;
    Array out;
    const char* err = nullptr;
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
    check_err(matops::determinant(a, &det, &err), err, "Not square", "det non-square");
}

void test_inverse() {
    using namespace math;
    Array a;
    Array inv;
    Array prod;
    const char* err = nullptr;

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
    check_err(matops::inverse(a, inv, &err), err, "Singular matrix", "inverse singular");
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
    check_err(matops::reshape(a, 0, 3, out, &err), err, "Dim mismatch", "reshape zero rows");
    err = nullptr;
    check_err(matops::reshape(a, 100, 3, out, &err), err, "Dim mismatch", "reshape over cap");
}

void test_power() {
    using namespace math;
    Array a;
    Array out;
    const char* err = nullptr;

    const double v[4] = {1, 1, 0, 1};
    check(fill(a, 2, 2, v), "fill shear");
    check(matops::power(a, 0, out, &err), "power 0 ok");
    const double vi[4] = {1, 0, 0, 1};
    check_matrix(out, 2, 2, vi, "a^0 = I");

    check(matops::power(a, 5, out, &err), "power 5 ok");
    const double v5[4] = {1, 5, 0, 1};
    check_matrix(out, 2, 2, v5, "shear^5");

    err = nullptr;
    check_err(matops::power(a, 101, out, &err), err, "Exponent out of range", "power cap");

    Array r;
    const double vr[6] = {1, 2, 3, 4, 5, 6};
    check(fill(r, 2, 3, vr), "fill 2x3");
    err = nullptr;
    check_err(matops::power(r, 2, out, &err), err, "Not square", "power non-square");
}

void test_eigenvalues() {
    using namespace math;
    Array a;
    Array out;
    const char* err = nullptr;

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
    check_err(matops::eigenvalues(a, out, &err), err, "Complex eigenvalues", "eigen complex");

    // 5x5 with a complex pair buried in a real spectrum: block diag
    // of rotation(embedded) and diag(1,2,3) -> still an error.
    const double v5[25] = {0, -1, 0, 0, 0, 1, 0,  0, 0, 0, 0, 0, 1,
                           0, 0,  0, 0, 0, 2, 0,  0, 0, 0, 0, 3};
    check(fill(a, 5, 5, v5), "fill mixed 5x5");
    err = nullptr;
    check_err(matops::eigenvalues(a, out, &err), err, "Complex eigenvalues", "eigen mixed complex");

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
    check_err(matops::eigenvalues(big, out, &err), err, "Eigen limit is 10x10", "eigen cap");

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

math::matexpr::Result eval_mat(const char* input) {
    return math::matexpr::evaluate(input);
}

void check_mat_result(const char* input, int rows, int cols, const double* expected,
                      const char* what) {
    ++g_checks;
    const auto res = eval_mat(input);
    if (res.kind != math::matexpr::Kind::kMatrix) {
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
    if (res.kind != math::matexpr::Kind::kScalar || !res.scalar.ok) {
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

void check_mat_error(const char* input, const char* expected_err, const char* what) {
    ++g_checks;
    const auto res = eval_mat(input);
    if (res.kind != math::matexpr::Kind::kError || res.error == nullptr ||
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

    check(eval_mat("2+2").kind == matexpr::Kind::kNone, "non-matrix input -> kNone");
    check(eval_mat("{1,2}+l1").kind == matexpr::Kind::kNone, "list input -> kNone");

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
    check(res.kind == matexpr::Kind::kMatrix && res.stored_matrix == 2 && res.matrices_modified,
          "store to [C]");
    check_near(matrices().matrix(2).get(1, 1), 8.0, "store [C] value");

    // Copy form
    res = eval_mat("[A] -> [D]");
    check(res.kind == matexpr::Kind::kMatrix && res.stored_matrix == 3, "copy [A] -> [D]");
    check_near(matrices().matrix(3).get(0, 1), 2.0, "copy [D] value");

    // MatAns holds the last matrix result
    check(matexpr::mat_ans().dim(0) == 2, "MatAns rows");
    check_near(matexpr::mat_ans().get(0, 0), 1.0, "MatAns value");

    // Scalar store
    res = eval_mat("det([A]) -> q");
    check(res.kind == matexpr::Kind::kScalar && res.scalar.stored_var == 'q' - 'a',
          "det -> q stored");
    check_near(engine().vars()['q'], -2.0, "q value");
    check_near(engine().vars().ans(), -2.0, "Ans updated");

    // dim/eigenvals list results
    res = eval_mat("dim([A])");
    check(res.kind == matexpr::Kind::kList && res.list->size() == 2, "dim kind");
    check_near(res.list->get(0), 2.0, "dim rows");
    check_near(res.list->get(1), 2.0, "dim cols");

    const double vs[9] = {2, -1, 0, -1, 2, -1, 0, -1, 2};
    check(fill(matrices().matrix(4), 3, 3, vs), "fill [E] sym");
    res = eval_mat("eigenvals([E]) -> l2");
    check(res.kind == matexpr::Kind::kList && res.stored_list == 1 && res.lists_modified,
          "eigenvals -> l2");
    check_near(lists().list(1).get(0), 2.0 + std::sqrt(2.0), "eigenvals l2[0]", 1e-8);

    // eigenvals() with a complex-conjugate pair (Phase 4C, D30/P4-7):
    // Kind::kText instead of Kind::kList, and a store target errors.
    const double vrot2[4] = {0, -1, 1, 0};
    check(fill(matrices().matrix(4), 2, 2, vrot2), "fill [E] rotation");
    res = eval_mat("eigenvals([E])");
    check(res.kind == matexpr::Kind::kText, "eigenvals complex kind");
    check(res.text != nullptr && std::strcmp(res.text, "{i,-i}") == 0, "eigenvals complex text");
    check_mat_error("eigenvals([E]) -> l3", "Complex results can't be stored",
                    "eigenvals complex store rejected");

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
    check_mat_error("2*dim([A])", "dim/eigenvals must stand alone", "nested dim");
    check_mat_error("[B]^T*[Q]", "Syntax error", "bad token");
    check_mat_error("det(identity(2))+", "Syntax error", "trailing plus");
}

void test_format_matrix() {
    using namespace math;
    Array m;
    const double v[4] = {1, 2.5, -3, 4};
    check(fill(m, 2, 2, v), "format fill");
    char buf[64];
    matexpr::format_matrix(m, buf, sizeof(buf));
    check(std::strcmp(buf, "[[1,2.5][-3,4]]") == 0, "format small matrix");

    Array big;
    const char* err = nullptr;
    check(matops::identity(10, big, &err), "identity(10)");
    char small[24];
    matexpr::format_matrix(big, small, sizeof(small));
    check(std::strstr(small, "...") != nullptr, "format truncates");
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
    test_format_matrix();
    test_store();

    std::printf("test_matrix: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
