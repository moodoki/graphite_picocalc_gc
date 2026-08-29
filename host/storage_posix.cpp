// platform::Storage over the host filesystem (Phase 6.4.2).
//
// Maps the card's absolute "/picocalc/..." paths into a directory on the
// development machine, so the SD-backed half of the calculator -- persisted
// state, the file browser, and the app manifests 6B.16 scans for -- runs
// without a card reader. That last one matters most: SD app loading is the
// newest and least-exercised code in the tree.
//
// Root directory, in order of preference:
//   $PICOCALC_HOME   -- set it to a fixture directory for a reproducible
//                       screenshot or test run
//   $HOME/.picocalc  -- the fork's behaviour, and the default
//
// TWO DELIBERATE DIVERGENCES FROM THE FIRMWARE, both documented in the
// spec's "what this does not model":
//
//   * list_dir sorts. FatFs returns entries in directory order; POSIX
//     readdir order is filesystem- and machine-dependent, so an unsorted
//     listing would render differently on macOS and Linux. D98 requires
//     the committed image set to be byte-identical on both, and the app
//     launcher does NOT sort its SD tier (the file browser does, in
//     apps/file_list.cpp). Sorting here is the cheapest place to make the
//     images reproducible.
//
//   * Paths containing ".." are refused outright. On the device the card
//     is the whole world; here the root is a directory inside someone's
//     home, and paths are composed from user-supplied data -- manifest
//     entry_path values and file-browser navigation. Escaping the root
//     would mean a malformed app.txt could read or write anywhere the
//     user can.

#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "platform/storage.hpp"

