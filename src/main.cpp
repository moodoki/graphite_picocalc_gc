// PicoCalc Graphing Calculator — entry point.
//
// Core 0: application (input, logic, rendering into the framebuffer).
// Core 1: display service (RGB565->666 conversion + SPI DMA push).
//
// Milestone 1 state: boots to a hardware diagnostics screen that
// exercises display, keyboard, PSRAM, and SD card. Replaced by the
// calculator home screen in milestone 2.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"

#include "config.hpp"
#include "platform/platform.hpp"
#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "apps/files_screen.hpp"
#include "apps/graph_model.hpp"
#include "apps/home_screen.hpp"

namespace {

platform::InitStatus g_init_status;

// SD read/write self-test result (task 1.5 acceptance).
enum class SdTest : std::uint8_t { kNoCard, kFailed, kOk };
SdTest g_sd_test = SdTest::kNoCard;

// PSRAM allocator self-test result (task 1.6 acceptance).
bool g_psram_alloc_ok = false;

void run_self_tests() {
    if (g_init_status.storage) {
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

    if (g_init_status.psram) {
        auto& ps = platform::psram();
        // Word-based r/w test (the vendored bulk path hangs on HW — D10).
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
        if (ev.key == platform::Key::kF5) {  // SD file listing
            ui::screen_manager().push(&apps::files_screen());
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
        font.draw_string(fb, 8, y, "PicoCalc GraphCalc  [milestone 1]", kGreen, kBlack);
        y += lh * 2;

#if PICOCALC_PICO2
        font.draw_string(fb, 8, y, "Board: Pico 2 (RP2350)", kWhite, kBlack);
#else
        font.draw_string(fb, 8, y, "Board: Pico 1 (RP2040)", kWhite, kBlack);
#endif
        y += lh;

        char line[48];
        std::snprintf(line, sizeof(line), "Display: OK  Keyboard: %s",
                      g_init_status.keyboard ? "OK" : "FAIL");
        font.draw_string(fb, 8, y, line, kWhite, kBlack);
        y += lh;

        std::snprintf(line, sizeof(line), "PSRAM: %s",
                      !g_init_status.psram ? "not detected"
                      : g_psram_alloc_ok   ? "OK (1KB r/w verified)"
                                           : "FAIL (readback mismatch)");
        font.draw_string(fb, 8, y, line, g_init_status.psram && g_psram_alloc_ok ? kGreen : kRed,
                         kBlack);
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
        font.draw_string(fb, 8, y, "F5 files. F6 or ESC exits.", kGrayLine, kBlack);
        y += lh;
        std::snprintf(line, sizeof(line), "Frame: %lu", static_cast<unsigned long>(frame_++));
        font.draw_string(fb, 8, y, line, kCursor, kBlack);

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
    uint32_t frame_ = 0;
};

DiagScreen g_diag_screen;

}  // namespace

int main() {
    stdio_init_all();

    g_init_status = platform::init();
    run_self_tests();

    // Display rendering is synchronous on core 0 (the dual-core display
    // handshake hangs on hardware — see D10). Core 1 is left idle.
    platform::display().set_backlight(200);

    apps::home_screen().load_state();
    apps::load_graph_state();

    auto& mgr = ui::screen_manager();
    mgr.push(&apps::home_screen());

    // RP2350 cold boot: the peripheral rail needs ~5-8 s to settle, so
    // the boot-time PSRAM/SD init can fail on a cold power-on (D14 —
    // measured 2026-07-11; warm reboots and Pico 1 are unaffected).
    // Boot stays instant; the loop below retries until they come up or
    // the window closes. A failing SD attempt with a card inserted
    // blocks up to ~1 s, so retries are spaced out.
    //
    // Retries cover the self-tests too, not just init: in the marginal
    // window init can "succeed" while readback is still garbage, and a
    // one-shot retest right after a late reinit can fail the same way —
    // either case used to freeze FAIL on the diag screen for good
    // (observed on HW 2026-07-17).
    constexpr uint32_t kLateInitWindowMs = 30'000;
    constexpr uint32_t kLateInitGapMs = 2'000;
    uint32_t late_init_last_ms = 0;

    // Event-driven rendering: a full-frame push is ~200 ms, so redraw only
    // after input (or the initial frame) instead of every loop iteration.
    bool dirty = true;
    while (true) {
        const bool psram_healthy = g_init_status.psram && g_psram_alloc_ok;
        const bool sd_healthy = g_init_status.storage && g_sd_test == SdTest::kOk;
        if (!psram_healthy || !sd_healthy) {
            const uint32_t now = platform::uptime_ms();
            if (now < kLateInitWindowMs && now - late_init_last_ms >= kLateInitGapMs) {
                late_init_last_ms = now;
                if (!g_init_status.psram && platform::psram().reinit()) {
                    g_init_status.psram = true;
                    printf("late-init: psram up at %lu ms\n", static_cast<unsigned long>(now));
                }
                if (!g_init_status.storage && platform::storage().init()) {
                    g_init_status.storage = true;
                    // Persistence arrived late: load what boot couldn't.
                    apps::home_screen().load_state();
                    apps::load_graph_state();
                    printf("late-init: storage up at %lu ms, state loaded\n",
                           static_cast<unsigned long>(now));
                }
                run_self_tests();
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
                    if (ui::Screen* s = mgr.current()) {
                        s->invalidate_band(0, ui::kStatusBarH);
                    }
                    dirty = true;
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
            if (!ev.pressed) {
                continue;
            }
            // F6 toggles the hardware diagnostics overlay from any screen.
            if (ev.key == platform::Key::kF6) {
                if (mgr.current() == &g_diag_screen) {
                    mgr.pop();
                } else {
                    mgr.push(&g_diag_screen);
                }
            } else if (ev.key == platform::Key::kHome && mgr.current() != &apps::home_screen()) {
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
