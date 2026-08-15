#pragma once

// SD-discovered app manifests (Phase 6B.15/6B.16, spec §4.5).
//
// A directory under /picocalc/apps/ holding an app.txt becomes its own
// named tile in 6A's launcher, instead of every script being reached
// through the program editor's generic "load" flow. app.txt is flat
// key=value:
//
//     name=Finance
//     icon=$
//     entry=main.py
//
// The scan fills the fixed table below and appends one AppRegistry
// tier-2 entry per manifest (§3.1). Nothing here allocates: AppEntry
// stores POINTERS into the manifest table, so that table is permanent
// and must outlive the scan rather than being scan-time scratch.

#include <cstddef>

#include "platform/app_registry.hpp"

namespace platform {

struct SdAppManifest {
    char name[24];        // launcher row text
    char icon_glyph[8];   // optional, UTF-8 glyph or empty
    char entry_path[64];  // absolute, e.g. "/picocalc/apps/finance/main.py"
};

// Parses one app.txt into `out`. Pure — no Storage, no logging, no
// globals — which is what makes every rule below a host test rather
// than something only a card with a bad file on it can show.
//
// `dir` is the app's directory ("/picocalc/apps/finance"); it supplies
// both defaults:
//
//   * `name` missing  -> the directory's own name ("finance").
//   * `entry` missing -> "main.py" inside the directory.
//
// Defaults rather than rejection because the overwhelmingly common
// manifest is a name and nothing else, and a directory that holds an
// app.txt has already declared its intent. What IS rejected (false,
// caller skips and logs): a path too long for entry_path, a name that
// ends up empty, and `type=` anything but script/python — so a §3.4
// native manifest, if that ever ships, is skipped with a diagnostic
// instead of being handed to the Python interpreter.
//
// Unknown keys are ignored, `#` comments and blank lines are skipped,
// keys are matched case-insensitively, and both sides of the `=` are
// trimmed. Over-long names and icons truncate rather than failing.
bool parse_app_manifest(const char* text, std::size_t len, const char* dir, SdAppManifest& out);

// Scans /picocalc/apps/*/app.txt and registers each valid manifest as a
// tier-2 AppEntry with kind = kScript and path = that slot's
// entry_path. Returns how many were registered. Malformed manifests are
// skipped and logged, never fatal.
//
// Clears the tier-2 table first, so a re-scan after a card swap
// replaces rather than duplicates. Built-in entries are untouched.
//
// `launch` is passed in rather than referenced directly (the spec
// sketched a bare scan_sd_apps()): running a script means pushing a
// screen, and platform/ must not depend on apps/. Every discovered
// entry shares the one thunk — they differ only by path, which is the
// whole reason AppLaunchFn takes the entry (D67).
int scan_sd_apps(AppLaunchFn launch);

// How many manifest slots the scan can fill. Matches the registry's
// tier-2 capacity: a table longer than that would parse manifests it
// could never register.
constexpr int kMaxSdApps = AppRegistry::kMaxSdApps;

}  // namespace platform
