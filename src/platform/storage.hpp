#pragma once

#include <cstddef>
#include <cstdint>

namespace platform {

// SD card file access via FatFs. All paths are absolute ("/picocalc/...").
class Storage {
public:
    // Mount the SD card. Returns false if no card or mount failed.
    bool init();

    bool mounted() const { return mounted_; }

    bool file_exists(const char* path) const;
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
