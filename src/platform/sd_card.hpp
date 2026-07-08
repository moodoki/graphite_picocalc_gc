#pragma once

#include <cstdint>

namespace platform::sd {

// Minimal SD card access in SPI mode (spi0, PicoCalc mainboard pinout
// from drivers/coyote_reference/config.h). Used by the FatFs diskio
// glue in sd_diskio.cpp; application code goes through Storage.

bool init();  // Card init sequence; false if absent/failed
bool initialized();
bool read_block(uint32_t lba, uint8_t* dst);         // 512 bytes
bool write_block(uint32_t lba, const uint8_t* src);  // 512 bytes
uint32_t sector_count();                             // Total 512-byte sectors (0 if unknown)
bool card_present();                                 // Via SD_DET pin

}  // namespace platform::sd
