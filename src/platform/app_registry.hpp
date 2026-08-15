#pragma once

// App registration table (Phase 6A.1, spec §3.1, D67).
//
// Two tiers, one list. Compiled-in apps (Notepad, and later the Python
// program editor) are registered explicitly at boot from main.cpp.
// SD-discovered apps — MicroPython scripts (§4.5) and, if §3.4 is ever
// built, compiled .uf2 apps — are appended by scan_sd_apps() once the
// card has mounted. Both tiers read back through one count()/get()
// loop, so the launcher walks a single list.
//
// Registration is an explicit list in main.cpp rather than
// static-initializer self-registration (D67), so launcher ordering is
// visible in one place and does not depend on translation-unit init
// order.

#include <cstdint>

namespace platform {

enum class AppKind : std::uint8_t {
    kBuiltIn,  // compiled into this firmware image; launch pushes a screen
    kScript,   // SD-discovered MicroPython app (§4.5), path = entry .py
    kNative,   // SD-discovered compiled .uf2 (§3.4, stretch), path = .uf2
};

struct AppEntry;

// Takes the entry so one thunk can serve every SD app: they differ only
// by path, and none of them exist until the boot scan runs, so a bare
// void(*)() would need a thunk generated per discovered app (D67).
// kBuiltIn entries ignore the argument and just push their screen.
using AppLaunchFn = void (*)(const AppEntry& self);

struct AppEntry {
    const char* name = nullptr;        // "Notepad", shown in the launcher
    const char* icon_glyph = nullptr;  // optional single-glyph icon, or null
    AppKind kind = AppKind::kBuiltIn;

    // kScript/kNative: the SD path this entry launches. kBuiltIn: null.
    // For SD entries this points into the permanent SdAppManifest table
    // (§4.5), never into scan-time scratch — the registry stores the
    // pointer, not a copy.
    const char* path = nullptr;

    AppLaunchFn launch = nullptr;
};

class AppRegistry {
public:
    // Built-in apps get the low indices, SD apps the high ones, so the
    // launcher's built-in rows never move when a card is added or
    // removed.
    static constexpr int kMaxBuiltIn = 16;
    static constexpr int kMaxSdApps = 16;
    static constexpr int kMaxApps = kMaxBuiltIn + kMaxSdApps;

    // Tier 1 — compiled-in apps, called at boot before the SD scan.
    // Ignores entries with no name or no launch fn. Returns false when
    // the built-in table is full.
    static bool register_app(const AppEntry& entry);

    // Tier 2 — SD-discovered apps, appended by scan_sd_apps() (§4.5).
    // Separate from register_app only so a failed or absent SD card can
    // never disturb the built-in entries; both tiers read back through
    // count()/get().
    static bool register_sd_app(const AppEntry& entry);

    // Drops every tier-2 entry, leaving the built-ins untouched. For a
    // card swap: rescanning without this would duplicate rows.
    static void clear_sd_apps();

    static int count();
    static int builtin_count();
    static int sd_count();

    // Null when index is out of range.
    static const AppEntry* get(int index);
};

}  // namespace platform
