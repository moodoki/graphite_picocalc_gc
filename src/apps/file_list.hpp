#pragma once

#include <cstddef>
#include <cstdint>

#include "platform/storage.hpp"

namespace apps {

// List presentation for the file browser (issues #45, #46), split out
// of FileBrowserScreen so it needs no framebuffer and no SD card to
// test — the same split that made ui::TextBuffer testable off-device.
// Everything here is a pure function over DirEntry.

// Case-insensitive suffix match. FAT is case-preserving but not
// case-sensitive, so a ".TXT" on the card must still match ".txt".
bool has_ext(const char* name, const char* ext);

// What a listing row IS, which is what the browser colours by. The
// extensions are this SD layout's, not a general guess: the calculator
// persists its own state as .dat (list%d.dat, matrix%d.dat,
// graphstate.dat, ...), an app is main.py, and the editor writes .txt.
enum class FileKind : std::uint8_t {
    kDir,
    kScript,    // .py
    kText,      // .txt, .md, .csv
    kCalcData,  // .dat — calculator state, not meant to be hand-edited
    kOther,
};

FileKind classify(const platform::Storage::DirEntry& e);

// Orders a listing in place: directories first, then by name,
// case-insensitively. Insertion sort rather than std::sort because the
// listing is capped at 32 entries and this costs no recursion and no
// template instantiation for a 72-byte element type.
void sort_entries(platform::Storage::DirEntry* entries, int count);

// Human-readable size: "742 B", "1.2K", "45K", "1.4M". Integer
// arithmetic throughout, so no float formatting is linked in for it.
// Values of 10 units and up are truncated, not rounded, so a size never
// reads larger than the file is.
void format_size(std::uint32_t bytes, char* out, std::size_t out_len);

}  // namespace apps
