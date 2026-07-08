// PicoCalc Graphing Calculator — entry point.
//
// Core 0: application (input, logic, rendering into the framebuffer).
// Core 1: display service (RGB565->666 conversion + SPI DMA push).
//
// Milestone 1 state: boots to a hardware diagnostics screen that
// exercises display, keyboard, PSRAM, and SD card. Replaced by the
// calculator home screen in milestone 2.

#include <cstdio>
#include <cstring>

#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "config.hpp"
#include "platform/platform.hpp"
#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/screen_manager.hpp"

namespace {

platform::InitStatus g_init_status;

// SD read/write self-test result (task 1.5 acceptance).
enum class SdTest { kNoCard, kFailed, kOk };
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
        const uint32_t addr = ps.alloc(1024);
        if (addr != platform::Psram::kInvalid) {
            uint8_t pattern[1024];
            uint8_t readback[1024];
            for (int i = 0; i < 1024; ++i) {
                pattern[i] = static_cast<uint8_t>(i * 7 + 13);
            }
            ps.write(addr, pattern, sizeof(pattern));
            ps.read(addr, readback, sizeof(readback));
            g_psram_alloc_ok = std::memcmp(pattern, readback, sizeof(pattern)) == 0;
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
        last_key_ = ev;
        ++key_count_;
        printf("key: code=%d ch='%c' shift=%d ctrl=%d\n", static_cast<int>(ev.key),
               ev.ch != 0 ? ev.ch : ' ', ev.shift_held, ev.ctrl_held);
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

    multicore_launch_core1(gfx::display_service_main);

    platform::display().set_backlight(200);

    auto& mgr = ui::screen_manager();
    mgr.push(&g_diag_screen);

    while (true) {
        const platform::KeyEvent ev = platform::keyboard().poll();
        if (ev.key != platform::Key::kNone) {
            mgr.handle_key(ev);
        }
        mgr.render_frame();
    }
}
