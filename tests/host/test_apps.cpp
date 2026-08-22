// Host-side tests for the Phase 6A app framework: the two-tier
// AppRegistry (6A.1, D67), and 6B.15's manifest parser on top of it.
// Pure logic, no platform dependency — the registry deliberately holds
// no Screen*, only a name and a launch fn, and parse_app_manifest takes
// a buffer rather than a path, which is what makes both testable
// off-device.
//
// The manifest cases matter here specifically because the alternative
// is discovering them one bad app.txt at a time, on a board, with an
// SD card that has to come out to be edited.

#include <cstdio>
#include <cstring>

#include "platform/app_registry.hpp"
#include "platform/sd_apps.hpp"

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

// ---- 6B.15: the app.txt parser ----

bool parse(const char* text, platform::SdAppManifest& out,
           const char* dir = "/picocalc/apps/finance") {
    return platform::parse_app_manifest(text, std::strlen(text), dir, out);
}

void test_manifest_full() {
    platform::SdAppManifest m = {};
    check(parse("name=Finance\nicon=$\nentry=main.py\n", m), "full manifest parses");
    check(std::strcmp(m.name, "Finance") == 0, "name read");
    check(std::strcmp(m.icon_glyph, "$") == 0, "icon read");
    check(std::strcmp(m.entry_path, "/picocalc/apps/finance/main.py") == 0,
          "relative entry resolves against the app dir");
}

void test_manifest_defaults() {
    // The common case: a directory, a main.py, and nothing said about
    // either. Both defaults have to fire for that app to appear at all.
    platform::SdAppManifest m = {};
    check(parse("", m), "empty manifest still yields an app");
    check(std::strcmp(m.name, "finance") == 0, "name defaults to the directory name");
    check(std::strcmp(m.entry_path, "/picocalc/apps/finance/main.py") == 0,
          "entry defaults to main.py");
    check(m.icon_glyph[0] == 0, "icon stays empty when absent");

    platform::SdAppManifest named = {};
    check(parse("name=Loan Calc\n", named), "name-only manifest parses");
    check(std::strcmp(named.name, "Loan Calc") == 0, "an embedded space is part of the name");
    check(std::strcmp(named.entry_path, "/picocalc/apps/finance/main.py") == 0,
          "entry still defaults when only the name is given");
}

void test_manifest_whitespace_and_comments() {
    platform::SdAppManifest m = {};
    check(parse("# a comment\r\n\r\n  name  =  Finance  \r\n"
                "\t# indented comment\r\nentry = run.py\r\n",
                m),
          "CRLF, blank lines, comments and padding all survive");
    check(std::strcmp(m.name, "Finance") == 0, "both sides of '=' are trimmed");
    check(std::strcmp(m.entry_path, "/picocalc/apps/finance/run.py") == 0, "trimmed entry resolves");

    platform::SdAppManifest last = {};
    check(parse("name=First\nname=Second", last), "no trailing newline is fine");
    check(std::strcmp(last.name, "Second") == 0, "a repeated key takes the last value");
}

void test_manifest_case_and_unknown_keys() {
    platform::SdAppManifest m = {};
    check(parse("NAME=Finance\nEntry=go.py\nauthor=someone\nversion=2\n", m),
          "keys match case-insensitively and unknown keys are ignored");
    check(std::strcmp(m.name, "Finance") == 0, "NAME= is name=");
    check(std::strcmp(m.entry_path, "/picocalc/apps/finance/go.py") == 0, "Entry= is entry=");

    platform::SdAppManifest junk = {};
    check(parse("this line has no equals sign\nname=Ok\n", junk),
          "a line with no '=' is skipped, not fatal");
    check(std::strcmp(junk.name, "Ok") == 0, "parsing continues past the junk line");
}

void test_manifest_absolute_entry() {
    platform::SdAppManifest m = {};
    check(parse("name=Shared\nentry=/picocalc/programs/shared.py\n", m),
          "an absolute entry parses");
    check(std::strcmp(m.entry_path, "/picocalc/programs/shared.py") == 0,
          "an absolute entry is taken as written, not appended to the dir");
}

void test_manifest_type_key() {
    platform::SdAppManifest m = {};
    check(parse("name=A\ntype=script\n", m), "type=script accepted");
    check(parse("name=A\ntype=python\n", m), "type=python accepted");
    check(parse("name=A\ntype=\n", m), "an empty type= is a half-written line, not a claim");
    // §3.4's native apps do not exist. A manifest claiming to be one
    // must not be handed to the Python interpreter, which would run the
    // .uf2 as source and raise something incomprehensible.
    check(!parse("name=A\ntype=native\n", m), "type=native refused until §3.4 exists");
}

void test_manifest_truncation_and_limits() {
    platform::SdAppManifest m = {};
    check(parse("name=A name far longer than the launcher row can ever show\n", m),
          "an over-long name truncates rather than failing");
    check(std::strlen(m.name) == sizeof(m.name) - 1, "truncated to the field width");

    // entry_path is 64 bytes; a dir plus entry that cannot fit has to be
    // refused, because a silently truncated path would name a file that
    // either does not exist or — worse — is a different one.
    char long_dir[80];
    std::snprintf(long_dir, sizeof(long_dir), "/picocalc/apps/%s",
                  "a-directory-name-long-enough-to-overflow-the-entry-path-field");
    platform::SdAppManifest over = {};
    check(!parse("entry=main.py\n", over, long_dir), "an over-long composed path is refused");

    platform::SdAppManifest no_leaf = {};
    check(!parse("", no_leaf, "/"), "a dir with no leaf and no name= is refused");
    check(!platform::parse_app_manifest("", 0, nullptr, no_leaf), "a null dir is refused");
    check(platform::parse_app_manifest(nullptr, 0, "/picocalc/apps/x", no_leaf),
          "a null text body falls back to both defaults");
    check(std::strcmp(no_leaf.entry_path, "/picocalc/apps/x/main.py") == 0,
          "defaults applied with no text at all");
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
    test_manifest_full();
    test_manifest_defaults();
    test_manifest_whitespace_and_comments();
    test_manifest_case_and_unknown_keys();
    test_manifest_absolute_entry();
    test_manifest_type_key();
    test_manifest_truncation_and_limits();

    std::printf("test_apps: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
