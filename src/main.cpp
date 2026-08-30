// PicoCalc Graphing Calculator (Graphite) — entry point.
//
// Core 0: application (input, logic, rendering into the framebuffer).
// Core 1: display service (RGB565->666 conversion + SPI DMA push, D10).
//
// Boots to the calculator home screen; the hardware diagnostics screen
// (DiagScreen: display/keyboard/PSRAM/SD/battery/die-temp self-tests) is
// reachable via the typed `diag` command.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "hardware/watchdog.h"
#include "pico/rand.h"
#include "pico/stdlib.h"

#include "config.hpp"
#include "platform/app_registry.hpp"
#include "platform/boot_trace.hpp"
#include "platform/fault.hpp"
#include "platform/platform.hpp"
#include "platform/power.hpp"
#include "platform/sd_apps.hpp"
#include "platform/sd_card.hpp"
#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/chrome.hpp"
#include "ui/input_line.hpp"  // kCapacity bounds the serial-injection buffer
#include "ui/screen_manager.hpp"
#include "math/array.hpp"
#include "math/functions.hpp"
#include "math/lists.hpp"
#include "math/matrix.hpp"
#include "math/named_lists.hpp"
#include "apps/files_screen.hpp"
#include "apps/graph_model.hpp"
#include "apps/home_screen.hpp"
#include "apps/notepad_screen.hpp"
#include "apps/program_screen.hpp"

// Build id from CMake (git short hash, "-dev" when the tree is dirty).
#ifndef PICOCALC_BUILD_ID
#define PICOCALC_BUILD_ID "unknown"
#endif
// Current code-complete phase, also from CMake (single source of truth
// next to the build-id block).
#ifndef PICOCALC_PHASE
#define PICOCALC_PHASE "?"
#endif

