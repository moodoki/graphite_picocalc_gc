#include "platform/display.hpp"

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"

extern "C" {
#include "i2ckbd/i2ckbd.h"
#include "lcdspi/lcdspi.h"
// Not exported in lcdspi.h but non-static in lcdspi.c.
void define_region_spi(int xstart, int ystart, int xend, int yend, int rw);
}

namespace platform {

namespace {

// RGB565 -> RGB666 expansion tables (5- and 6-bit channel to 8-bit,
// upper 6 bits significant to the panel).
uint8_t lut5[32];
uint8_t lut6[64];

void init_luts() {
    for (int i = 0; i < 32; ++i) {
        lut5[i] = static_cast<uint8_t>((i << 3) | (i >> 2));
    }
    for (int i = 0; i < 64; ++i) {
        lut6[i] = static_cast<uint8_t>((i << 2) | (i >> 4));
    }
}

// Convert a run of RGB565 pixels to 3-byte RGB666 wire format.
// RAM-resident: the dual-core display service (D10 revival) runs this on
// core 1, and executing it from flash (XIP) contends with core 0's USB
// stack — see the 2026-07-25 worklog. Matches lcdspi's spi_write_fast.
void __not_in_flash_func(convert_565_666)(const uint16_t* src, uint8_t* dst, int count) {
    for (int i = 0; i < count; ++i) {
        const uint16_t p = src[i];
        *dst++ = lut5[(p >> 11) & 0x1F];  // R
        *dst++ = lut6[(p >> 5) & 0x3F];   // G
        *dst++ = lut5[p & 0x1F];          // B
    }
}

// Chunked DMA staging: two ping-pong buffers of 4 scanlines each.
constexpr int kChunkLines = 4;
constexpr int kChunkPixels = kScreenW * kChunkLines;
uint8_t staging[2][kChunkPixels * 3];

int dma_chan = -1;

void __not_in_flash_func(dma_push)(const uint8_t* buf, int bytes) {
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, spi_get_dreq(Pico_LCD_SPI_MOD, true));
    dma_channel_configure(dma_chan, &cfg, &spi_get_hw(Pico_LCD_SPI_MOD)->dr, buf, bytes, true);
}

void __not_in_flash_func(dma_wait)() {
    if (dma_chan >= 0) {
        dma_channel_wait_for_finish_blocking(dma_chan);
    }
}

}  // namespace

void Display::init() {
    init_luts();
    lcd_init();  // Vendored: SPI + GPIO setup and ST7365P init sequence
    if (dma_chan < 0) {
        dma_chan = dma_claim_unused_channel(true);
    }
    initialized_ = true;
}

void Display::push_rect(int x, int y, int w, int h, const uint16_t* px) {
    if (w <= 0 || h <= 0) {
        return;
    }
    define_region_spi(x, y, x + w - 1, y + h - 1, 1);
    // Convert and send one chunk at a time (blocking SPI).
    int remaining = w * h;
    const uint16_t* src = px;
    while (remaining > 0) {
        const int n = remaining < kChunkPixels ? remaining : kChunkPixels;
        convert_565_666(src, staging[0], n);
        spi_write_fast(Pico_LCD_SPI_MOD, staging[0], n * 3);
        src += n;
        remaining -= n;
    }
    spi_finish(Pico_LCD_SPI_MOD);
    lcd_spi_raise_cs();
}

void __not_in_flash_func(Display::push_rect_dma)(int x, int y, int w, int h, const uint16_t* px) {
    if (w <= 0 || h <= 0) {
        return;
    }
    define_region_spi(x, y, x + w - 1, y + h - 1, 1);
    int remaining = w * h;
    const uint16_t* src = px;
    int buf = 0;
    bool dma_active = false;
    while (remaining > 0) {
        const int n = remaining < kChunkPixels ? remaining : kChunkPixels;
        convert_565_666(src, staging[buf], n);
        if (dma_active) {
            dma_wait();
        }
        dma_push(staging[buf], n * 3);
        dma_active = true;
        buf ^= 1;
        src += n;
        remaining -= n;
    }
    dma_wait();
    spi_finish(Pico_LCD_SPI_MOD);
    lcd_spi_raise_cs();
}

void Display::set_backlight(uint8_t level) {
    // STM32 register 0x05 = LCD backlight; write form sets bit 7.
    uint8_t msg[2] = {static_cast<uint8_t>(0x05 | 0x80), level};
    i2c_write_timeout_us(I2C_KBD_MOD, I2C_KBD_ADDR, msg, 2, false, 100000);
}

Display& display() {
    static Display instance;
    return instance;
}

}  // namespace platform
