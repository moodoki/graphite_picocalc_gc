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

    // Persistence: one file per list, /picocalc/list1.dat..list6.dat
    // (perf fix, 2026-07-22 — a single concatenated lists.dat made every
    // save touch all six lists' full contents, even a one-element edit
    // to just one of them). save(index) persists only that list — every
    // caller only ever mutates one list per operation (a single ->lk
    // store target), so this is always the right granularity; there is
    // no "save everything" entry point because nothing needs one.
    // load() reads all six and is all-or-nothing per list: when a list
    // needs the PSRAM tier before PSRAM is up (cold boot, D14) it loads
    // nothing for that list and returns false so the late-init loop
    // retries.
    bool save(platform::Storage& storage, int index) const;
    bool load(platform::Storage& storage);

private:
    Array lists_[kCount];
    // Per-list load latch so a late-init retry (waiting on PSRAM for one
    // big list, say) never re-reads a list that already loaded — that
    // would clobber an in-session edit made while the retry was pending.
    bool loaded_[kCount] = {};
};

// Singleton accessor (project convention).
ListStore& lists();

}  // namespace math
