// The I/O half of §4.5, split from sd_apps.cpp so the parser stays
// linkable into the host tests: this file reaches Storage and printf,
// that one reaches nothing.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "platform/io_scratch.hpp"
#include "platform/sd_apps.hpp"
#include "platform/storage.hpp"

namespace platform {

namespace {

constexpr const char* kAppsDir = "/picocalc/apps";
constexpr const char* kManifestName = "app.txt";

// PERMANENT, not scan-time scratch — this is the whole reason §4.5 calls
// the table out. Every registered AppEntry's name, icon_glyph and path
// is a POINTER in here, so the table has to outlive the scan.
// 16 * 96 = 1,536 bytes of bss, the standing cost of SD apps existing.
SdAppManifest g_manifests[kMaxSdApps];

// Directory entries to read in one listing. Larger than kMaxSdApps on
// purpose: /picocalc/apps may hold a stray README or a .DS_Store, and a
// truncated listing would silently push a real app off the end.
constexpr int kMaxDirEntries = 32;

// One manifest is small; 512 bytes covers a name, an icon, an entry and
// several ignored keys. A longer file has its tail ignored rather than
// failing.
constexpr std::size_t kManifestBytes = 512;

// Both transient buffers come out of the shared staging region rather
// than bss of their own, on io_scratch.hpp's terms: the scan is a single
// boot (or late-init) step on core 0, it does not nest, and it holds no
// pointer into the region across a call that could reach another owner —
// storage() reads straight into the buffer below and returns.
struct ScanScratch {
    Storage::DirEntry entries[kMaxDirEntries];
    char manifest[kManifestBytes];
};
static_assert(sizeof(ScanScratch) <= kIoScratchBytes,
              "SD app scan scratch does not fit the shared staging region");

}  // namespace

int scan_sd_apps(AppLaunchFn launch) {
    // First, so a re-scan after a card swap replaces the tier-2 rows
    // rather than duplicating them. Built-ins are untouched.
    AppRegistry::clear_sd_apps();
    if (launch == nullptr || !storage().mounted()) {
        return 0;
    }

    // Same as the text editor does for its own save directory: create it
    // if it is missing, so dropping an app folder onto a fresh card does
    // not also require making the parent by hand. An empty apps
    // directory then lists cleanly as zero entries.
    storage().ensure_dir(kAppsDir);

    auto& scratch = *reinterpret_cast<ScanScratch*>(io_scratch());
    const int n = storage().list_dir(kAppsDir, scratch.entries, kMaxDirEntries);
    if (n <= 0) {
        // An empty apps directory is the normal state, not worth a log
        // line.
        return 0;
    }

    int registered = 0;
    for (int i = 0; i < n && registered < kMaxSdApps; ++i) {
        if (!scratch.entries[i].is_dir) {
            continue;
        }
        char dir[kMaxPath];
        char manifest_path[kMaxPath];
        std::snprintf(dir, sizeof(dir), "%s/%s", kAppsDir, scratch.entries[i].name);
        std::snprintf(manifest_path, sizeof(manifest_path), "%s/%s", dir, kManifestName);
        if (!storage().file_exists(manifest_path)) {
            // A directory under apps/ with no manifest is not an error:
            // it is how a user keeps data, or a work in progress, there.
            continue;
        }

        const int len = storage().read_file(
            manifest_path, reinterpret_cast<std::uint8_t*>(scratch.manifest), kManifestBytes);
        if (len <= 0) {
            std::printf("sd-apps: %s unreadable\n", manifest_path);
            continue;
        }

        SdAppManifest& slot = g_manifests[registered];
        if (!parse_app_manifest(scratch.manifest, static_cast<std::size_t>(len), dir, slot)) {
            std::printf("sd-apps: %s malformed, skipped\n", manifest_path);
            continue;
        }
        if (!storage().file_exists(slot.entry_path)) {
            // The one check the parser cannot make, and worth failing
            // on: a tile that raises the moment it is opened is worse
            // than a tile that never appears.
            std::printf("sd-apps: %s names a missing entry %s, skipped\n", manifest_path,
                        slot.entry_path);
            continue;
        }

        AppEntry entry = {};
        entry.name = slot.name;
        entry.icon_glyph = slot.icon_glyph[0] != 0 ? slot.icon_glyph : nullptr;
        entry.kind = AppKind::kScript;
        entry.path = slot.entry_path;
        entry.launch = launch;
        if (!AppRegistry::register_sd_app(entry)) {
            break;  // tier-2 table full
        }
        ++registered;
        std::printf("sd-apps: %s -> %s\n", slot.name, slot.entry_path);
    }
    return registered;
}

}  // namespace platform
