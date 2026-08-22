// Host-side tests for the file browser's listing logic (issues #44-#46).
//
// This is the half of FileBrowserScreen that has no framebuffer and no
// SD card in it — classification, ordering and size formatting — split
// out for exactly that reason. The ordering cases are the ones worth
// having off-device: FatFs hands back creation order, so a wrong
// comparator looks plausible on the four-file card you develop against
// and only falls apart on a real one.

#include <cstdio>
#include <cstring>

#include "apps/file_list.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

void check_size(std::uint32_t bytes, const char* want) {
    char got[24];
    apps::format_size(bytes, got, sizeof(got));
    ++g_checks;
    if (std::strcmp(got, want) != 0) {
        ++g_failures;
        std::printf("FAIL: format_size(%lu) = \"%s\", want \"%s\"\n",
                    static_cast<unsigned long>(bytes), got, want);
    }
}

platform::Storage::DirEntry entry(const char* name, bool is_dir, std::uint32_t size = 0) {
    platform::Storage::DirEntry e = {};
    std::snprintf(e.name, sizeof(e.name), "%s", name);
    e.is_dir = is_dir;
    e.size = size;
    return e;
}

void test_has_ext() {
    check(platform::has_ext("main.py", ".py"), "has_ext exact");
    check(platform::has_ext("MAIN.PY", ".py"), "has_ext is case-insensitive on the name");
    check(platform::has_ext("notes.TxT", ".txt"), "has_ext mixed case");
    check(!platform::has_ext("python", ".py"), "has_ext matches the suffix, not the stem");
    check(!platform::has_ext("py", ".py"), "has_ext needs more than the extension itself");
    check(!platform::has_ext("", ".py"), "has_ext on an empty name");
    check(!platform::has_ext("main.py", ""), "has_ext with an empty extension never matches");
}

void test_classify() {
    check(apps::classify(entry("apps", true)) == apps::FileKind::kDir, "classify dir");
    check(apps::classify(entry("main.py", false)) == apps::FileKind::kScript, "classify .py");
    check(apps::classify(entry("notes.txt", false)) == apps::FileKind::kText, "classify .txt");
    check(apps::classify(entry("elements.csv", false)) == apps::FileKind::kText, "classify .csv");
    check(apps::classify(entry("README.MD", false)) == apps::FileKind::kText, "classify .MD");
    check(apps::classify(entry("list1.dat", false)) == apps::FileKind::kCalcData, "classify .dat");
    check(apps::classify(entry("firmware.uf2", false)) == apps::FileKind::kOther, "classify other");
    // A directory wins over its own name's extension: /picocalc/apps
    // holds one folder per app, and nothing stops one being "plot.py".
    check(apps::classify(entry("plot.py", true)) == apps::FileKind::kDir,
          "a directory named like a script is still a directory");
}

void test_sort_dirs_first() {
    platform::Storage::DirEntry e[5] = {
        entry("zebra.txt", false), entry("apps", true), entry("main.py", false),
        entry("Backup", true),     entry("alpha.dat", false),
    };
    apps::sort_entries(e, 5);
    check(std::strcmp(e[0].name, "apps") == 0, "sort: first dir");
    check(std::strcmp(e[1].name, "Backup") == 0, "sort: second dir, case-insensitively after apps");
    check(std::strcmp(e[2].name, "alpha.dat") == 0, "sort: files follow dirs");
    check(std::strcmp(e[3].name, "main.py") == 0, "sort: files by name");
    check(std::strcmp(e[4].name, "zebra.txt") == 0, "sort: last file");
}

void test_sort_case_insensitive() {
    platform::Storage::DirEntry e[4] = {
        entry("banana", false),
        entry("Apple", false),
        entry("cherry", false),
        entry("APRICOT", false),
    };
    apps::sort_entries(e, 4);
    check(std::strcmp(e[0].name, "Apple") == 0, "icase sort: Apple");
    check(std::strcmp(e[1].name, "APRICOT") == 0, "icase sort: APRICOT after Apple, not before b");
    check(std::strcmp(e[2].name, "banana") == 0, "icase sort: banana");
    check(std::strcmp(e[3].name, "cherry") == 0, "icase sort: cherry");
}

