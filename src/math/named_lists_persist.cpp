// Named-list persistence (4D.13): a name-directory index
// (/picocalc/listdir.dat, magic PCN1) plus one data file per registry
// slot (/picocalc/nlist<idx>.dat) in the same shape lists_persist.cpp
// writes (magic + dtype + count header, raw elements). Files are keyed
// by slot index, not name, so a rename only rewrites the small index.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "platform/io_scratch.hpp"
#include "platform/storage.hpp"
#include "math/named_lists.hpp"

namespace math {

namespace {

constexpr const char* kIndexPath = "/picocalc/listdir.dat";
constexpr char kIndexMagic[4] = {'P', 'C', 'N', '1'};

struct IndexImage {
    char magic[4];
    uint8_t used[NamedLists::kMax];
    char names[NamedLists::kMax][NamedLists::kMaxName + 1];
};

constexpr char kListMagic[4] = {'P', 'C', 'L', '2'};

struct Header {
    char magic[4];
    uint8_t dtype;
    uint8_t reserved[3];
    uint32_t count;
};

constexpr int kChunkElements = 256;
// D70 lever A: view over the shared one-shot I/O region. Safe
// because only one store is ever saving or loading at a time.
static_assert(kChunkElements * sizeof(calc_t) <= platform::kIoScratchBytes,
              "named-list persistence chunk must fit the shared I/O scratch region");
calc_t* g_chunk = reinterpret_cast<calc_t*>(platform::io_scratch());
constexpr int kChunkComplex = kChunkElements / 2;

void path_for(int idx, char* buf, size_t cap) {
    std::snprintf(buf, cap, "/picocalc/nlist%d.dat", idx);
}

}  // namespace

bool NamedLists::save_index(platform::Storage& storage) const {
    if (!storage.mounted()) {
        return false;
    }
    IndexImage img = {};
    std::memcpy(img.magic, kIndexMagic, sizeof(kIndexMagic));
    for (int i = 0; i < kMax; ++i) {
        img.used[i] = used_[i] ? 1 : 0;
        std::snprintf(img.names[i], sizeof(img.names[i]), "%s", names_[i]);
    }
    return storage.write_file(kIndexPath, reinterpret_cast<const uint8_t*>(&img), sizeof(img));
}

bool NamedLists::save(platform::Storage& storage, int idx) const {
    if (!storage.mounted() || !used(idx)) {
        return false;
    }
    char path[32];
    path_for(idx, path, sizeof(path));
    const Array& lst = lists_[idx];
    Header h = {};
    std::memcpy(h.magic, kListMagic, sizeof(kListMagic));
    h.dtype = static_cast<uint8_t>(lst.dtype());
    h.count = static_cast<uint32_t>(lst.size());
    if (!storage.write_file(path, reinterpret_cast<const uint8_t*>(&h), sizeof(h))) {
        return false;
    }
    const int n = lst.size();
    if (lst.dtype() == Dtype::kComplex) {
        auto* cbuf = reinterpret_cast<Complex*>(g_chunk);
        for (int at = 0; at < n; at += kChunkComplex) {
            const int m = n - at < kChunkComplex ? n - at : kChunkComplex;
            lst.read_range_c(at, m, cbuf);
            if (!storage.append_file(path, reinterpret_cast<const uint8_t*>(cbuf),
                                     static_cast<size_t>(m) * sizeof(Complex))) {
                return false;
            }
        }
        return true;
    }
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

void NamedLists::remove_file(platform::Storage& storage, int idx) const {
    if (!storage.mounted()) {
        return;
    }
    char path[32];
    path_for(idx, path, sizeof(path));
    if (storage.file_exists(path)) {
        storage.delete_file(path);
    }
}

bool NamedLists::load(platform::Storage& storage) {
    if (!storage.mounted()) {
        return false;
    }
    if (!index_loaded_) {
        IndexImage img = {};
        const int n = storage.read_file(kIndexPath, reinterpret_cast<uint8_t*>(&img), sizeof(img));
        if (n == static_cast<int>(sizeof(img)) &&
            std::memcmp(img.magic, kIndexMagic, sizeof(kIndexMagic)) == 0) {
            for (int i = 0; i < kMax; ++i) {
                used_[i] = img.used[i] != 0;
                std::snprintf(names_[i], sizeof(names_[i]), "%s", img.names[i]);
                if (used_[i] && !valid_name(names_[i])) {
                    used_[i] = false;  // Corrupt entry: drop it
                    names_[i][0] = 0;
                }
            }
        }
        index_loaded_ = true;  // Missing/corrupt index = empty registry
    }
    bool all_done = true;
    for (int i = 0; i < kMax; ++i) {
        if (!used_[i] || loaded_[i]) {
            continue;
        }
        char path[32];
        path_for(i, path, sizeof(path));
        if (!storage.file_exists(path)) {
            loaded_[i] = true;  // Named but never saved: empty
            continue;
        }
        Header h = {};
        if (storage.read_file_range(path, 0, reinterpret_cast<uint8_t*>(&h), sizeof(h)) !=
                static_cast<int>(sizeof(h)) ||
            std::memcmp(h.magic, kListMagic, sizeof(kListMagic)) != 0) {
            loaded_[i] = true;  // Corrupt: keep empty
            continue;
        }
        const bool complex = h.dtype == static_cast<uint8_t>(Dtype::kComplex);
        if ((h.dtype != static_cast<uint8_t>(Dtype::kDouble) && !complex) ||
            h.count >
                static_cast<uint32_t>(complex ? Array::kMaxComplexElements : Array::kMaxElements)) {
            loaded_[i] = true;
            continue;
        }
        constexpr size_t kSlabElems = ArrayStore::kSlabBytes / sizeof(calc_t);
        if ((complex || h.count > kSlabElems) && !psram_backend::available()) {
            all_done = false;  // Needs PSRAM, not up yet (D14) — retry
            continue;
        }
        const int n = static_cast<int>(h.count);
        Array& lst = lists_[i];
        lst.clear();
        if (!lst.set_dtype(complex ? Dtype::kComplex : Dtype::kDouble) || !lst.resize(n)) {
            all_done = false;
            continue;
        }
        size_t off = sizeof(h);
        bool truncated = false;
        if (complex) {
            auto* cbuf = reinterpret_cast<Complex*>(g_chunk);
            for (int at = 0; at < n; at += kChunkComplex) {
                const int m = n - at < kChunkComplex ? n - at : kChunkComplex;
                const int bytes = m * static_cast<int>(sizeof(Complex));
                if (storage.read_file_range(path, off, reinterpret_cast<uint8_t*>(cbuf),
                                            static_cast<size_t>(bytes)) != bytes) {
                    lst.resize(at);
                    truncated = true;
                    break;
                }
                lst.write_range_c(at, m, cbuf);
                off += static_cast<size_t>(bytes);
            }
        } else {
            for (int at = 0; at < n; at += kChunkElements) {
                const int m = n - at < kChunkElements ? n - at : kChunkElements;
                const int bytes = m * static_cast<int>(sizeof(calc_t));
                if (storage.read_file_range(path, off, reinterpret_cast<uint8_t*>(g_chunk),
                                            static_cast<size_t>(bytes)) != bytes) {
                    lst.resize(at);
                    truncated = true;
                    break;
                }
                lst.write_range(at, m, g_chunk);
                off += static_cast<size_t>(bytes);
            }
        }
        (void)truncated;  // Keep what arrived (lists_persist precedent)
        loaded_[i] = true;
    }
    return all_done;
}

}  // namespace math
