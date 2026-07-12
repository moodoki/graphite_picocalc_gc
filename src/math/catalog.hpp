#pragma once

namespace math {

// One row per parser-callable function (task 2.26). Drives both engine
// registration (build_lookup) and the help catalog — one source of
// truth, so help cannot drift from the parser.
struct FnDescriptor {
    const char* name;       // "ncr"
    const char* signature;  // "ncr(n, r)" — must itself be parseable
    const char* summary;    // "Combinations: n choose r"
    const void* fn;         // Binding for build_lookup
    int arity;              // 0..2 (TE_FUNCTION0 + arity)
};

// Registration headroom for build_lookup's fixed-size table.
constexpr int kMaxCatalogEntries = 32;

// The full catalog, in display order.
const FnDescriptor* catalog(int* count);

}  // namespace math
