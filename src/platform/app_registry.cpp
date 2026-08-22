#include "platform/app_registry.hpp"

namespace platform {

namespace {

// Two fixed tables rather than one, so clear_sd_apps() is a counter
// reset and can never renumber or drop a built-in entry.
AppEntry g_builtin[AppRegistry::kMaxBuiltIn] = {};
int g_builtin_count = 0;

AppEntry g_sd[AppRegistry::kMaxSdApps] = {};
int g_sd_count = 0;

bool usable(const AppEntry& entry) {
    return entry.name != nullptr && entry.name[0] != 0 && entry.launch != nullptr;
}

}  // namespace

bool AppRegistry::register_app(const AppEntry& entry) {
    if (!usable(entry) || g_builtin_count >= kMaxBuiltIn) {
        return false;
    }
    g_builtin[g_builtin_count++] = entry;
    return true;
}

bool AppRegistry::register_sd_app(const AppEntry& entry) {
    if (!usable(entry) || g_sd_count >= kMaxSdApps) {
        return false;
    }
    g_sd[g_sd_count++] = entry;
    return true;
}

void AppRegistry::clear_sd_apps() {
    g_sd_count = 0;
}

int AppRegistry::count() {
    return g_builtin_count + g_sd_count;
}

int AppRegistry::builtin_count() {
    return g_builtin_count;
}

int AppRegistry::sd_count() {
    return g_sd_count;
}

const AppEntry* AppRegistry::get(int index) {
    if (index < 0) {
        return nullptr;
    }
    if (index < g_builtin_count) {
        return &g_builtin[index];
    }
    const int sd_index = index - g_builtin_count;
    if (sd_index < g_sd_count) {
        return &g_sd[sd_index];
    }
    return nullptr;
}

}  // namespace platform
