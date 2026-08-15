// Host-side tests for the Phase 6A app framework: the two-tier
// AppRegistry (6A.1, D67). Pure logic, no platform dependency — the
// registry deliberately holds no Screen*, only a name and a launch fn,
// which is what makes it testable off-device.

#include <cstdio>
#include <cstring>

#include "platform/app_registry.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

// Launch fns record which entry they were handed, so the tests can
// prove the AppEntry& argument (D67's whole point) actually arrives.
const platform::AppEntry* g_last_launched = nullptr;

void record_launch(const platform::AppEntry& self) {
    g_last_launched = &self;
}

platform::AppEntry make(const char* name, platform::AppKind kind, const char* path) {
    platform::AppEntry e = {};
    e.name = name;
    e.kind = kind;
    e.path = path;
    e.launch = record_launch;
    return e;
}

void test_empty_registry() {
    check(platform::AppRegistry::count() == 0, "empty: count 0");
    check(platform::AppRegistry::get(0) == nullptr, "empty: get(0) null");
    check(platform::AppRegistry::get(-1) == nullptr, "empty: get(-1) null");
}

void test_builtin_tier() {
    check(platform::AppRegistry::register_app(
              make("Notepad", platform::AppKind::kBuiltIn, nullptr)),
          "register built-in");
    check(platform::AppRegistry::register_app(
              make("Files", platform::AppKind::kBuiltIn, nullptr)),
          "register second built-in");

    check(platform::AppRegistry::count() == 2, "two built-ins counted");
    check(platform::AppRegistry::builtin_count() == 2, "builtin_count 2");
    check(platform::AppRegistry::sd_count() == 0, "sd_count 0");

    const auto* first = platform::AppRegistry::get(0);
    check(first != nullptr && std::strcmp(first->name, "Notepad") == 0,
          "registration order preserved");
    check(platform::AppRegistry::get(2) == nullptr, "one past the end is null");
}

void test_rejects_unusable() {
    const int before = platform::AppRegistry::count();

    platform::AppEntry no_launch = {};
    no_launch.name = "Broken";
    check(!platform::AppRegistry::register_app(no_launch), "entry with no launch fn rejected");

    platform::AppEntry no_name = {};
    no_name.launch = record_launch;
    check(!platform::AppRegistry::register_app(no_name), "entry with no name rejected");

    platform::AppEntry empty_name = make("", platform::AppKind::kBuiltIn, nullptr);
    check(!platform::AppRegistry::register_app(empty_name), "entry with empty name rejected");

    check(platform::AppRegistry::count() == before, "rejected entries did not land");
}

void test_sd_tier_sorts_after_builtins() {
    check(platform::AppRegistry::register_sd_app(
              make("Finance", platform::AppKind::kScript, "/picocalc/apps/finance/main.py")),
          "register SD app");

    check(platform::AppRegistry::count() == 3, "combined count");
    check(platform::AppRegistry::builtin_count() == 2, "builtins unchanged by SD register");
    check(platform::AppRegistry::sd_count() == 1, "sd_count 1");

    // The SD tier must land after every built-in, so adding or removing
    // a card never renumbers a built-in row.
    const auto* sd = platform::AppRegistry::get(2);
    check(sd != nullptr && std::strcmp(sd->name, "Finance") == 0, "SD app is the last row");
    check(sd != nullptr && sd->kind == platform::AppKind::kScript, "SD app kind preserved");
    check(sd != nullptr && std::strcmp(sd->path, "/picocalc/apps/finance/main.py") == 0,
          "SD app path preserved");

    const auto* builtin = platform::AppRegistry::get(0);
    check(builtin != nullptr && std::strcmp(builtin->name, "Notepad") == 0,
          "built-in row 0 did not move");
}

void test_clear_sd_apps_spares_builtins() {
    platform::AppRegistry::clear_sd_apps();
    check(platform::AppRegistry::count() == 2, "clear_sd_apps drops only the SD tier");
    check(platform::AppRegistry::builtin_count() == 2, "built-ins survive clear_sd_apps");
    check(platform::AppRegistry::sd_count() == 0, "sd_count back to 0");
    check(platform::AppRegistry::get(2) == nullptr, "cleared SD row is gone");

    // Rescanning must not duplicate — this is why clear_sd_apps exists.
    platform::AppRegistry::register_sd_app(
        make("Finance", platform::AppKind::kScript, "/picocalc/apps/finance/main.py"));
    check(platform::AppRegistry::count() == 3, "rescan after clear does not duplicate");
    platform::AppRegistry::clear_sd_apps();
}

void test_launch_receives_its_entry() {
    // D67: launch takes const AppEntry& precisely so one shared thunk
    // can serve every SD app by reading self.path. Prove the argument
    // is the registry's own entry, not a copy of some caller's local.
    platform::AppRegistry::register_sd_app(
        make("Snake", platform::AppKind::kScript, "/picocalc/apps/snake/main.py"));

    const auto* entry = platform::AppRegistry::get(platform::AppRegistry::count() - 1);
    check(entry != nullptr, "fetched the SD entry");

    g_last_launched = nullptr;
    entry->launch(*entry);
    check(g_last_launched == entry, "launch received the registry's own entry");
    check(g_last_launched != nullptr &&
              std::strcmp(g_last_launched->path, "/picocalc/apps/snake/main.py") == 0,
          "launch can read its own path");

    platform::AppRegistry::clear_sd_apps();
}

void test_capacity_bounds() {
    // Fill the built-in tier to its cap, then confirm the overflow is a
    // clean false rather than a write past the table.
    const int before = platform::AppRegistry::builtin_count();
    int added = 0;
    while (platform::AppRegistry::register_app(
        make("Filler", platform::AppKind::kBuiltIn, nullptr))) {
        ++added;
        if (added > platform::AppRegistry::kMaxBuiltIn + 4) {
            break;  // register_app never refused — fail below rather than spin
        }
    }
    check(before + added == platform::AppRegistry::kMaxBuiltIn, "built-in tier fills to kMaxBuiltIn");
    check(!platform::AppRegistry::register_app(make("Extra", platform::AppKind::kBuiltIn, nullptr)),
          "built-in overflow refused");
    check(platform::AppRegistry::builtin_count() == platform::AppRegistry::kMaxBuiltIn,
          "count stays at the cap after refusal");

    // The SD tier has its own cap and must not be reachable by built-in
    // overflow — a full built-in table cannot spill into SD rows.
    check(platform::AppRegistry::register_sd_app(
              make("SdOk", platform::AppKind::kScript, "/picocalc/apps/x/main.py")),
          "SD tier still accepts entries when built-ins are full");
    platform::AppRegistry::clear_sd_apps();
}

}  // namespace

int main() {
    test_empty_registry();
    test_builtin_tier();
    test_rejects_unusable();
    test_sd_tier_sorts_after_builtins();
    test_clear_sd_apps_spares_builtins();
    test_launch_receives_its_entry();
    test_capacity_bounds();

    std::printf("test_apps: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
