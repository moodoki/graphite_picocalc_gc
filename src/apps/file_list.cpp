#include "apps/file_list.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace apps {

namespace {

char lower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// Case-insensitive strcmp, with a case-sensitive tie-break so the order
// is total: "README" and "readme" can coexist in one FAT directory only
// as different names, and a comparator that called them equal would
// leave their relative order down to the listing order.
int name_cmp(const char* a, const char* b) {
    const char* pa = a;
    const char* pb = b;
    for (; *pa != 0 && *pb != 0; ++pa, ++pb) {
        const char la = lower(*pa);
        const char lb = lower(*pb);
        if (la != lb) {
            return la < lb ? -1 : 1;
        }
    }
    if (*pa != *pb) {
        return *pa == 0 ? -1 : 1;
    }
    return std::strcmp(a, b);  // equal ignoring case: order by exact bytes
}

}  // namespace

FileKind classify(const platform::Storage::DirEntry& e) {
    using platform::has_ext;
    if (e.is_dir) {
        return FileKind::kDir;
    }
    if (has_ext(e.name, ".py")) {
        return FileKind::kScript;
    }
    if (has_ext(e.name, ".txt") || has_ext(e.name, ".md") || has_ext(e.name, ".csv")) {
        return FileKind::kText;
    }
    if (has_ext(e.name, ".dat")) {
        return FileKind::kCalcData;
    }
    return FileKind::kOther;
}

void sort_entries(platform::Storage::DirEntry* entries, int count) {
    for (int i = 1; i < count; ++i) {
        const platform::Storage::DirEntry key = entries[i];
        int j = i - 1;
        while (j >= 0) {
            const bool after = entries[j].is_dir != key.is_dir
                                   ? !entries[j].is_dir  // a file sorts after any directory
                                   : name_cmp(entries[j].name, key.name) > 0;
            if (!after) {
                break;
            }
            entries[j + 1] = entries[j];
            --j;
        }
        entries[j + 1] = key;
    }
}

void format_size(std::uint32_t bytes, char* out, std::size_t out_len) {
    if (bytes < 1024) {
        std::snprintf(out, out_len, "%lu B", static_cast<unsigned long>(bytes));
        return;
    }
    const char* unit = "K";
    std::uint32_t whole = bytes / 1024;
    std::uint32_t rem = bytes % 1024;
    if (whole >= 1024) {
        rem = whole % 1024;
        whole /= 1024;
        unit = "M";
    }
    if (whole < 10) {
        // A tenth below 10 units: "1.2K" carries information "1K" loses,
        // and most files on this card are in that range. Clamped, so a
        // rounded tenth never carries into the next unit — 10239 B is
        // 9.9990K, and a tenth of 10 would print as "9.10K".
        const std::uint32_t tenths = std::min<std::uint32_t>((rem * 10 + 512) / 1024, 9);
        std::snprintf(out, out_len, "%lu.%lu%s", static_cast<unsigned long>(whole),
                      static_cast<unsigned long>(tenths), unit);
        return;
    }
    std::snprintf(out, out_len, "%lu%s", static_cast<unsigned long>(whole), unit);
}

const char* basename_of(const char* path) {
    if (path == nullptr) {
        return "";
    }
    const char* slash = std::strrchr(path, '/');
    return slash != nullptr ? slash + 1 : path;
}

MoveCheck check_move(const char* src_path, bool src_is_dir, const char* dest_dir, char* out_dest,
                     std::size_t out_len) {
    if (src_path == nullptr || dest_dir == nullptr || src_path[0] == 0 || dest_dir[0] == 0) {
        return MoveCheck::kBadSource;
    }
    const char* slash = std::strrchr(src_path, '/');
    if (slash == nullptr || slash[1] == 0) {
        return MoveCheck::kBadSource;
    }

    // Already here? Compare the source's own directory with the target,
    // rather than comparing paths after the join — a no-op move would
    // otherwise fail later as "destination exists", which is true but
    // says the wrong thing.
    const auto src_dir_len = static_cast<std::size_t>(slash - src_path);
    if (std::strlen(dest_dir) == src_dir_len &&
        std::strncmp(src_path, dest_dir, src_dir_len) == 0) {
        return MoveCheck::kSameFolder;
    }

    // A directory may not be moved into itself or into anything below
    // it. Both are checked against the SOURCE path with a separator, so
    // "/picocalc/apps" does not swallow "/picocalc/appsdata".
    if (src_is_dir) {
        const std::size_t src_len = std::strlen(src_path);
        if (std::strncmp(dest_dir, src_path, src_len) == 0 &&
            (dest_dir[src_len] == 0 || dest_dir[src_len] == '/')) {
            return MoveCheck::kIntoItself;
        }
    }

    const char* name = slash + 1;
    // dest_dir + '/' + name + NUL
    if (std::strlen(dest_dir) + 1 + std::strlen(name) + 1 > out_len) {
        return MoveCheck::kTooLong;
    }
    std::snprintf(out_dest, out_len, "%s/%s", dest_dir, name);
    return MoveCheck::kOk;
}

}  // namespace apps
