// lists.dat: fixed header (magic + per-list dtype tag and count),
// followed by each list's raw elements in order. The dtype tag is in
// the format from day one (D21) so complex-valued lists later change
// the tag, not the layout. Data is streamed in slab-sized chunks —
// a full 10000-element list (80 KB) never fits an SRAM file buffer.

#include <cstdint>
#include <cstring>

#include "platform/storage.hpp"
#include "math/lists.hpp"

namespace math {

namespace {

constexpr const char* kPath = "/picocalc/lists.dat";

// Bump on layout change ("PCL2", ...): old images then fail to load
// and the lists start empty.
constexpr char kMagic[4] = {'P', 'C', 'L', '1'};

struct Header {
    char magic[4];
    uint8_t dtype[ListStore::kCount];
    uint8_t reserved[2];
    uint32_t count[ListStore::kCount];
};

constexpr int kChunkElements = 256;
calc_t g_chunk[kChunkElements];

}  // namespace

bool ListStore::save(platform::Storage& storage) const {
    if (!storage.mounted()) {
        return false;
    }
    Header h = {};
    std::memcpy(h.magic, kMagic, sizeof(kMagic));
    for (int i = 0; i < kCount; ++i) {
        h.dtype[i] = static_cast<uint8_t>(lists_[i].dtype());
        h.count[i] = static_cast<uint32_t>(lists_[i].size());
    }
    if (!storage.write_file(kPath, reinterpret_cast<const uint8_t*>(&h), sizeof(h))) {
        return false;
    }
    for (const Array& lst : lists_) {
        const int n = lst.size();
        for (int at = 0; at < n; at += kChunkElements) {
            const int m = n - at < kChunkElements ? n - at : kChunkElements;
            lst.read_range(at, m, g_chunk);
            if (!storage.append_file(kPath, reinterpret_cast<const uint8_t*>(g_chunk),
                                     static_cast<size_t>(m) * sizeof(calc_t))) {
                return false;
            }
        }
    }
    return true;
}

bool ListStore::load(platform::Storage& storage) {
    if (!storage.mounted()) {
        return false;
    }
    if (!storage.file_exists(kPath)) {
        return true;  // Nothing saved yet — loaded state is "all empty"
    }
    Header h = {};
    if (storage.read_file_range(kPath, 0, reinterpret_cast<uint8_t*>(&h), sizeof(h)) !=
            static_cast<int>(sizeof(h)) ||
        std::memcmp(h.magic, kMagic, sizeof(kMagic)) != 0) {
        return true;  // Corrupt/old image: ignore it, keep lists empty
    }
    constexpr size_t kSlabElems = ArrayStore::kSlabBytes / sizeof(calc_t);
    bool needs_psram = false;
    for (int i = 0; i < kCount; ++i) {
        if (h.dtype[i] != static_cast<uint8_t>(Dtype::kDouble) ||
            h.count[i] > static_cast<uint32_t>(Array::kMaxElements)) {
            return true;  // Unknown dtype / bad count: treat as corrupt
        }
        needs_psram = needs_psram || h.count[i] > kSlabElems;
    }
    // All-or-nothing: a cold boot can reach here with SD up but PSRAM
    // still settling (D14). Load nothing and let late-init retry.
    if (needs_psram && !psram_backend::available()) {
        return false;
    }
    size_t off = sizeof(h);
    for (int i = 0; i < kCount; ++i) {
        const int n = static_cast<int>(h.count[i]);
        if (!lists_[i].resize(n)) {
            return false;
        }
        for (int at = 0; at < n; at += kChunkElements) {
            const int m = n - at < kChunkElements ? n - at : kChunkElements;
            const int bytes = m * static_cast<int>(sizeof(calc_t));
            if (storage.read_file_range(kPath, off, reinterpret_cast<uint8_t*>(g_chunk),
                                        static_cast<size_t>(bytes)) != bytes) {
                lists_[i].resize(at);  // Keep what arrived; truncate the rest
                return true;
            }
            lists_[i].write_range(at, m, g_chunk);
            off += static_cast<size_t>(bytes);
        }
    }
    return true;
}

}  // namespace math
