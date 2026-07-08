#include "platform/sd_card.hpp"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/time.h"

namespace platform::sd {

namespace {

// PicoCalc mainboard SD pinout (drivers/coyote_reference/config.h).
constexpr uint kPinSck = 18;
constexpr uint kPinMosi = 19;
constexpr uint kPinMiso = 16;
constexpr uint kPinCs = 17;
constexpr uint kPinDet = 22;
spi_inst_t* const kSpi = spi0;

constexpr uint32_t kInitBaud = 400 * 1000;
constexpr uint32_t kRunBaud = 12500 * 1000;

// SD commands (SPI mode)
constexpr uint8_t kCmd0 = 0;     // GO_IDLE_STATE
constexpr uint8_t kCmd8 = 8;     // SEND_IF_COND
constexpr uint8_t kCmd9 = 9;     // SEND_CSD
constexpr uint8_t kCmd16 = 16;   // SET_BLOCKLEN
constexpr uint8_t kCmd17 = 17;   // READ_SINGLE_BLOCK
constexpr uint8_t kCmd24 = 24;   // WRITE_BLOCK
constexpr uint8_t kCmd55 = 55;   // APP_CMD
constexpr uint8_t kCmd58 = 58;   // READ_OCR
constexpr uint8_t kAcmd41 = 41;  // SD_SEND_OP_COND

bool g_initialized = false;
bool g_sdhc = false;  // Block (vs byte) addressing
uint32_t g_sectors = 0;

void cs_low() {
    gpio_put(kPinCs, 0);
}
void cs_high() {
    gpio_put(kPinCs, 1);
    uint8_t ff = 0xFF;  // Extra clocks so the card releases MISO
    spi_write_blocking(kSpi, &ff, 1);
}

uint8_t xfer(uint8_t out) {
    uint8_t in = 0;
    spi_write_read_blocking(kSpi, &out, &in, 1);
    return in;
}

bool wait_ready(uint32_t timeout_ms) {
    const absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (!time_reached(deadline)) {
        if (xfer(0xFF) == 0xFF) {
            return true;
        }
    }
    return false;
}

// Send a command, return R1 (or 0xFF on timeout).
uint8_t command(uint8_t cmd, uint32_t arg) {
    if (cmd != kCmd0 && !wait_ready(300)) {
        return 0xFF;
    }
    uint8_t frame[6] = {
        static_cast<uint8_t>(0x40 | cmd),
        static_cast<uint8_t>(arg >> 24),
        static_cast<uint8_t>(arg >> 16),
        static_cast<uint8_t>(arg >> 8),
        static_cast<uint8_t>(arg),
        // Valid CRCs required only for CMD0/CMD8 before SPI mode settles.
        static_cast<uint8_t>(cmd == kCmd0   ? 0x95
                             : cmd == kCmd8 ? 0x87
                                            : 0x01),
    };
    spi_write_blocking(kSpi, frame, 6);
    for (int i = 0; i < 10; ++i) {
        const uint8_t r = xfer(0xFF);
        if (!(r & 0x80)) {
            return r;
        }
    }
    return 0xFF;
}

bool read_csd_sectors() {
    cs_low();
    if (command(kCmd9, 0) != 0) {
        cs_high();
        return false;
    }
    // Wait for data token 0xFE
    for (int i = 0; i < 1000; ++i) {
        if (xfer(0xFF) == 0xFE) {
            uint8_t csd[16];
            for (auto& b : csd) {
                b = xfer(0xFF);
            }
            xfer(0xFF);  // CRC
            xfer(0xFF);
            cs_high();
            if ((csd[0] >> 6) == 1) {  // CSD v2 (SDHC/SDXC)
                const uint32_t c_size =
                    (static_cast<uint32_t>(csd[7] & 0x3F) << 16) | (csd[8] << 8) | csd[9];
                g_sectors = (c_size + 1) * 1024;
            } else {  // CSD v1
                const uint32_t c_size = ((csd[6] & 0x03) << 10) | (csd[7] << 2) | (csd[8] >> 6);
                const uint32_t c_mult = ((csd[9] & 0x03) << 1) | (csd[10] >> 7);
                const uint32_t read_bl_len = csd[5] & 0x0F;
                g_sectors = (c_size + 1) * (1u << (c_mult + 2)) * (1u << read_bl_len) / 512;
            }
            return true;
        }
    }
    cs_high();
    return false;
}

}  // namespace

bool card_present() {
    // DET pin: low = card inserted (switch to GND), pulled up otherwise.
    return !gpio_get(kPinDet);
}

bool init() {
    g_initialized = false;

    gpio_init(kPinCs);
    gpio_set_dir(kPinCs, GPIO_OUT);
    gpio_put(kPinCs, 1);
    gpio_init(kPinDet);
    gpio_set_dir(kPinDet, GPIO_IN);
    gpio_pull_up(kPinDet);

    spi_init(kSpi, kInitBaud);
    gpio_set_function(kPinSck, GPIO_FUNC_SPI);
    gpio_set_function(kPinMosi, GPIO_FUNC_SPI);
    gpio_set_function(kPinMiso, GPIO_FUNC_SPI);
    gpio_pull_up(kPinMiso);

    if (!card_present()) {
        return false;
    }

    // >74 dummy clocks with CS high
    for (int i = 0; i < 10; ++i) {
        xfer(0xFF);
    }

    cs_low();
    // CMD0: enter idle state
    bool idle = false;
    for (int i = 0; i < 10 && !idle; ++i) {
        idle = (command(kCmd0, 0) == 0x01);
    }
    if (!idle) {
        cs_high();
        return false;
    }

    // CMD8: voltage check — distinguishes v2 cards
    const bool v2 = (command(kCmd8, 0x1AA) == 0x01);
    if (v2) {
        for (int i = 0; i < 4; ++i) {
            xfer(0xFF);  // Discard R7 payload
        }
    }

    // ACMD41 until ready (HCS set for v2 cards)
    const absolute_time_t deadline = make_timeout_time_ms(1000);
    uint8_t r = 0xFF;
    do {
        command(kCmd55, 0);
        r = command(kAcmd41, v2 ? (1u << 30) : 0);
    } while (r != 0x00 && !time_reached(deadline));
    if (r != 0x00) {
        cs_high();
        return false;
    }

    // CMD58: OCR — check CCS for block addressing
    g_sdhc = false;
    if (command(kCmd58, 0) == 0x00) {
        uint8_t ocr[4];
        for (auto& b : ocr) {
            b = xfer(0xFF);
        }
        g_sdhc = (ocr[0] & 0x40) != 0;
    }
    if (!g_sdhc) {
        command(kCmd16, 512);
    }
    cs_high();

    spi_set_baudrate(kSpi, kRunBaud);
    read_csd_sectors();
    g_initialized = true;
    return true;
}

bool initialized() {
    return g_initialized;
}

uint32_t sector_count() {
    return g_sectors;
}

bool read_block(uint32_t lba, uint8_t* dst) {
    if (!g_initialized) {
        return false;
    }
    const uint32_t addr = g_sdhc ? lba : lba * 512;
    cs_low();
    if (command(kCmd17, addr) != 0) {
        cs_high();
        return false;
    }
    // Wait for data token
    const absolute_time_t deadline = make_timeout_time_ms(300);
    uint8_t tok = 0xFF;
    while ((tok = xfer(0xFF)) == 0xFF) {
        if (time_reached(deadline)) {
            cs_high();
            return false;
        }
    }
    if (tok != 0xFE) {
        cs_high();
        return false;
    }
    spi_read_blocking(kSpi, 0xFF, dst, 512);
    xfer(0xFF);  // CRC
    xfer(0xFF);
    cs_high();
    return true;
}

bool write_block(uint32_t lba, const uint8_t* src) {
    if (!g_initialized) {
        return false;
    }
    const uint32_t addr = g_sdhc ? lba : lba * 512;
    cs_low();
    if (command(kCmd24, addr) != 0) {
        cs_high();
        return false;
    }
    xfer(0xFF);
    xfer(0xFE);  // Data token
    spi_write_blocking(kSpi, src, 512);
    xfer(0xFF);  // Dummy CRC
    xfer(0xFF);
    const uint8_t resp = xfer(0xFF);
    if ((resp & 0x1F) != 0x05) {  // Data accepted
        cs_high();
        return false;
    }
    const bool ok = wait_ready(500);
    cs_high();
    return ok;
}

}  // namespace platform::sd
