// list<N>.dat (N = 1..6, one file per list, perf fix 2026-07-22): fixed
// header (magic + dtype tag + count) followed by that list's raw
// elements. Splitting the old single concatenated lists.dat into one
// file per list means save() only ever touches the one list an edit
// actually changed, instead of re-writing all six lists' full contents
// on every commit — the actual cost of a "large lists feel sluggish to
// enter values into" report traced to `ListStore::save()` (not a
// rendering path). Old lists.dat images are simply never read under
// this scheme and are left on the card, same as the project's existing
// "old files ignored, not deleted" precedent for prior format bumps.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"
#include "math/lists.hpp"

namespace math {

namespace {

// Bump on layout change ("PCL3", ...): old images then fail to load
// and that list starts empty.
constexpr char kMagic[4] = {'P', 'C', 'L', '2'};

struct Header {
    char magic[4];
    uint8_t dtype;
    uint8_t reserved[3];
    uint32_t count;
};

constexpr int kChunkElements = 256;
calc_t g_chunk[kChunkElements];

void path_for(int index, char* buf, size_t cap) {
    std::snprintf(buf, cap, "/picocalc/list%d.dat", index + 1);
}

bool save_list(platform::Storage& storage, const Array& lst, int index) {
    if (!storage.mounted()) {
        return false;
    }
    char path[24];
    path_for(index, path, sizeof(path));
    Header h = {};
    std::memcpy(h.magic, kMagic, sizeof(kMagic));
    h.dtype = static_cast<uint8_t>(lst.dtype());
    h.count = static_cast<uint32_t>(lst.size());
    if (!storage.write_file(path, reinterpret_cast<const uint8_t*>(&h), sizeof(h))) {
        return false;
    }
    const int n = lst.size();
    for (int at = 0; at < n; at += kChunkElements) {
        const int m = n - at < kChunkElements ? n - at : kChunkElements;
        lst.read_range(at, m, g_chunk);
        if (!storage.append_file(path, reinterpret_cast<const uint8_t*>(g_chunk),
                                 static_cast<size_t>(m) * sizeof(calc_t))) {
            return false;
        }
    }
    return true;
}

// Mirrors the old load()'s per-list contract: true if this list is done
// (loaded, or intentionally left empty because there's nothing/nothing
// readable saved), false only when it needs the PSRAM tier and PSRAM
// isn't up yet (D14) — the caller retries in that case.
bool load_list(platform::Storage& storage, Array& lst, int index) {
    if (!storage.mounted()) {
        return false;
    }
    char path[24];
    path_for(index, path, sizeof(path));
    if (!storage.file_exists(path)) {
        return true;  // Nothing saved yet — loaded state is "empty"
    }
    Header h = {};
    if (storage.read_file_range(path, 0, reinterpret_cast<uint8_t*>(&h), sizeof(h)) !=
            static_cast<int>(sizeof(h)) ||
        std::memcmp(h.magic, kMagic, sizeof(kMagic)) != 0) {
        return true;  // Corrupt/old image: ignore it, keep this list empty
    }
    if (h.dtype != static_cast<uint8_t>(Dtype::kDouble) ||
        h.count > static_cast<uint32_t>(Array::kMaxElements)) {
        return true;  // Unknown dtype / bad count: treat as corrupt
    }
    constexpr size_t kSlabElems = ArrayStore::kSlabBytes / sizeof(calc_t);
    if (h.count > kSlabElems && !psram_backend::available()) {
        return false;  // Needs PSRAM, not up yet — let late-init retry
    }
    const int n = static_cast<int>(h.count);
    if (!lst.resize(n)) {
        return false;
    }
    size_t off = sizeof(h);
    for (int at = 0; at < n; at += kChunkElements) {
        const int m = n - at < kChunkElements ? n - at : kChunkElements;
        const int bytes = m * static_cast<int>(sizeof(calc_t));
        if (storage.read_file_range(path, off, reinterpret_cast<uint8_t*>(g_chunk),
                                    static_cast<size_t>(bytes)) != bytes) {
            lst.resize(at);  // Keep what arrived; truncate the rest
            return true;
        }
        lst.write_range(at, m, g_chunk);
        off += static_cast<size_t>(bytes);
    }
    return true;
}

}  // namespace

bool ListStore::save(platform::Storage& storage, int index) const {
    return save_list(storage, lists_[index], index);
}

bool ListStore::load(platform::Storage& storage) {
    bool all_done = true;
    for (int i = 0; i < kCount; ++i) {
        if (loaded_[i]) {
            continue;  // Already loaded — don't clobber an in-session edit
        }
        if (load_list(storage, lists_[i], i)) {
            loaded_[i] = true;
        } else {
            all_done = false;
        }
    }
    return all_done;
}

}  // namespace math
