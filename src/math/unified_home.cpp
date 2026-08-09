#include "math/unified_home.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/array.hpp"
#include "math/array_format.hpp"
#include "math/complex.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/frac.hpp"
#include "math/named_lists.hpp"

// Home-screen dispatch for the unified evaluator (task 5.2.10). See the header
// for why this is in math/ and not in the screen.
namespace math::unified {

namespace {

// One program and one output buffer, file scope for the same non-reentrancy
// argument the rest of the evaluator rests on: one HomeScreen, one Enter at a
// time. The Program is 2 KB and would otherwise sit on the caller's frame,
// which is exactly the shape D47 spent a session removing.
Program g_prog;
char g_text[192];

// The three formatters moved to array_format.hpp in 5.2.11 — display code
// outliving the evaluators it happened to live in, the same treatment MatAns
// got. There is still exactly one implementation of each.
void format_value(const Value& v, bool to_frac, char* buf, size_t cap) {
    switch (v.kind) {
        case Kind::kList:
            format_list(*v.a, buf, cap);
            return;
        case Kind::kMatrix:
            if (to_frac) {
                format_matrix_frac(*v.a, buf, cap);
            } else {
                format_matrix(*v.a, buf, cap);
            }
            return;
        case Kind::kComplex:
            format_complex(v.c, number_mode(), buf, cap);
            return;
        default:
            // >Frac on a scalar (4D.2): p/q when a tight fraction exists,
            // else the ordinary decimal — home_screen.cpp:594's rule.
            if (to_frac && frac::format_fraction(v.r, 10000, buf, cap)) {
                return;
            }
            format_number(v.r, buf, cap);
            return;
    }
}

// "a" / "theta" / "l1" / "costs" / "[C]". The glyph is the caller's.
void store_label(const Commit& c, char* buf, size_t cap) {
    buf[0] = 0;
    if (c.var >= 0) {
        if (c.var == Variables::kTheta) {
            std::snprintf(buf, cap, "theta");
        } else {
            std::snprintf(buf, cap, "%c", static_cast<char>('a' + c.var));
        }
        return;
    }
    if (c.matrix >= 0) {
        std::snprintf(buf, cap, "[%c]", static_cast<char>('A' + c.matrix));
        return;
    }
    if (c.list >= 0) {
        char name[NamedLists::kMaxName + 2] = {};
        list_ref_name(c.list, name, static_cast<int>(sizeof(name)));
        std::snprintf(buf, cap, "%s", name);
    }
}

int popcount(uint32_t v) {
    int n = 0;
    for (; v != 0; v >>= 1) {
        n += static_cast<int>(v & 1U);
    }
    return n;
}

}  // namespace

HomeResult evaluate_home(const char* expr, bool to_frac) {
    HomeResult res;
    const char* err = nullptr;
    if (!compile(expr, g_prog, &err)) {
        res.kind = HomeKind::kError;
        res.error = err != nullptr ? err : "Syntax error";
        return res;
    }
    Value v;
    if (!run(g_prog, &v, &err, Mode::kCommit, &res.commit)) {
        res.kind = HomeKind::kError;
        res.error = err != nullptr ? err : "Syntax error";
        return res;
    }

    // mat2list is the one call that writes lists and yields a scalar, so
    // `real result + a non-empty lists_mask` identifies it uniquely: an
    // in-place sort and a list store both yield lists. That is the whole
    // reconstruction 5.2.8 promised when it left "Done (n lists)" to this
    // layer, and it needs no special-case plumbed through the evaluator.
    if (v.kind == Kind::kReal && res.commit.lists_mask != 0) {
        const int n = popcount(res.commit.lists_mask);
        std::snprintf(g_text, sizeof(g_text), "Done (%d list%s)", n, n == 1 ? "" : "s");
        res.kind = HomeKind::kText;
        res.text = g_text;
        return res;
    }

    format_value(v, to_frac, g_text, sizeof(g_text));
    res.text = g_text;
    store_label(res.commit, res.store_label, sizeof(res.store_label));

    switch (v.kind) {
        case Kind::kList:
            res.kind = HomeKind::kList;
            break;
        case Kind::kMatrix:
            res.kind = HomeKind::kMatrix;
            break;
        default:
            res.kind = HomeKind::kScalar;
            res.scalar_value = v.kind == Kind::kReal ? v.r : v.c.re;
            // The exact-form probe runs only over a real, finite, unstored
            // result — home_screen.cpp:101's three conditions, moved here so
            // the screen asks one question instead of re-deriving them.
            res.exact_form_ok = v.kind == Kind::kReal && std::isfinite(v.r) && res.commit.var < 0;
            break;
    }
    return res;
}

bool evaluate_scalar(const char* expr, Complex* out, const char** err) {
    if (!compile(expr, g_prog, err)) {
        return false;
    }
    Value v;
    if (!run(g_prog, &v, err, Mode::kProbe)) {
        return false;
    }
    if (!v.is_scalar()) {
        if (err != nullptr) {
            *err = "Expected a number";
        }
        return false;
    }
    *out = v.as_complex();
    return true;
}

}  // namespace math::unified