namespace {

platform::InitStatus g_init_status;

// SD read/write self-test result (task 1.5 acceptance).
enum class SdTest : std::uint8_t { kNoCard, kFailed, kOk };
SdTest g_sd_test = SdTest::kNoCard;

// PSRAM allocator self-test result (task 1.6 acceptance).
bool g_psram_alloc_ok = false;

// Bulk-transfer self-test (D10 un-quarantine, 2026-07-18).
enum class BulkTest : std::uint8_t { kNotRun, kOk, kFailed, kHungLastBoot };
BulkTest g_psram_bulk = BulkTest::kNotRun;
uint32_t g_bulk_write_us = 0;  // 1 KB timings
uint32_t g_bulk_read_us = 0;
int g_bulk_fail_step = -1;

// The historical failure mode of the bulk path is a DMA wait that never
// returns, so the test arms the hardware watchdog: a wedge reboots in
// 2 s, and the marker left in a watchdog scratch register makes the
// next boot skip the test instead of boot-looping. (Scratch 4-7 belong
// to the boot ROM's watchdog-vector protocol; 0/1 are free.)
constexpr uint32_t kBulkTestMarker = 0xB07DFACEu;

// Hard fault on the previous boot (D47), read before anything else can
// consume the watchdog reboot cause and reported once USB is up.
bool g_prior_fault = false;
platform::FaultInfo g_fault;

// P6-14 hardware spike (2026-08-14, phase6-spec.md §0.3): does
// watchdog_caused_reboot() actually read false after a genuine physical
// power-cycle? Captured at the same point as g_prior_fault, before
// anything else (run_self_tests()'s PSRAM bulk test) can re-arm the
// watchdog and overwrite the reason bits.
bool g_watchdog_caused_reboot = false;

void run_psram_bulk_test() {
    if (watchdog_caused_reboot() && watchdog_hw->scratch[0] == kBulkTestMarker) {
        watchdog_hw->scratch[0] = 0;
        g_bulk_fail_step = static_cast<int>(watchdog_hw->scratch[1]);
        g_psram_bulk = BulkTest::kHungLastBoot;
        return;
    }

    auto& ps = platform::psram();
    const uint32_t base = ps.alloc(2 * 1024 + 64);
    if (base == platform::Psram::kInvalid) {
        return;  // Leave kNotRun; a late-init retry may succeed.
    }

    static uint8_t src[1024];
    static uint8_t dst[1024];

    watchdog_hw->scratch[0] = kBulkTestMarker;
    watchdog_enable(2'000, true);

    bool ok = true;
    int step = 0;
    auto check = [&](uint32_t addr, size_t len, uint32_t seed) {
        watchdog_hw->scratch[1] = static_cast<uint32_t>(++step);
        watchdog_update();
        for (size_t i = 0; i < len; ++i) {
            src[i] = static_cast<uint8_t>(seed + i * 31);
        }
        std::memset(dst, 0, len);
        ps.write(addr, src, len);
        ps.read(addr, dst, len);
        if (std::memcmp(src, dst, len) != 0) {
            ok = false;
            g_bulk_fail_step = step;
        }
    };

    // Sizes straddling the 27/31-byte PIO chunk caps, then bigger runs.
    check(base, 1, 0x11);
    check(base, 27, 0x22);
    check(base, 28, 0x33);
    check(base, 31, 0x44);
    check(base, 32, 0x55);
    check(base, 64, 0x66);
    check(base, 255, 0x77);
    check(base, 1024, 0x88);
    check(base + 3, 61, 0x99);  // Unaligned start

    // Chunk-boundary addressing: two 32-byte writes must read back as
    // one contiguous 64-byte run (catches per-chunk address bugs).
    {
        watchdog_hw->scratch[1] = static_cast<uint32_t>(++step);
        watchdog_update();
        for (size_t i = 0; i < 64; ++i) {
            src[i] = static_cast<uint8_t>(0xA0 + i * 7);
        }
        ps.write(base + 1024, src, 32);
        ps.write(base + 1024 + 32, src + 32, 32);
        std::memset(dst, 0, 64);
        ps.read(base + 1024, dst, 64);
        if (std::memcmp(src, dst, 64) != 0) {
            ok = false;
            g_bulk_fail_step = step;
        }
    }

    // 1 KB timing (D10 revisit data: informs the Array PSRAM tier).
    {
        watchdog_hw->scratch[1] = static_cast<uint32_t>(++step);
        watchdog_update();
        uint64_t t0 = time_us_64();
        ps.write(base, src, 1024);
        g_bulk_write_us = static_cast<uint32_t>(time_us_64() - t0);
        t0 = time_us_64();
        ps.read(base, dst, 1024);
        g_bulk_read_us = static_cast<uint32_t>(time_us_64() - t0);
    }

    hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
    watchdog_hw->scratch[0] = 0;
    g_psram_bulk = ok ? BulkTest::kOk : BulkTest::kFailed;
    printf("psram-bulk: %s (1KB write %lu us, read %lu us)\n", ok ? "OK" : "FAIL",
           static_cast<unsigned long>(g_bulk_write_us), static_cast<unsigned long>(g_bulk_read_us));
}

void run_self_tests() {
    // Both tests are skipped once green: with the D26 retry-forever
    // heartbeat this runs indefinitely while *either* subsystem is
    // down, and the PSRAM word test bump-allocates 256 B per run (no
    // free), while the SD probe rewrites a file. Ejecting a card
    // resets g_sd_test, so a remount retests.
    if (g_init_status.storage && g_sd_test != SdTest::kOk) {
        auto& fs = platform::storage();
        fs.ensure_dir("/picocalc");
        const char* probe = "/picocalc/selftest.txt";
        char buf[32] = {};
        if (fs.write_string(probe, "picocalc-ok") && fs.read_string(probe, buf, sizeof(buf)) &&
            std::strcmp(buf, "picocalc-ok") == 0) {
            g_sd_test = SdTest::kOk;
            fs.delete_file(probe);
        } else {
            g_sd_test = SdTest::kFailed;
        }
    }

    if (g_init_status.psram && !g_psram_alloc_ok) {
        auto& ps = platform::psram();
        // Word-based r/w test (bulk is tested separately below — D10).
        const uint32_t addr = ps.alloc(256);
        if (addr != platform::Psram::kInvalid) {
            bool ok = true;
            for (uint32_t i = 0; i < 64; ++i) {
                ps.write_word(addr + i * 4, 0xA5A50000u + i);
            }
            for (uint32_t i = 0; i < 64; ++i) {
                if (ps.read_word(addr + i * 4) != 0xA5A50000u + i) {
                    ok = false;
                    break;
                }
            }
            g_psram_alloc_ok = ok;
        }
    }

    // Bulk path (D10): run once, after the word path proves healthy —
    // late-init retries re-enter here until PSRAM comes up.
    if (g_psram_alloc_ok && g_psram_bulk == BulkTest::kNotRun) {
        run_psram_bulk_test();
    }
}

// ---- Diagnostics screen ----

class DiagScreen : public ui::Screen {
public:
    bool on_key(const platform::KeyEvent& ev) override {
        if (ev.key == platform::Key::kNone || !ev.pressed) {
            return false;
        }
        if (ev.key == platform::Key::kEscape) {
            ui::screen_manager().pop();
            return true;
        }
        last_key_ = ev;
        ++key_count_;
        printf("key: code=%d ch='%c' shift=%d ctrl=%d alt=%d\n", static_cast<int>(ev.key),
               ev.ch != 0 ? ev.ch : ' ', ev.shift_held, ev.ctrl_held, ev.alt_held);
        return true;
    }

    void render(gfx::Framebuffer& fb) override {
        using namespace platform::colors;
        const auto& font = gfx::main_font();
        const int lh = font.height() + 2;

        fb.clear(kBlack);
        int y = 8;
        font.draw_string(fb, 8, y, "Graphite GC", kGreen, kBlack);
        // Phase + build id, right-aligned on the title line (git short
        // hash, "-dev" when the tree is dirty).
        char line[48];
        std::snprintf(line, sizeof(line), "Phase %s [%s]", PICOCALC_PHASE, PICOCALC_BUILD_ID);
        font.draw_string(fb, platform::kScreenW - 8 - font.text_width(line), y, line, kGrayLine,
                         kBlack);
        y += lh * 2;

#if PICOCALC_PICO2
        font.draw_string(fb, 8, y, "Board: Pico 2 (RP2350)", kWhite, kBlack);
#else
        font.draw_string(fb, 8, y, "Board: Pico 1 (RP2040)", kWhite, kBlack);
#endif
        y += lh;

        std::snprintf(line, sizeof(line), "Display: OK  Keyboard: %s",
                      g_init_status.keyboard ? "OK" : "FAIL");
        font.draw_string(fb, 8, y, line, kWhite, kBlack);
        y += lh;

        const char* bulk = g_psram_bulk == BulkTest::kOk             ? "bulk OK"
                           : g_psram_bulk == BulkTest::kFailed       ? "bulk FAIL"
                           : g_psram_bulk == BulkTest::kHungLastBoot ? "bulk HUNG"
                                                                     : "bulk not run";
        std::snprintf(line, sizeof(line), "PSRAM: %s, %s",
                      !g_init_status.psram ? "not detected"
                      : g_psram_alloc_ok   ? "word OK"
                                           : "word FAIL",
                      bulk);
        const bool psram_all_ok =
            g_init_status.psram && g_psram_alloc_ok && g_psram_bulk == BulkTest::kOk;
        font.draw_string(fb, 8, y, line, psram_all_ok ? kGreen : kRed, kBlack);
        y += lh;

        std::snprintf(line, sizeof(line), "SD card: %s",
                      g_sd_test == SdTest::kOk       ? "OK (file r/w verified)"
                      : g_sd_test == SdTest::kFailed ? "FAIL"
                                                     : "no card");
        font.draw_string(fb, 8, y, line, g_sd_test == SdTest::kOk ? kGreen : kRed, kBlack);
        y += lh;

        const auto batt = platform::battery_status();
        if (batt.percent >= 0) {
            std::snprintf(line, sizeof(line), "Battery: %d%%%s", batt.percent,
                          batt.charging ? " (charging)" : "");
        } else {
            std::snprintf(line, sizeof(line), "Battery: unavailable");
        }
        font.draw_string(fb, 8, y, line, batt.percent >= 0 ? kWhite : kRed, kBlack);
        y += lh;

        const float temp = platform::die_temp_c();
        std::snprintf(line, sizeof(line), "Die temp: %.1f C", static_cast<double>(temp));
        font.draw_string(fb, 8, y, line, kWhite, kBlack);
        y += lh * 2;

        std::snprintf(line, sizeof(line), "Keys seen: %d", key_count_);
        font.draw_string(fb, 8, y, line, kWhite, kBlack);
        y += lh;
        if (last_key_.ch != 0) {
            std::snprintf(line, sizeof(line), "Last key: '%c'", last_key_.ch);
        } else {
            std::snprintf(line, sizeof(line), "Last key: code %d", static_cast<int>(last_key_.key));
        }
        font.draw_string(fb, 8, y, line, kWhite, kBlack);
        y += lh * 2;

        font.draw_string(fb, 8, y, "Type on the keyboard to test input.", kGrayLine, kBlack);
        y += lh;
        // 8px-wide font on a 320px panel: keep lines under ~38 chars.
        font.draw_string(fb, 8, y, "ESC exits.", kGrayLine, kBlack);
        y += lh;
        font.draw_string(fb, 8, y, "Type files on home for SD list.", kGrayLine, kBlack);

        // Color bars: visual check for channel order / 565->666 conversion.
        const int bar_y = platform::kScreenH - 24;
        fb.fill_rect(0, bar_y, 80, 24, kRed);
        fb.fill_rect(80, bar_y, 80, 24, kGreen);
        fb.fill_rect(160, bar_y, 80, 24, kBlue);
        fb.fill_rect(240, bar_y, 80, 24, kWhite);
    }

private:
    platform::KeyEvent last_key_;
    int key_count_ = 0;
};

DiagScreen g_diag_screen;

// ---- Built-in app registry entries (Phase 6A.1, D67 tier 1) ----
//
// Each launch fn is captureless, so it converts to AppLaunchFn. A
// kBuiltIn entry ignores the AppEntry argument — only the SD tiers
// need it, to read their own `path`.
void register_builtin_apps() {
    platform::AppEntry notepad = {};
    notepad.name = "Notepad";
    notepad.kind = platform::AppKind::kBuiltIn;
    notepad.launch = [](const platform::AppEntry&) {
        ui::screen_manager().push(&apps::notepad_screen());
    };
    platform::AppRegistry::register_app(notepad);

    platform::AppEntry python = {};
    python.name = "Python";
    python.kind = platform::AppKind::kBuiltIn;
    python.launch = [](const platform::AppEntry&) {
        // Paired with launch_sd_app()'s queue_app() below: one singleton
        // screen, two modes, and the mode is always chosen here rather
        // than left over from the last visit.
        apps::program_screen().open_editor();
        ui::screen_manager().push(&apps::program_screen());
    };
    platform::AppRegistry::register_app(python);

    platform::AppEntry files = {};
    files.name = "Files";
    files.kind = platform::AppKind::kBuiltIn;
    files.launch = [](const platform::AppEntry&) {
        ui::screen_manager().push(&apps::files_screen());
    };
    platform::AppRegistry::register_app(files);
}

// The one thunk every SD-discovered app shares (§4.5, 6B.16). They
// differ only by path, and none of them exists until the scan runs —
// which is exactly why AppLaunchFn takes the entry rather than being a
// bare void(*)() (D67).
//
// Queue then push, in that order: ProgramScreen runs the script from
// on_activate, so it is already the top screen by the time the script
// draws or asks for a graph.
void launch_sd_app(const platform::AppEntry& self) {
    apps::program_screen().queue_app(self.path, self.name);
    ui::screen_manager().push(&apps::program_screen());
}

}  // namespace

int main() {
    stdio_init_all();

    // Before anything else can consume the watchdog reboot cause — a
    // fault-triggered reboot looks like any other to run_self_tests().
    g_prior_fault = platform::take_prior_fault(&g_fault);
    g_watchdog_caused_reboot = watchdog_caused_reboot();
    // Same ordering rule as the two captures above: read the previous boot's
    // verdict before arming anything. From here until boot_trace_end(), a
    // stall of more than a few seconds reboots the board instead of hanging
    // it -- psram().init() below has waits that cannot time out on their own.
    platform::boot_trace_begin();
    // Paint the unused stack now, while it is shallow, so the heartbeat
    // below can report how deep anything has actually gone (D47).
    platform::paint_stack();

    g_init_status = platform::init();
    platform::boot_stage(platform::BootStage::kSelfTest);
    run_self_tests();
    // Disarms the boot watchdog: the loads below go through FatFs, where a
    // slow card legitimately takes seconds. The backlight came on back in
    // platform::init(), where it now lives.
    platform::boot_stage(platform::BootStage::kStateLoad);

    math::fn::seed_rand(get_rand_64());

    // Dual-core display pipeline (D10, revived 2026-07-25): core 1 DMAs
    // the current strip while core 0 renders the next. Strip mode (Pico 1)
    // only; no-op on the full-framebuffer path. Must precede the first
    // render_frame below. The D10 stall was XIP flash contention with
    // core 0's USB stack, fixed by RAM-residency — see the D10 addendum.
    gfx::start_display_service();

    apps::home_screen().load_state();
    apps::load_graph_state();
    // Data lists (Phase 3A). load() is all-or-nothing and returns
    // false while SD or (for large lists) PSRAM is still down — the
    // late-init loop below retries it.
    bool lists_loaded = math::lists().load(platform::storage());
    // Named user lists (4D.13) — same contract.
    bool named_loaded = math::named_lists().load(platform::storage());
    // Matrix variables (Phase 4A) — same all-or-nothing contract.
    bool matrices_loaded = math::matrices().load(platform::storage());
    // MatAns (last matrix result) — persisted like the named matrices.
    bool matans_loaded = math::load_ans(platform::storage());
    // Device power settings (4D.19-20): brightness/APD; a missing file
    // keeps the STM32's own boot defaults.
    bool settings_loaded = platform::power::load(platform::storage());
    // The typed `diag` command pushes the diagnostics overlay (the old
    // global F6 toggle is gone — 2026-07-18 remap).
    apps::home_screen().set_diag_screen(&g_diag_screen);

    // Built-in apps for the 6A launcher (D67 tier 1). An explicit list
    // here rather than per-translation-unit self-registration, so the
    // launcher's row order is visible in one place and doesn't depend
    // on static-init order. The SD tier (tier 2) is appended later by
    // 6B.16's scan, and always sorts after these.
    register_builtin_apps();
    // Tier 2 (§4.5, 6B.16). A no-op when the card has not mounted yet —
    // the late-init loop below rescans once it does, which is the
    // ordinary case on an RP2350 cold boot (D14).
    platform::scan_sd_apps(&launch_sd_app);

    auto& mgr = ui::screen_manager();
    mgr.push(&apps::home_screen());

    // RP2350 cold boot: the peripheral rail needs ~5-8 s to settle, so
    // the boot-time PSRAM/SD init can fail on a cold power-on (D14 —
    // measured 2026-07-11; warm reboots and Pico 1 are unaffected).
    // Boot stays instant; the loop below retries until they come up.
    // A failing SD attempt with a card inserted blocks up to ~1 s, so
    // retries are spaced out.
    //
    // Retries cover the self-tests too, not just init: in the marginal
    // window init can "succeed" while readback is still garbage, and a
    // one-shot retest right after a late reinit can fail the same way —
    // either case used to freeze FAIL on the diag screen for good
    // (observed on HW 2026-07-17).
    //
    // D26 (HW 2026-07-19): retries no longer give up. The old 30 s
    // window is only the *fast phase* — after it, an unhealthy SD or
    // PSRAM keeps retrying on a slow heartbeat forever. An SD card
    // observed to need longer than the window after an extended
    // power-off used to stay dead until a reboot; the status bar shows
    // red SD/PSRAM while a subsystem is down (ui::set_health_flags).
    constexpr uint32_t kRetryFastWindowMs = 30'000;
    constexpr uint32_t kRetryFastGapMs = 2'000;
    constexpr uint32_t kRetrySlowGapMs = 10'000;
    uint32_t late_init_last_ms = 0;
    // Persisted state loads exactly once: a card mounted late loads
    // then, but a card ejected + re-inserted mid-session must NOT
    // clobber the in-memory working state with stale files (D26).
    bool state_loaded = g_init_status.storage;

    // Event-driven rendering: a full-frame push is ~200 ms, so redraw only
    // after input (or the initial frame) instead of every loop iteration.
    bool dirty = true;

    // Boot is over: clear the wedge streak, and make sure nothing left the
    // boot watchdog armed behind us.
    platform::boot_trace_end();

    while (true) {
        const bool psram_healthy = g_init_status.psram && g_psram_alloc_ok;
        const bool sd_healthy = g_init_status.storage && g_sd_test == SdTest::kOk;

        // D26 hot-plug: poll the DET pin (~1 s, one GPIO read).
        // Ejecting drops the mount right away — otherwise FatFs keeps
        // believing in the card and a state save could half-write.
        // Insertion arms an immediate retry below instead of waiting
        // out the slow heartbeat.
        {
            static uint32_t last_det_ms = 0;
            static bool last_present = platform::sd::card_present();
            const uint32_t now = platform::uptime_ms();
            if (now - last_det_ms >= 1'000) {
                last_det_ms = now;
                const bool present = platform::sd::card_present();
                if (!present && g_init_status.storage) {
                    platform::storage().on_card_removed();
                    g_init_status.storage = false;
                    g_sd_test = SdTest::kNoCard;
                    printf("sd: card removed at %lu ms\n", static_cast<unsigned long>(now));
                }
                if (present && !last_present) {
                    late_init_last_ms = 0;
                    printf("sd: card inserted at %lu ms\n", static_cast<unsigned long>(now));
                }
                last_present = present;
            }
        }

        if (!psram_healthy || !sd_healthy) {
            const uint32_t now = platform::uptime_ms();
            const uint32_t gap = now < kRetryFastWindowMs ? kRetryFastGapMs : kRetrySlowGapMs;
            if (now - late_init_last_ms >= gap) {
                late_init_last_ms = now;
                if (!g_init_status.psram && platform::psram().reinit()) {
                    g_init_status.psram = true;
                    printf("late-init: psram up at %lu ms\n", static_cast<unsigned long>(now));
                }
                if (!g_init_status.storage && platform::storage().init()) {
                    g_init_status.storage = true;
                    if (!state_loaded) {
                        // Persistence arrived late: load what boot couldn't.
                        apps::home_screen().load_state();
                        apps::load_graph_state();
                        state_loaded = true;
                        printf("late-init: storage up at %lu ms, state loaded\n",
                               static_cast<unsigned long>(now));
                    } else {
                        printf("late-init: storage remounted at %lu ms\n",
                               static_cast<unsigned long>(now));
                    }
                    // Both branches: the card that just mounted may be a
                    // different card. scan_sd_apps() clears tier 2 first,
                    // so this replaces the launcher's SD rows rather
                    // than duplicating them (§4.5). After the state
                    // loads above, never during — they share
                    // io_scratch's staging region.
                    const int n_apps = platform::scan_sd_apps(&launch_sd_app);
                    if (n_apps > 0) {
                        printf("late-init: %d sd app(s) registered\n", n_apps);
                    }
                }
                run_self_tests();
                if (!lists_loaded) {
                    lists_loaded = math::lists().load(platform::storage());
                    if (lists_loaded) {
                        printf("late-init: lists loaded at %lu ms\n",
                               static_cast<unsigned long>(now));
                    }
                }
                if (!named_loaded) {
                    named_loaded = math::named_lists().load(platform::storage());
                    if (named_loaded) {
                        printf("late-init: named lists loaded at %lu ms\n",
                               static_cast<unsigned long>(now));
                    }
                }
                if (!settings_loaded) {
                    settings_loaded = platform::power::load(platform::storage());
                    if (settings_loaded) {
                        printf("late-init: settings loaded at %lu ms\n",
                               static_cast<unsigned long>(now));
                    }
                }
                if (!matrices_loaded) {
                    matrices_loaded = math::matrices().load(platform::storage());
                    if (matrices_loaded) {
                        printf("late-init: matrices loaded at %lu ms\n",
                               static_cast<unsigned long>(now));
                    }
                }
                if (!matans_loaded) {
                    matans_loaded = math::load_ans(platform::storage());
                    if (matans_loaded) {
                        printf("late-init: matans loaded at %lu ms\n",
                               static_cast<unsigned long>(now));
                    }
                }
                const bool psram_now = g_init_status.psram && g_psram_alloc_ok;
                const bool sd_now = g_init_status.storage && g_sd_test == SdTest::kOk;
                if (psram_now != psram_healthy || sd_now != sd_healthy) {
                    printf("late-init: self-tests psram=%s sd=%s at %lu ms\n",
                           psram_now ? "ok" : "FAIL", sd_now ? "ok" : "FAIL",
                           static_cast<unsigned long>(now));
                    if (ui::Screen* s = mgr.current()) {
                        s->invalidate_all();
                    }
                    dirty = true;
                }
            }
        }

        // D26 status-bar health indicators (red SD / PSRAM): update on
        // any change, in either direction, and repaint the status band
        // (same pattern as the battery refresh below).
        {
            static bool last_sd_ok = true;
            static bool last_psram_ok = true;
            const bool sd_ok = g_init_status.storage && g_sd_test == SdTest::kOk;
            const bool ps_ok = g_init_status.psram && g_psram_alloc_ok;
            if (sd_ok != last_sd_ok || ps_ok != last_psram_ok) {
                last_sd_ok = sd_ok;
                last_psram_ok = ps_ok;
                ui::set_health_flags(sd_ok, ps_ok);
                // Not while a script owns the panel (6B.8/D80) — repainting
                // the status bar there would put chrome over its canvas.
                if (ui::Screen* s = mgr.current(); s != nullptr && !s->owns_display()) {
                    s->invalidate_band(0, ui::kStatusBarH);
                    dirty = true;
                }
            }
        }

        // Once the app has stayed up a few seconds, forget any fault streak:
        // the BOOTSEL escape in fault_capture() is only meant to fire for a
        // fault that recurs every boot, not for an isolated one.
        {
            static bool streak_cleared = false;
            if (!streak_cleared && platform::uptime_ms() > 5'000) {
                streak_cleared = true;
                platform::clear_fault_streak();
            }
        }

        // Core-0 stack high-water mark (D47). The guard says *that* it
        // overflowed; this says how close normal work comes, which is what
        // sizing a depth cap actually needs. 4096 is the whole of SCRATCH_Y,
        // with core 1's stack immediately below it.
        {
            static uint32_t last_stack_report_ms = 0;
            static uint32_t last_peak = 0;
            const uint32_t now_ms = platform::uptime_ms();
            const uint32_t peak = platform::stack_peak_used();
            // Report on the usual 30 s cadence, but also immediately on any
            // new high-water mark — that is the interesting event.
            if (now_ms > 3'000 && (peak > last_peak || now_ms - last_stack_report_ms >= 30'000)) {
                last_stack_report_ms = now_ms;
                last_peak = peak;
                printf("stack: peak %lu of %lu\n", static_cast<unsigned long>(peak),
                       static_cast<unsigned long>(platform::stack_total()));
                // ArrayStore slab high-water, on the same event (D70
                // lever B): kSlabCount must be sized from what real use
                // actually peaks at, not from a guess. `miss` counts
                // allocations that fell back to PSRAM because the pool
                // was empty — nonzero is not an error, it is the
                // fallback working, but it is what costs speed.
                auto& store = math::array_store();
                printf("slabs: peak %d of %d, live %d, miss %lu\n", store.slabs_peak(),
                       math::ArrayStore::kSlabCount, store.slabs_live(),
                       static_cast<unsigned long>(store.slab_misses()));
            }
        }

        // Previous boot ended in a hard fault (D47). Repeated on the
        // same 30 s heartbeat as the others rather than printed once at
        // boot — a one-shot before USB enumerates is a one-shot nobody
        // sees, and this is exactly the line worth not missing.
        if (g_prior_fault) {
            static uint32_t last_fault_report_ms = 0;
            const uint32_t now_ms = platform::uptime_ms();
            if (now_ms > 3'000 && now_ms - last_fault_report_ms >= 30'000) {
                last_fault_report_ms = now_ms;
                printf(
                    "fault: core %lu streak %lu  pc=0x%08lx lr=0x%08lx sp=0x%08lx  "
                    "stack %lu of %lu\n",
                    static_cast<unsigned long>(g_fault.core),
                    static_cast<unsigned long>(g_fault.streak),
                    static_cast<unsigned long>(g_fault.pc), static_cast<unsigned long>(g_fault.lr),
                    static_cast<unsigned long>(g_fault.sp),
                    static_cast<unsigned long>(g_fault.depth),
                    static_cast<unsigned long>(platform::stack_total()));
            }
        }

        // Previous boot never reached the main loop (2026-08-30). On the
        // same 30 s heartbeat as the blocks above and for a sharper version
        // of the same reason: the boot this reports on is one where nothing
        // was capturable while it was happening.
        {
            static uint32_t last_wedge_report_ms = 0;
            platform::BootStage wedge_stage = platform::BootStage::kEntry;
            uint32_t wedge_streak = 0;
            const uint32_t now_ms = platform::uptime_ms();
            if (platform::prior_boot_wedged(&wedge_stage, &wedge_streak) && now_ms > 3'000 &&
                now_ms - last_wedge_report_ms >= 30'000) {
                last_wedge_report_ms = now_ms;
                printf("boot: previous boot wedged at stage %s, streak %lu\n",
                       platform::boot_stage_name(wedge_stage),
                       static_cast<unsigned long>(wedge_streak));
            }
        }

        // P6-14 hardware spike (2026-08-14): does watchdog_caused_reboot()
        // read false after a genuine power-cycle? Same heartbeat reasoning
        // as the blocks above — a one-shot before USB enumerates would be
        // missed on exactly the boot this is meant to catch.
        {
            static uint32_t last_wd_report_ms = 0;
            const uint32_t now_ms = platform::uptime_ms();
            if (now_ms > 3'000 && now_ms - last_wd_report_ms >= 30'000) {
                last_wd_report_ms = now_ms;
                printf("boot: watchdog_caused_reboot=%d\n", g_watchdog_caused_reboot ? 1 : 0);
            }
        }

        // D10 verification: repeat the bulk-test verdict on a 30 s
        // heartbeat (like battery:) — the boot-time print races USB
        // enumeration and a one-shot is easy to miss on attach.
        {
            static uint32_t last_bulk_report_ms = 0;
            const uint32_t now_ms = platform::uptime_ms();
            if (now_ms > 3'000 && now_ms - last_bulk_report_ms >= 30'000 &&
                g_psram_bulk != BulkTest::kNotRun) {
                last_bulk_report_ms = now_ms;
                const char* verdict = g_psram_bulk == BulkTest::kOk       ? "OK"
                                      : g_psram_bulk == BulkTest::kFailed ? "FAIL"
                                                                          : "HUNG-LAST-BOOT";
                printf("psram-bulk: %s step=%d (1KB write %lu us, read %lu us)\n", verdict,
                       g_bulk_fail_step, static_cast<unsigned long>(g_bulk_write_us),
                       static_cast<unsigned long>(g_bulk_read_us));
            }
        }

        // Die-temperature heartbeat (30 s, like battery/psram-bulk). The
        // on-chip sensor reads junction temp — useful for watching the
        // Pico 1 overclock + core-1 display service thermals.
        {
            static uint32_t last_temp_report_ms = 0;
            const uint32_t now_ms = platform::uptime_ms();
            if (now_ms > 3'000 && now_ms - last_temp_report_ms >= 30'000) {
                last_temp_report_ms = now_ms;
                printf("temp: die %.1f C\n", static_cast<double>(platform::die_temp_c()));
            }
        }

        // Status-bar liveness: the render model is event-driven, so a
        // battery change used to sit invisible until the next keypress
        // (stale %/charging on screen, HW 2026-07-18). This check ran
        // at 1 Hz but read a cache with a 30 s I2C refresh inside, so
        // on-screen freshness was really ~31 s (offline spin, HW
        // 2026-07-18). battery_poll() now owns the refresh (~5 s pacing
        // for STM32 stability, this is its only call site); repaint
        // just the 16-row status band on change.
        {
            static uint32_t last_batt_check_ms = 0;
            static int last_batt_percent = -2;  // Distinct from the -1 "unknown"
            static bool last_batt_charging = false;
            const uint32_t now = platform::uptime_ms();
            if (now - last_batt_check_ms >= 1'000) {
                last_batt_check_ms = now;
                const auto batt = platform::battery_poll();
                if (batt.percent != last_batt_percent || batt.charging != last_batt_charging) {
                    last_batt_percent = batt.percent;
                    last_batt_charging = batt.charging;
                    // See the health-flag branch above: a script's canvas is
                    // not ours to draw on.
                    if (ui::Screen* s = mgr.current(); s != nullptr && !s->owns_display()) {
                        s->invalidate_band(0, ui::kStatusBarH);
                        dirty = true;
                    }
                }
            }
        }

        // Drain every queued key before rendering: while a render is in
        // flight the STM32 FIFO keeps buffering held-key repeats, and
        // handling one event per frame made the backlog play out after
        // release (table scroll overrun, HW 2026-07-18). The first
        // drain attempt broke on the first kNone — but poll() is a
        // two-phase machine, so kNone usually means "read in flight",
        // not "FIFO empty", and the loop still consumed one event per
        // frame (offline spin, HW 2026-07-18). Now keep polling until
        // a completed read reports the FIFO empty; each drained event
        // costs one ~10 ms poll cycle, still far cheaper than a frame.
        // Cap + time budget guard against a wedged FIFO streaming
        // events forever.
        // Soft-sleep APD (4D.19): inactivity timer + the paced STM32
        // backlight write queue live in power::tick().
        platform::power::tick();

#if PICOCALC_SERIAL_INJECT
        // Serial line injection (Phase 5.1, tasks 5.1.2/5.1.3). Nothing else
        // in the firmware reads stdin, so any byte arriving here is a host
        // script submitting an expression to the home screen.
        //
        // The buffer is static (bss): this runs on core 0, whose whole stack
        // is 4 KB, and D47/D48 were both about buffers sitting on it. One
        // copy is safe because only this loop touches it.
        {
            static char inject_buf[ui::InputLine::kCapacity];
            static size_t inject_len = 0;
            static bool inject_overflow = false;

            // Bound the work per frame so a chattering host cannot starve
            // rendering, the same reflex as the key drain's kMaxEventsPerFrame.
            constexpr int kMaxInjectCharsPerFrame = 512;
            for (int i = 0; i < kMaxInjectCharsPerFrame; ++i) {
                const int c = getchar_timeout_us(0);
                if (c < 0) {
                    break;  // Nothing waiting — non-blocking
                }
                if (c == '\r') {
                    continue;
                }
                if (c != '\n') {
                    if (inject_len + 1 < sizeof(inject_buf)) {
                        inject_buf[inject_len++] = static_cast<char>(c);
                    } else {
                        inject_overflow = true;  // Report, never truncate
                    }
                    continue;
                }

                inject_buf[inject_len] = 0;
                const size_t line_len = inject_len;
                inject_len = 0;
                const bool was_overflow = inject_overflow;
                inject_overflow = false;

                if (was_overflow) {
                    printf("inject: error line too long (max %u)\n",
                           static_cast<unsigned>(sizeof(inject_buf) - 1));
                    continue;
                }
                if (line_len == 0) {
                    continue;  // Bare newline — keepalive, not a submission
                }

                // Injection targets the home screen. Popping is deterministic
                // and is what a user would do; erroring out instead would
                // strand an unattended script on whatever screen was open.
                if (mgr.current() != &apps::home_screen()) {
                    mgr.pop_to_root();
                    printf("inject: popped to home\n");
                }

                const char* result = nullptr;
                const char* kind = nullptr;
                // Evaluation time, for §9's A/B pass (5.2.12). The A/B number
                // itself is the host-side round trip, because the baseline is a
                // *released* binary that predates this field and cannot report
                // one. This exists to BOUND that number: the gap between the two
                // says how much of the round trip was never evaluation, without
                // which a small M1 delta cannot be told from USB jitter.
                //
                // Appended at the end of the line, never inserted, so one parser
                // reads both builds — the baseline simply has no `us=` to find.
                const uint64_t t0 = time_us_64();
                const bool ok = apps::home_screen().submit_line(inject_buf, &result, &kind);
                const uint64_t elapsed_us = time_us_64() - t0;
                if (!ok) {
                    printf("inject: error rejected \"%s\"\n", inject_buf);
                } else if (result == nullptr) {
                    // Dispatched as a typed command (cls, diag, ...), which
                    // pushes no history entry to report.
                    printf("inject: \"%s\" -> command\n", inject_buf);
                } else {
#if PICOCALC_EVAL_PROBE
                    printf("inject: \"%s\" -> \"%s\" kind=%s us=%lu eval_us=%lu\n", inject_buf,
                           result, kind, static_cast<unsigned long>(elapsed_us),
                           static_cast<unsigned long>(apps::home_eval_us()));
#else
                    printf("inject: \"%s\" -> \"%s\" kind=%s us=%lu\n", inject_buf, result, kind,
                           static_cast<unsigned long>(elapsed_us));
#endif
                }
                dirty = true;
            }
        }
#endif

        constexpr int kMaxEventsPerFrame = 16;
        constexpr uint64_t kDrainBudgetUs = 250'000;
        const uint64_t drain_deadline_us = platform::uptime_us() + kDrainBudgetUs;
        for (int n = 0; n < kMaxEventsPerFrame;) {
            const platform::KeyEvent ev = platform::keyboard().poll();
            if (ev.key == platform::Key::kNone) {
                if (platform::keyboard().fifo_empty() ||
                    platform::uptime_us() >= drain_deadline_us) {
                    break;
                }
                continue;  // Read in flight (or hold-repeat suppressed)
            }
            ++n;
            if (platform::power::note_key(ev.pressed)) {
                dirty = true;  // Waking repaints under the restored light
                continue;      // The wake key must not also type
            }
            if (!ev.pressed) {
                continue;
            }
            // Diagnostics are reached via the typed `diag` command on
            // the home screen (2026-07-18 remap; F6 freed).
            if (ev.key == platform::Key::kHome && mgr.current() != &apps::home_screen()) {
                // HOME returns to the home screen from anywhere. On the
                // home screen itself it falls through to the input line
                // (cursor-to-start).
                mgr.pop_to_root();
            } else {
                mgr.handle_key(ev);
            }
            dirty = true;
        }
        if (dirty) {
            mgr.render_frame();
            dirty = false;
        }
    }
}
