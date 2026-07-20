// matrices.dat: fixed header (magic + per-matrix dtype tag and shape),
// followed by each matrix's raw elements row-major. Mirrors
// lists_persist.cpp — the dtype tag is in the format from day one
// (D21) so complex-valued matrices later change the tag, not the
// layout. Data streams in slab-sized chunks.

#include <cstdint>
#include <cstring>

#include "platform/storage.hpp"
#include "math/matrix.hpp"

namespace math {

namespace {

constexpr const char* kPath = "/picocalc/matrices.dat";

// Bump on layout change ("PCM2", ...): old images then fail to load
// and the matrices start empty.
constexpr char kMagic[4] = {'P', 'C', 'M', '1'};

struct Header {
    char magic[4];
    uint8_t dtype[MatrixStore::kCount];
    uint8_t reserved[2];
    uint16_t rows[MatrixStore::kCount];
    uint16_t cols[MatrixStore::kCount];
};

constexpr int kChunkElements = 256;
calc_t g_chunk[kChunkElements];

}  // namespace

bool MatrixStore::save(platform::Storage& storage) const {
    if (!storage.mounted()) {
        return false;
    }
    Header h = {};
    std::memcpy(h.magic, kMagic, sizeof(kMagic));
    for (int i = 0; i < kCount; ++i) {
        h.dtype[i] = static_cast<uint8_t>(matrices_[i].dtype());
        h.rows[i] = static_cast<uint16_t>(matrices_[i].dim(0));
        h.cols[i] = static_cast<uint16_t>(matrices_[i].dim(1));
    }
    if (!storage.write_file(kPath, reinterpret_cast<const uint8_t*>(&h), sizeof(h))) {
        return false;
    }
    for (const Array& m : matrices_) {
        const int n = m.size();
        for (int at = 0; at < n; at += kChunkElements) {
            const int cnt = n - at < kChunkElements ? n - at : kChunkElements;
            m.read_range(at, cnt, g_chunk);
            if (!storage.append_file(kPath, reinterpret_cast<const uint8_t*>(g_chunk),
                                     static_cast<size_t>(cnt) * sizeof(calc_t))) {
                return false;
            }
        }
    }
    return true;
}

bool MatrixStore::load(platform::Storage& storage) {
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
        return true;  // Corrupt/old image: ignore it, keep matrices empty
    }
    constexpr size_t kSlabElems = ArrayStore::kSlabBytes / sizeof(calc_t);
    bool needs_psram = false;
    for (int i = 0; i < kCount; ++i) {
        const int n = static_cast<int>(h.rows[i]) * static_cast<int>(h.cols[i]);
        if (h.dtype[i] != static_cast<uint8_t>(Dtype::kDouble) || n > Array::kMaxElements) {
            return true;  // Unknown dtype / bad shape: treat as corrupt
        }
        needs_psram = needs_psram || static_cast<size_t>(n) > kSlabElems;
    }
    // All-or-nothing: SD can be up while PSRAM is still settling on a
    // cold boot (D14). Load nothing and let late-init retry.
    if (needs_psram && !psram_backend::available()) {
        return false;
    }
    size_t off = sizeof(h);
    for (int i = 0; i < kCount; ++i) {
        const int rows = h.rows[i];
        const int cols = h.cols[i];
        const int n = rows * cols;
        if (n == 0) {
            matrices_[i].clear();
            continue;
        }
        if (!matrices_[i].resize(rows, cols)) {
            return false;
        }
        for (int at = 0; at < n; at += kChunkElements) {
            const int cnt = n - at < kChunkElements ? n - at : kChunkElements;
            const int bytes = cnt * static_cast<int>(sizeof(calc_t));
            if (storage.read_file_range(kPath, off, reinterpret_cast<uint8_t*>(g_chunk),
                                        static_cast<size_t>(bytes)) != bytes) {
                matrices_[i].clear();  // Truncated image: drop this one
                return true;
            }
            matrices_[i].write_range(at, cnt, g_chunk);
            off += static_cast<size_t>(bytes);
        }
    }
    return true;
}

}  // namespace math
