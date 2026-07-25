#pragma once

#include "math/array.hpp"
#include "math/lists.hpp"

namespace platform {
class Storage;
}

namespace math {

// Named user lists (4D.13, D38/P4-10 full integration): up to kMax
// lists with user-chosen names (lowercase letter-first, 2..kMaxName
// chars), usable everywhere an l1-l6 token works. l1-l6 stay fixed;
// a named entry is just an Array (complex dtype rides the 4D.24 tier).
//
// Registry slots are stable across renames; remove() frees the slot
// for reuse. Persistence (named_lists_persist.cpp, firmware-only TU):
// one data file per slot (/picocalc/nlist<idx>.dat, PCL2-shaped) plus
// a name-directory index (/picocalc/listdir.dat, magic PCN1).
class NamedLists {
public:
    static constexpr int kMax = 20;
    static constexpr int kMaxName = 5;

    // Slot of `name`, or -1. Exact match (engine-style case-sensitive
    // lowercase names).
    int find(const char* name) const;
    // Claim a free slot for `name`. -1 when invalid/duplicate/full.
    int create(const char* name);
    bool remove(int idx);  // Clears the array, frees the slot
    bool rename(int idx, const char* new_name);

    bool used(int idx) const { return idx >= 0 && idx < kMax && used_[idx]; }
    const char* name(int idx) const { return used(idx) ? names_[idx] : ""; }
    int count() const;
    // The n-th used slot in registry order (editor columns), or -1.
    int nth(int n) const;

    Array& list(int idx) { return lists_[idx]; }
    const Array& list(int idx) const { return lists_[idx]; }

    // Name validity: 2..kMaxName chars, lowercase letter first,
    // [a-z0-9] after, not an l<digit> form, and not colliding with an
    // engine identifier (catalog functions, constants, reserved words).
    static bool valid_name(const char* name);

    // Persistence (see named_lists_persist.cpp).
    bool save_index(platform::Storage& storage) const;
    bool save(platform::Storage& storage, int idx) const;
    bool load(platform::Storage& storage);  // False = PSRAM retry needed
    void remove_file(platform::Storage& storage, int idx) const;

private:
    char names_[kMax][kMaxName + 1] = {};
    Array lists_[kMax];
    bool used_[kMax] = {};
    bool loaded_[kMax] = {};
    bool index_loaded_ = false;
};

// Singleton accessor (project convention).
NamedLists& named_lists();

// Unified list addressing (4D.13): refs 0..5 are l1..l6, kNamedRefBase
// + k is named-registry slot k.
constexpr int kNamedRefBase = 6;

Array& list_by_ref(int ref);
// Display name for a ref ("l1", or the registry name).
void list_ref_name(int ref, char* buf, int cap);

}  // namespace math
