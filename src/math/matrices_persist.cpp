// matrix<N>.dat (N = 1..10, one file per matrix, perf fix 2026-07-22):
// fixed header (magic + dtype tag + shape) followed by that matrix's
// raw elements row-major. Mirrors lists_persist.cpp's fix — splitting
// the old single concatenated matrices.dat into one file per matrix
// means save() only ever touches the one matrix an edit actually
// changed, instead of re-writing all ten matrices' full contents on
// every cell commit. Old matrices.dat images are simply never read
// under this scheme and are left on the card, same as the project's
// existing "old files ignored, not deleted" precedent for prior format
// bumps.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"
#include "math/matrix.hpp"

namespace math {

namespace {

// Bump on layout change ("PCM3", ...): old images then fail to load
// and that matrix starts empty.
constexpr char kMagic[4] = {'P', 'C', 'M', '2'};

struct Header {
    char magic[4];
    uint8_t dtype;
    uint8_t reserved;
    uint16_t rows;
    uint16_t cols;
};

constexpr int kChunkElements = 256;
calc_t g_chunk[kChunkElements];
// Complex chunk (4D.25) aliases the same buffer budget: 128 complex
// elements per pass through the 2 KB chunk.
constexpr int kChunkComplex = kChunkElements / 2;

void path_for(int index, char* buf, size_t cap) {
    std::snprintf(buf, cap, "/picocalc/matrix%d.dat", index + 1);
}

bool save_matrix(platform::Storage& storage, const Array& m, int index) {
    if (!storage.mounted()) {
        return false;
    }
    char path[24];
    path_for(index, path, sizeof(path));
    Header h = {};
    std::memcpy(h.magic, kMagic, sizeof(kMagic));
    h.dtype = static_cast<uint8_t>(m.dtype());
    h.rows = static_cast<uint16_t>(m.dim(0));
    h.cols = static_cast<uint16_t>(m.dim(1));
    if (!storage.write_file(path, reinterpret_cast<const uint8_t*>(&h), sizeof(h))) {
        return false;
    }
    const int n = m.size();
    if (m.dtype() == Dtype::kComplex) {
        auto* cbuf = reinterpret_cast<Complex*>(g_chunk);
        for (int at = 0; at < n; at += kChunkComplex) {
            const int cnt = n - at < kChunkComplex ? n - at : kChunkComplex;
            m.read_range_c(at, cnt, cbuf);
            if (!storage.append_file(path, reinterpret_cast<const uint8_t*>(cbuf),
                                     static_cast<size_t>(cnt) * sizeof(Complex))) {
                return false;
            }
        }
        return true;
    }
    for (int at = 0; at < n; at += kChunkElements) {
        const int cnt = n - at < kChunkElements ? n - at : kChunkElements;
        m.read_range(at, cnt, g_chunk);
        if (!storage.append_file(path, reinterpret_cast<const uint8_t*>(g_chunk),
                                 static_cast<size_t>(cnt) * sizeof(calc_t))) {
            return false;
        }
    }
    return true;
}

// Mirrors lists_persist.cpp's load_list() contract: true if this
// matrix is done (loaded, or intentionally left empty), false only
// when it needs the PSRAM tier and PSRAM isn't up yet (D14).
bool load_matrix(platform::Storage& storage, Array& m, int index) {
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
        return true;  // Corrupt/old image: ignore it, keep this matrix empty
    }
    const int n = static_cast<int>(h.rows) * static_cast<int>(h.cols);
    const bool complex = h.dtype == static_cast<uint8_t>(Dtype::kComplex);
    if ((h.dtype != static_cast<uint8_t>(Dtype::kDouble) && !complex) ||
        n > (complex ? Array::kMaxComplexElements : Array::kMaxElements)) {
        return true;  // Unknown dtype / bad shape: treat as corrupt
    }
    constexpr size_t kSlabElems = ArrayStore::kSlabBytes / sizeof(calc_t);
    if ((complex || static_cast<size_t>(n) > kSlabElems) && !psram_backend::available()) {
        return false;  // Needs PSRAM, not up yet — let late-init retry
    }
    if (n == 0) {
        m.clear();
        return true;
    }
    m.clear();
    if (!m.set_dtype(complex ? Dtype::kComplex : Dtype::kDouble) || !m.resize(h.rows, h.cols)) {
        return false;
    }
    size_t off = sizeof(h);
    if (complex) {
        auto* cbuf = reinterpret_cast<Complex*>(g_chunk);
        for (int at = 0; at < n; at += kChunkComplex) {
            const int cnt = n - at < kChunkComplex ? n - at : kChunkComplex;
            const int bytes = cnt * static_cast<int>(sizeof(Complex));
            if (storage.read_file_range(path, off, reinterpret_cast<uint8_t*>(cbuf),
                                        static_cast<size_t>(bytes)) != bytes) {
                m.clear();  // Truncated image: drop this one
                return true;
            }
            m.write_range_c(at, cnt, cbuf);
            off += static_cast<size_t>(bytes);
        }
        return true;
    }
    for (int at = 0; at < n; at += kChunkElements) {
        const int cnt = n - at < kChunkElements ? n - at : kChunkElements;
        const int bytes = cnt * static_cast<int>(sizeof(calc_t));
        if (storage.read_file_range(path, off, reinterpret_cast<uint8_t*>(g_chunk),
                                    static_cast<size_t>(bytes)) != bytes) {
            m.clear();  // Truncated image: drop this one
            return true;
        }
        m.write_range(at, cnt, g_chunk);
        off += static_cast<size_t>(bytes);
    }
    return true;
}

}  // namespace

bool MatrixStore::save(platform::Storage& storage, int index) const {
    return save_matrix(storage, matrices_[index], index);
}

bool MatrixStore::load(platform::Storage& storage) {
    bool all_done = true;
    for (int i = 0; i < kCount; ++i) {
        if (loaded_[i]) {
            continue;  // Already loaded — don't clobber an in-session edit
        }
        if (load_matrix(storage, matrices_[i], i)) {
            loaded_[i] = true;
        } else {
            all_done = false;
        }
    }
    return all_done;
}

}  // namespace math
