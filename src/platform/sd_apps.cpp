#include "platform/sd_apps.hpp"

#include <algorithm>
#include <cstring>

namespace platform {

namespace {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// Case-insensitive compare of [begin, end) against a NUL-terminated key.
bool key_is(const char* begin, const char* end, const char* key) {
    for (const char* p = begin; p != end; ++p, ++key) {
        if (*key == 0 || lower(*p) != *key) {
            return false;
        }
    }
    return *key == 0;
}

// Copies [begin, end) into a fixed field, truncating rather than
// failing — a 30-character app name should show as 23 characters in the
// launcher, not stop the app from appearing at all.
void copy_field(char* dst, std::size_t cap, const char* begin, const char* end) {
    auto n = static_cast<std::size_t>(end - begin);
    n = std::min(n, cap - 1);
    std::memcpy(dst, begin, n);
    dst[n] = 0;
}

// The last path component of "/picocalc/apps/finance" — the fallback
// app name. Returns a pointer into `dir`.
const char* dir_leaf(const char* dir) {
    const char* leaf = dir;
    for (const char* p = dir; *p != 0; ++p) {
        if (*p == '/') {
            leaf = p + 1;
        }
    }
    return leaf;
}

}  // namespace

bool parse_app_manifest(const char* text, std::size_t len, const char* dir, SdAppManifest& out) {
    if (dir == nullptr || dir[0] == 0) {
        return false;
    }
    out.name[0] = 0;
    out.icon_glyph[0] = 0;
    out.entry_path[0] = 0;

    // Held separately from out.entry_path because it may still be
    // relative to `dir` when the last line has been read.
    char entry[64] = {};
    bool type_ok = true;

    const char* p = text;
    const char* const end = text == nullptr ? nullptr : text + len;
    while (p != nullptr && p < end) {
        const char* line = p;
        while (p < end && *p != '\n') {
            ++p;
        }
        const char* line_end = p;
        if (p < end) {
            ++p;  // step over the newline
        }

        while (line < line_end && is_space(*line)) {
            ++line;
        }
        while (line_end > line && is_space(line_end[-1])) {
            --line_end;
        }
        if (line == line_end || *line == '#') {
            continue;
        }

        const char* eq = line;
        while (eq < line_end && *eq != '=') {
            ++eq;
        }
        if (eq == line_end) {
            continue;  // not a key=value line; ignore rather than fail
        }

        const char* key_end = eq;
        while (key_end > line && is_space(key_end[-1])) {
            --key_end;
        }
        const char* val = eq + 1;
        while (val < line_end && is_space(*val)) {
            ++val;
        }

        if (key_is(line, key_end, "name")) {
            copy_field(out.name, sizeof(out.name), val, line_end);
        } else if (key_is(line, key_end, "icon")) {
            copy_field(out.icon_glyph, sizeof(out.icon_glyph), val, line_end);
        } else if (key_is(line, key_end, "entry")) {
            copy_field(entry, sizeof(entry), val, line_end);
        } else if (key_is(line, key_end, "type")) {
            // Empty counts as script: `type=` with nothing after it is a
            // half-written line, not a claim to be something else.
            type_ok = val == line_end || key_is(val, line_end, "script") ||
                      key_is(val, line_end, "python");
        }
        // Anything else: ignored, so a manifest can carry version= or
        // author= without this having to know about them.
    }

    if (!type_ok) {
        return false;
    }
    if (out.name[0] == 0) {
        const char* leaf = dir_leaf(dir);
        copy_field(out.name, sizeof(out.name), leaf, leaf + std::strlen(leaf));
        if (out.name[0] == 0) {
            return false;  // a directory path ending in '/' has no leaf
        }
    }
    if (entry[0] == 0) {
        std::memcpy(entry, "main.py", sizeof("main.py"));
    }

    if (entry[0] == '/') {
        // Absolute: taken as written, which is what §3.4's native
        // extension would need (a .uf2 outside the app's directory).
        if (std::strlen(entry) >= sizeof(out.entry_path)) {
            return false;
        }
        std::memcpy(out.entry_path, entry, std::strlen(entry) + 1);
        return true;
    }

    const std::size_t dir_len = std::strlen(dir);
    const std::size_t entry_len = std::strlen(entry);
    if (dir_len + 1 + entry_len >= sizeof(out.entry_path)) {
        return false;
    }
    std::memcpy(out.entry_path, dir, dir_len);
    out.entry_path[dir_len] = '/';
    std::memcpy(out.entry_path + dir_len + 1, entry, entry_len + 1);
    return true;
}

}  // namespace platform
