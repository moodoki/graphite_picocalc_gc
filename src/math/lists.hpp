#pragma once

#include "math/array.hpp"

namespace platform {
class Storage;
}

namespace math {

// The six named data lists l1..l6 (spec §2.3; TI's L1..L6, lowercase
// per D19). Pure state — persistence lives in lists_persist.cpp
// (firmware-only TU, like graph_persist).
class ListStore {
public:
    static constexpr int kCount = 6;

    Array& list(int index) { return lists_[index]; }
    const Array& list(int index) const { return lists_[index]; }

    // Persistence to /picocalc/lists.dat. load() is all-or-nothing:
    // when the file needs the PSRAM tier before PSRAM is up (cold
    // boot, D14) it loads nothing and returns false so the late-init
    // loop retries.
    bool save(platform::Storage& storage) const;
    bool load(platform::Storage& storage);

private:
    Array lists_[kCount];
};

// Singleton accessor (project convention).
ListStore& lists();

}  // namespace math
