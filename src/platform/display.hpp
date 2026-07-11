#pragma once

#include <cstdint>

namespace platform {

// 320x320 RGB565
constexpr int kScreenW = 320;
constexpr int kScreenH = 320;

struct Color {
    uint16_t rgb565;

    static constexpr Color from_rgb(uint8_t r, uint8_t g, uint8_t b) {
        return {static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))};
    }
};

namespace colors {
constexpr Color kBlack = {0x0000};
constexpr Color kWhite = {0xFFFF};
constexpr Color kBlue = Color::from_rgb(0, 0, 255);
constexpr Color kRed = Color::from_rgb(255, 0, 0);
constexpr Color kGreen = Color::from_rgb(0, 200, 0);
constexpr Color kGrayBg = Color::from_rgb(240, 240, 240);
constexpr Color kGrayLine = Color::from_rgb(200, 200, 200);
// Graph grid: dark enough to recede behind plots on the black background
// (kGrayLine reads near-white on the panel — 2026-07-11 test drive).
constexpr Color kGridLine = Color::from_rgb(60, 60, 60);
constexpr Color kCursor = Color::from_rgb(0, 120, 215);
}  // namespace colors

// Low-level LCD transport. Pixel buffering and drawing primitives live in
// gfx::Framebuffer; this class owns panel bring-up and pixel push.
//
// Wire format note: the ST7365P panel is initialized (by the vendored
// Coyote OS driver) with COLMOD 0x66 — 18-bit color, 3 bytes/pixel over
// SPI. RGB565 buffers are converted to RGB666 during push. See decision
// D6 in docs/notes/decisions.md.
class Display {
public:
    void init();

    // Blocking push of an RGB565 rectangle to the panel. Safe from either
    // core, but must not overlap with an in-flight DMA push.
    void push_rect(int x, int y, int w, int h, const uint16_t* px);

    // DMA pipeline (used by gfx display service on core 1):
    // convert+push a rectangle using chunked DMA. Blocks until complete,
    // but overlaps conversion with SPI transfer.
    void push_rect_dma(int x, int y, int w, int h, const uint16_t* px);

    // LCD backlight via the STM32 south bridge (I2C reg 0x05), 0-255.
    // Requires Keyboard/I2C bus to be initialized first.
    void set_backlight(uint8_t level);

    bool initialized() const { return initialized_; }

private:
    bool initialized_ = false;
};

Display& display();

}  // namespace platform
