#pragma once

namespace math {

// One row per parser-callable function (task 2.26). Drives both engine
// registration (build_lookup) and the help catalog — one source of
// truth, so help cannot drift from the parser.
struct FnDescriptor {
    const char* name;       // "ncr"
    const char* signature;  // "ncr(n, r)" — must itself be parseable
    const char* summary;    // "Combinations: n choose r"
    const void* fn;         // Binding for build_lookup; nullptr = help-only
                            // (list functions live in list_expr, not tinyexpr)
    int arity;              // 0..4 (TE_FUNCTION0 + arity; te caps at 7)
};

// Registration headroom for build_lookup's fixed-size table.
constexpr int kMaxCatalogEntries = 72;

// The full catalog, in display order.
const FnDescriptor* catalog(int* count);

// Scientific constants (4D.17): engine-bound as plain identifiers (no
// parens — TE_VARIABLE entries over the descriptors' own storage), and
// browsable/insertable via the home-screen `const` picker. Multi-char
// names so they can't shadow the a-z user variables.
struct ConstDescriptor {
    const char* name;     // Engine identifier ("clight")
    const char* symbol;   // Conventional symbol ("c")
    const char* summary;  // "Speed of light (m/s)"
    double value;
};

constexpr int kMaxConstants = 24;

const ConstDescriptor* constants(int* count);

}  // namespace math
