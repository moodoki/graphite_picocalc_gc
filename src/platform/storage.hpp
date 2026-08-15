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
    int list_dir(const char* path, DirEntry* entries, int max_entries) const;

    // Convenience: NUL-terminated string I/O.
    bool read_string(const char* path, char* buf, size_t max_len) const;
    bool write_string(const char* path, const char* str) const;

private:
    bool mounted_ = false;
};

Storage& storage();

}  // namespace platform