namespace platform {

namespace {

std::string g_root;

// True if any path component is exactly "..". Rejecting the component
// rather than the substring keeps legitimate names like "a..b" working.
bool has_parent_component(const char* path) {
    const char* p = path;
    while (*p != 0) {
        const char* end = std::strchr(p, '/');
        const size_t len = (end == nullptr) ? std::strlen(p) : static_cast<size_t>(end - p);
        if (len == 2 && p[0] == '.' && p[1] == '.') {
            return true;
        }
        if (end == nullptr) {
            break;
        }
        p = end + 1;
    }
    return false;
}

// "/picocalc/apps/foo" -> "<root>/apps/foo". Empty on refusal, which every
// caller below treats as failure.
std::string real_path(const char* path) {
    if (path == nullptr || g_root.empty() || has_parent_component(path)) {
        return {};
    }
    const char* rest = path;
    // The card's root is the mapped directory, so strip the /picocalc
    // prefix when it is there and simply anchor anything else.
    static constexpr char kPrefix[] = "/picocalc";
    const size_t plen = sizeof(kPrefix) - 1;
    if (std::strncmp(rest, kPrefix, plen) == 0 && (rest[plen] == '/' || rest[plen] == 0)) {
        rest += plen;
    }
    while (*rest == '/') {
        ++rest;
    }
    if (*rest == 0) {
        return g_root;
    }
    return g_root + "/" + rest;
}

bool is_dir(const std::string& p) {
    struct stat st{};
    return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

}  // namespace

bool Storage::init() {
    mounted_ = false;
    const char* explicit_root = std::getenv("PICOCALC_HOME");
    if (explicit_root != nullptr && explicit_root[0] != 0) {
        g_root = explicit_root;
    } else {
        const char* home = std::getenv("HOME");
        if (home == nullptr || home[0] == 0) {
            return false;
        }
        g_root = std::string(home) + "/.picocalc";
    }
    // Created rather than required: the device's card is prepared by hand,
    // but a first host run should just work.
    if (::mkdir(g_root.c_str(), 0755) != 0 && !is_dir(g_root)) {
        return false;
    }
    mounted_ = true;
    return true;
}

void Storage::on_card_removed() {
    // No card to eject. Kept so the shared hot-plug path compiles and
    // behaves: after this the mount is gone until init() runs again.
    mounted_ = false;
}

bool Storage::file_exists(const char* path) const {
    if (!mounted_) {
        return false;
    }
    const std::string p = real_path(path);
    if (p.empty()) {
        return false;
    }
    struct stat st{};
    return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

long Storage::file_size(const char* path) const {
    if (!mounted_) {
        return -1;
    }
    const std::string p = real_path(path);
    if (p.empty()) {
        return -1;
    }
    struct stat st{};
    if (::stat(p.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        return -1;
    }
    return static_cast<long>(st.st_size);
}

int Storage::read_file(const char* path, uint8_t* buf, size_t max_len) const {
    return read_file_range(path, 0, buf, max_len);
}

int Storage::read_file_range(const char* path, size_t offset, uint8_t* buf, size_t max_len) const {
    if (!mounted_) {
        return -1;
    }
    const std::string p = real_path(path);
    if (p.empty()) {
        return -1;
    }
    std::FILE* f = std::fopen(p.c_str(), "rb");
    if (f == nullptr) {
        return -1;
    }
    if (offset != 0 && std::fseek(f, static_cast<long>(offset), SEEK_SET) != 0) {
        std::fclose(f);
        return -1;
    }
    // A short read is the end of the file, not an error -- same as f_read.
    const size_t n = std::fread(buf, 1, max_len, f);
    const bool bad = std::ferror(f) != 0;
    std::fclose(f);
    return bad ? -1 : static_cast<int>(n);
}

namespace {

bool write_with_mode(const std::string& p, const uint8_t* buf, size_t len, const char* mode) {
    std::FILE* f = std::fopen(p.c_str(), mode);
    if (f == nullptr) {
        return false;
    }
    const size_t n = std::fwrite(buf, 1, len, f);
    const bool bad = std::ferror(f) != 0;
    return std::fclose(f) == 0 && !bad && n == len;
}

}  // namespace

bool Storage::write_file(const char* path, const uint8_t* buf, size_t len) const {
    if (!mounted_) {
        return false;
    }
    const std::string p = real_path(path);
    return !p.empty() && write_with_mode(p, buf, len, "wb");
}

bool Storage::append_file(const char* path, const uint8_t* buf, size_t len) const {
    if (!mounted_) {
        return false;
    }
    const std::string p = real_path(path);
    return !p.empty() && write_with_mode(p, buf, len, "ab");
}

bool Storage::delete_file(const char* path) const {
    if (!mounted_) {
        return false;
    }
    const std::string p = real_path(path);
    return !p.empty() && ::unlink(p.c_str()) == 0;
}

bool Storage::ensure_dir(const char* path) const {
    if (!mounted_) {
        return false;
    }
    const std::string p = real_path(path);
    if (p.empty()) {
        return false;
    }
    // Already-exists is success, matching FR_EXIST.
    return ::mkdir(p.c_str(), 0755) == 0 || is_dir(p);
}

bool Storage::rename_file(const char* old_path, const char* new_path) const {
    if (!mounted_) {
        return false;
    }
    const std::string from = real_path(old_path);
    const std::string to = real_path(new_path);
    if (from.empty() || to.empty()) {
        return false;
    }
    // NEVER clobber (D55). POSIX rename() silently replaces an existing
    // destination where f_rename refuses, so the check is load-bearing
    // here rather than belt-and-braces as it is in the firmware.
    struct stat st{};
    if (::stat(to.c_str(), &st) == 0) {
        return false;
    }
    return ::rename(from.c_str(), to.c_str()) == 0;
}

bool Storage::delete_dir(const char* path) const {
    if (!mounted_) {
        return false;
    }
    const std::string p = real_path(path);
    // rmdir refuses a non-empty directory, which is exactly D55's
    // deliberately non-recursive contract.
    return !p.empty() && ::rmdir(p.c_str()) == 0;
}

int Storage::list_dir(const char* path, DirEntry* entries, int max_entries, int skip) const {
    if (!mounted_ || entries == nullptr || max_entries <= 0) {
        return -1;
    }
    const std::string p = real_path(path);
    if (p.empty()) {
        return -1;
    }
    DIR* dir = ::opendir(p.c_str());
    if (dir == nullptr) {
        return -1;
    }

    // Read the whole directory, then sort -- see the divergence note at the
    // top. FatFs also omits "." and ".."; POSIX does not, so drop them here
    // or the file browser grows two entries the device never shows.
    std::vector<DirEntry> all;
    while (const dirent* de = ::readdir(dir)) {
        if (std::strcmp(de->d_name, ".") == 0 || std::strcmp(de->d_name, "..") == 0) {
            continue;
        }
        DirEntry e{};
        std::snprintf(e.name, sizeof(e.name), "%s", de->d_name);
        struct stat st{};
        const std::string child = p + "/" + de->d_name;
        if (::stat(child.c_str(), &st) == 0) {
            e.is_dir = S_ISDIR(st.st_mode);
            e.size = static_cast<uint32_t>(st.st_size);
        }
        all.push_back(e);
    }
    ::closedir(dir);

    std::sort(all.begin(), all.end(),
              [](const DirEntry& a, const DirEntry& b) { return std::strcmp(a.name, b.name) < 0; });

    // `skip` resumes a walk the caller could not hold at once (issue #53).
    // Skipping past the end is zero entries, not an error.
    if (skip >= static_cast<int>(all.size())) {
        return 0;
    }
    const int n = std::min(max_entries, static_cast<int>(all.size()) - skip);
    for (int i = 0; i < n; ++i) {
        entries[i] = all[static_cast<size_t>(skip + i)];
    }
    return n;
}

bool Storage::read_string(const char* path, char* buf, size_t max_len) const {
    if (max_len == 0) {
        return false;
    }
    const int n = read_file(path, reinterpret_cast<uint8_t*>(buf), max_len - 1);
    if (n < 0) {
        return false;
    }
    buf[n] = 0;
    return true;
}

bool Storage::write_string(const char* path, const char* str) const {
    return write_file(path, reinterpret_cast<const uint8_t*>(str), std::strlen(str));
}

Storage& storage() {
    static Storage instance;
    return instance;
}

}  // namespace platform
