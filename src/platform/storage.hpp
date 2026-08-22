#pragma once

#include <cstddef>
#include <cstdint>

namespace platform {

// Absolute-path buffer size, including the NUL (Phase 6A.6). Before
// this, every caller sized its own local to the one path it knew about
// (char path[24] / [32]); the file browser is the first component that
// composes paths it can't predict, so the bound needs a name. DirEntry
// names are 64 B, and the browser caps descent at 4 levels below its
// start directory, so 128 leaves room without sizing for arbitrary
// nesting nothing on this SD layout needs.
constexpr size_t kMaxPath = 128;

// Case-insensitive extension match: FAT is case-preserving but not
// case-sensitive, so a ".TXT" on the card must still match ".txt".
//
// Header-only and dependency-free on purpose. It began in the file
// browser, and the `calc` binding needs the same rule — but scripting/
// sits below apps/ and must not reach up into it, so the shared rule
// lives here, next to the filesystem whose case behaviour is the reason
// it exists.
inline bool has_ext(const char* name, const char* ext) {
    if (name == nullptr || ext == nullptr) {
        return false;
    }
    const size_t n = __builtin_strlen(name);
    const size_t e = __builtin_strlen(ext);
    if (e == 0 || n < e) {
        return false;
    }
    const char* tail = name + (n - e);
    for (size_t i = 0; i < e; ++i) {
        char a = tail[i];
        char b = ext[i];
        if (a >= 'A' && a <= 'Z') {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

// SD card file access via FatFs. All paths are absolute ("/picocalc/...").
class Storage {
public:
    // Mount the SD card. Returns false if no card or mount failed.
    bool init();

    // D26 hot-plug: the DET pin says the card is gone — drop the mount
    // (and the sd-layer init state) immediately rather than letting
    // I/O fail mid-write; the main loop's retry heartbeat re-inits
    // after re-insertion.
    void on_card_removed();

    bool mounted() const { return mounted_; }

    bool file_exists(const char* path) const;
    // File size in bytes, or -1 if missing/unopenable. Used to read the
    // tail of an append-only log (history.txt) rather than its head.
    long file_size(const char* path) const;
    // Returns bytes read, or -1 on error.
    int read_file(const char* path, uint8_t* buf, size_t max_len) const;
    // Read up to max_len bytes starting at byte `offset` — for files
    // too large for one SRAM buffer (lists.dat, Phase 3). Returns
    // bytes read, or -1 on error.
    int read_file_range(const char* path, size_t offset, uint8_t* buf, size_t max_len) const;
    bool write_file(const char* path, const uint8_t* buf, size_t len) const;
    bool append_file(const char* path, const uint8_t* buf, size_t len) const;
    bool delete_file(const char* path) const;

    // Creates the directory if missing (no error if it exists).
    bool ensure_dir(const char* path) const;

    // Renames or moves a file or directory (Phase 6A.7, D55). False if
    // the destination already exists or the source doesn't — never
    // clobbers.
    bool rename_file(const char* old_path, const char* new_path) const;

    // Removes an EMPTY directory. False if it doesn't exist or still
    // has entries — deliberately non-recursive (D55), so emptying a
    // populated directory stays an explicit, separate act rather than
    // the side effect of one delete keypress.
    bool delete_dir(const char* path) const;

    struct DirEntry {
        char name[64];
        bool is_dir;
        uint32_t size;
    };
    // Returns number of entries, or -1 on error.
    //
    // `skip` resumes: it discards that many entries before filling, so a
    // caller holding a small buffer can walk a directory it could never
    // hold at once (issue #53). A short return means the end was
    // reached. Resuming re-opens and re-scans, so walking a directory of
    // n entries in windows of w costs about n^2/2w readdirs — chosen over
    // holding an open FF_DIR across calls, which would leak the handle if
    // the caller abandoned the walk, and over a permanent 2.3 KB buffer.
    int list_dir(const char* path, DirEntry* entries, int max_entries, int skip = 0) const;

    // Convenience: NUL-terminated string I/O.
    bool read_string(const char* path, char* buf, size_t max_len) const;
    bool write_string(const char* path, const char* str) const;

private:
    bool mounted_ = false;
};

Storage& storage();

}  // namespace platform