void test_sort_edges() {
    platform::Storage::DirEntry one[1] = {entry("only.txt", false)};
    apps::sort_entries(one, 1);
    check(std::strcmp(one[0].name, "only.txt") == 0, "sort of one entry");
    apps::sort_entries(one, 0);  // must not touch anything
    check(std::strcmp(one[0].name, "only.txt") == 0, "sort of zero entries");

    // A prefix sorts before the longer name that contains it.
    platform::Storage::DirEntry pre[3] = {
        entry("list10.dat", false), entry("list1.dat", false), entry("list2.dat", false)};
    apps::sort_entries(pre, 3);
    check(std::strcmp(pre[0].name, "list1.dat") == 0, "prefix sorts first");
    check(std::strcmp(pre[1].name, "list10.dat") == 0, "list10 before list2 (byte order, not numeric)");
    check(std::strcmp(pre[2].name, "list2.dat") == 0, "list2 last");

    // Names equal but for case must not be dropped or reordered
    // arbitrarily — the tie-break is the exact bytes.
    platform::Storage::DirEntry dup[2] = {entry("readme", false), entry("README", false)};
    apps::sort_entries(dup, 2);
    check(std::strcmp(dup[0].name, "README") == 0, "case tie-break is deterministic");
    check(std::strcmp(dup[1].name, "readme") == 0, "case tie-break keeps both");
}

void test_sort_is_total_over_a_full_listing() {
    // 32 entries is the browser's cap; check the result is ordered by
    // the same predicate the comparator claims, end to end.
    platform::Storage::DirEntry e[32];
    for (int i = 0; i < 32; ++i) {
        char name[16];
        std::snprintf(name, sizeof(name), "%c%02d", static_cast<char>('z' - i % 26), 31 - i);
        e[i] = entry(name, i % 5 == 0);
    }
    apps::sort_entries(e, 32);
    bool ordered = true;
    for (int i = 1; i < 32; ++i) {
        if (e[i - 1].is_dir != e[i].is_dir) {
            ordered = ordered && e[i - 1].is_dir;  // dirs may only precede files
        } else if (std::strcmp(e[i - 1].name, e[i].name) > 0) {
            ordered = false;  // same kind, so plain byte order holds here
        }
    }
    check(ordered, "a full 32-entry listing comes out ordered");
}

void test_format_size() {
    check_size(0, "0 B");
    check_size(1, "1 B");
    check_size(742, "742 B");
    check_size(1023, "1023 B");
    check_size(1024, "1.0K");
    check_size(1229, "1.2K");           // 1.2004K
    check_size(10 * 1024, "10K");       // at 10 units the tenth is dropped
    check_size(46387, "45K");           // 45.3K, truncated
    check_size(1024 * 1024 - 1, "1023K");
    check_size(1024 * 1024, "1.0M");
    check_size(1468006, "1.4M");
    check_size(45UL * 1024 * 1024, "45M");
    check_size(9 * 1024 + 512, "9.5K");

    // The tenth must never carry into the next unit. 10239 B is
    // 9.9990K, and a rounded tenth is 10 — "9.10K" as digits, or a
    // silent jump to 10K. Both misreport a file that is under 10K.
    check_size(10 * 1024 - 1, "9.9K");
    check_size(8191, "7.9K");  // 7.999K, truncated rather than shown as 8.0K
}

void check_move_case(const char* src, bool is_dir, const char* dest_dir,
                     apps::MoveCheck want, const char* want_path, const char* what) {
    char got[platform::kMaxPath];
    got[0] = 0;
    const apps::MoveCheck res = apps::check_move(src, is_dir, dest_dir, got, sizeof(got));
    ++g_checks;
    if (res != want) {
        ++g_failures;
        std::printf("FAIL: %s (verdict %d, want %d)\n", what, static_cast<int>(res),
                    static_cast<int>(want));
        return;
    }
    if (want_path != nullptr && std::strcmp(got, want_path) != 0) {
        ++g_failures;
        std::printf("FAIL: %s (path \"%s\", want \"%s\")\n", what, got, want_path);
    }
}

void test_basename() {
    check(std::strcmp(apps::basename_of("/picocalc/notes/a.txt"), "a.txt") == 0, "basename");
    check(std::strcmp(apps::basename_of("/a.txt"), "a.txt") == 0, "basename at root");
    check(std::strcmp(apps::basename_of("bare"), "bare") == 0, "basename with no slash");
    check(std::strcmp(apps::basename_of(""), "") == 0, "basename of empty");
    check(std::strcmp(apps::basename_of(nullptr), "") == 0, "basename of null");
}

