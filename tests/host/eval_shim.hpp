#pragma once

#include <cstdint>

#include "math/array.hpp"
#include "math/complex.hpp"
#include "math/engine.hpp"
#include "math/list_ops.hpp"
#include "math/matrix.hpp"
#include "math/named_lists.hpp"
#include "math/unified_eval.hpp"
#include "math/unified_home.hpp"

// Adapter from the unified evaluator to the result shape the retired
// evaluators' suites were written against (Phase 5.2, task 5.2.11).
//
// **This exists so 5.2.11 keeps those checks instead of deleting them.**
// `test_lists`, `test_matrix` and `test_complex_expr` carry ~770 checks that
// phase5.2-spec.md §6 calls "the specification", written against
// `listexpr::Result`, `matexpr::Result` and `complexexpr::Result`. Deleting the
// evaluators would have taken the checks with them, and re-pinning the same
// behaviours a third time by hand is exactly the kind of churn that loses
// coverage quietly.
//
// So the *shape* is preserved and the *evaluator* underneath is replaced. Each
// suite's one-line `eval_*` wrapper now calls this; every assertion in the
// bodies is untouched. Where a suite's expectation genuinely changed, the
// change is one of the signed-off rows in
// docs/notes/unified-evaluator-changes.md and the assertion says which.
//
// The three retired Results differed slightly from one another (matexpr had
// kMatrix and kText, listexpr had names_modified, complexexpr was a bare
// EvalResult). This is their union, which is fair: they are all one evaluator's
// output now, and the union is what that evaluator can return.
namespace shim {

// Storage for an array result, held across the commit (see eval()).
inline math::Array& held() {
    static math::Array instance;
    return instance;
}

enum class Kind : uint8_t { kNone, kScalar, kList, kMatrix, kText, kError };

struct Scalar {
    bool ok = false;
    double value = 0;
    int stored_var = -1;
    const char* error = nullptr;
};

struct Result {
    Kind kind = Kind::kNone;
    const char* error = nullptr;
    const math::Array* list = nullptr;
    const math::Array* matrix = nullptr;
    const char* text = nullptr;
    Scalar scalar;
    bool scalar_complex = false;
    math::Complex cvalue;
    int stored_list = -1;
    int stored_matrix = -1;
    bool lists_modified = false;
    bool matrices_modified = false;
    bool names_modified = false;
    uint32_t lists_mask = 0;
};

// Evaluate and commit, exactly as the home screen does.
inline Result eval(const char* input) {
    Result r;
    // The typed value comes from a PROBE, and it has to be taken first: a
    // commit writes Ans, so probing afterwards would evaluate `ans*ans` against
    // the answer it had just stored. (Found twice — the differential harness
    // had the same trap. kProbe is side-effect-free, not order-free.)
    math::unified::Program p;
    math::unified::Value v;
    const char* perr = nullptr;
    const bool probed = math::unified::compile(input, p, &perr) &&
                        math::unified::run(p, &v, &perr, math::unified::Mode::kProbe);
    const math::unified::Kind vkind = v.kind;
    math::Complex vscalar;
    if (probed && v.is_scalar()) {
        vscalar = v.as_complex();
    } else if (probed) {
        // An array Value names an evaluator temporary, and the commit below
        // clears the pool. Copy it out first — the suites read the result
        // through this pointer long after eval() returns.
        if (v.kind == math::unified::Kind::kList) {
            math::listops::copy(*v.a, held());  // 1-D: matops::copy would reshape
        } else {
            math::matops::copy(*v.a, held());
        }
    }

    const math::unified::HomeResult h = math::unified::evaluate_home(input, /*to_frac=*/false);
    if (h.kind == math::unified::HomeKind::kError) {
        r.kind = Kind::kError;
        r.error = h.error;
        r.scalar.error = h.error;
        return r;
    }
    const math::unified::Commit& c = h.commit;
    if (h.kind == math::unified::HomeKind::kText) {
        r.kind = Kind::kText;
        r.text = h.text;
        r.lists_mask = c.lists_mask;  // "Done (n lists)" wrote them
        r.lists_modified = c.lists_mask != 0;
        return r;
    }

    r.stored_list = c.list;
    r.stored_matrix = c.matrix;
    r.lists_mask = c.lists_mask;
    r.lists_modified = c.lists_mask != 0;
    r.matrices_modified = c.matrix >= 0;
    r.names_modified = c.names_modified;

    if (!probed) {
        r.kind = Kind::kError;
        r.error = perr;
        return r;
    }
    switch (vkind) {
        case math::unified::Kind::kList:
            r.kind = Kind::kList;
            r.list = &held();
            return r;
        case math::unified::Kind::kMatrix:
            r.kind = Kind::kMatrix;
            r.matrix = &held();
            return r;
        default:
            r.kind = Kind::kScalar;
            r.scalar.ok = true;
            r.scalar.value = vscalar.re;
            r.scalar.stored_var = c.var;
            r.scalar_complex = !vscalar.is_real();
            r.cvalue = vscalar;
            return r;
    }
}

}  // namespace shim