void test_move_ok() {
    check_move_case("/picocalc/a.txt", false, "/picocalc/notes", apps::MoveCheck::kOk,
                    "/picocalc/notes/a.txt", "file moves into a subfolder");
    check_move_case("/picocalc/notes/a.txt", false, "/picocalc", apps::MoveCheck::kOk,
                    "/picocalc/a.txt", "file moves up to the parent");
    check_move_case("/picocalc/apps/old", true, "/picocalc/notes", apps::MoveCheck::kOk,
                    "/picocalc/notes/old", "a directory moves to an unrelated folder");
}

void test_move_same_folder() {
    check_move_case("/picocalc/a.txt", false, "/picocalc", apps::MoveCheck::kSameFolder, nullptr,
                    "moving into the folder it is already in");
    // Reported as kSameFolder rather than falling through to "exists":
    // both refuse, but only one of them says what actually happened.
    check_move_case("/picocalc/notes/a.txt", false, "/picocalc/notes",
                    apps::MoveCheck::kSameFolder, nullptr, "same folder, one level down");
}

void test_move_into_itself() {
    // f_rename would do this and orphan the subtree; the card does not
    // object, so this check is the only thing standing in the way.
    check_move_case("/picocalc/apps", true, "/picocalc/apps", apps::MoveCheck::kIntoItself,
                    nullptr, "a directory into itself");
    check_move_case("/picocalc/apps", true, "/picocalc/apps/periodic",
                    apps::MoveCheck::kIntoItself, nullptr, "a directory into its own child");
    check_move_case("/picocalc/apps", true, "/picocalc/apps/a/b/c", apps::MoveCheck::kIntoItself,
                    nullptr, "a directory deep into its own subtree");

    // The separator is what makes this safe: a sibling whose name
    // merely starts with the source's name is NOT inside it.
    check_move_case("/picocalc/apps", true, "/picocalc/appsdata", apps::MoveCheck::kOk,
                    "/picocalc/appsdata/apps", "a sibling with a prefix name is not inside it");
    // A FILE named like a directory carries no subtree rule.
    check_move_case("/picocalc/apps", false, "/picocalc/apps/periodic", apps::MoveCheck::kOk,
                    "/picocalc/apps/periodic/apps", "the subtree rule applies to dirs only");
}

void test_move_rejects_bad_input() {
    check_move_case("", false, "/picocalc", apps::MoveCheck::kBadSource, nullptr, "empty source");
    check_move_case(nullptr, false, "/picocalc", apps::MoveCheck::kBadSource, nullptr,
                    "null source");
    check_move_case("/picocalc/a.txt", false, "", apps::MoveCheck::kBadSource, nullptr,
                    "empty destination");
    check_move_case("noslash", false, "/picocalc", apps::MoveCheck::kBadSource, nullptr,
                    "source with no directory part");
    check_move_case("/picocalc/", false, "/picocalc/notes", apps::MoveCheck::kBadSource, nullptr,
                    "source ending in a slash has no basename");
}

void test_move_length_bound() {
    // kMaxPath is 128 and descent is capped at 4 levels, so this is not
    // reachable through the UI — which is exactly why it should be
    // checked here rather than trusted.
    char out[24];  // deliberately too small for the joined path
    const apps::MoveCheck res = apps::check_move("/picocalc/notes/a-fairly-long-name.txt", false,
                                                 "/picocalc/some/other/folder", out, sizeof(out));
    check(res == apps::MoveCheck::kTooLong, "a destination that would not fit is refused");

    // One byte of headroom either side of the bound, so the check is
    // the length and not an off-by-one near it.
    char exact[20];  // "/a/bcd.txt" + NUL is 11
    check(apps::check_move("/x/bcd.txt", false, "/a", exact, 11) == apps::MoveCheck::kOk,
          "a destination that fits exactly is allowed");
    check(apps::check_move("/x/bcd.txt", false, "/a", exact, 10) == apps::MoveCheck::kTooLong,
          "one byte short is refused");
}

}  // namespace

int main() {
    test_has_ext();
    test_classify();
    test_sort_dirs_first();
    test_sort_case_insensitive();
    test_sort_edges();
    test_sort_is_total_over_a_full_listing();
    test_format_size();
    test_basename();
    test_move_ok();
    test_move_same_folder();
    test_move_into_itself();
    test_move_rejects_bad_input();
    test_move_length_bound();

    std::printf("test_file_list: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
