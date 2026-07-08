// FatFs media access layer, bridging to platform::sd. Replaces the stub
// drivers/fatfs/diskio.c (which is not compiled — see CMakeLists.txt).

#include "platform/sd_card.hpp"

extern "C" {
#include "ff.h"

#include "diskio.h"
}

extern "C" DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    return platform::sd::initialized() ? 0 : STA_NOINIT;
}

extern "C" DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    return platform::sd::init() ? 0 : STA_NOINIT;
}

extern "C" DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0) {
        return RES_PARERR;
    }
    for (UINT i = 0; i < count; ++i) {
        if (!platform::sd::read_block(sector + i, buff + 512u * i)) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

extern "C" DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0) {
        return RES_PARERR;
    }
    for (UINT i = 0; i < count; ++i) {
        if (!platform::sd::write_block(sector + i, buff + 512u * i)) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

extern "C" DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv != 0) {
        return RES_PARERR;
    }
    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;  // Writes are synchronous
        case GET_SECTOR_COUNT:
            *static_cast<LBA_t*>(buff) = platform::sd::sector_count();
            return RES_OK;
        case GET_BLOCK_SIZE:
            *static_cast<DWORD*>(buff) = 1;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

// No RTC in Phase 1: fixed timestamp (2026-01-01 00:00:00).
extern "C" DWORD get_fattime(void) {
    return ((DWORD)(2026 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
}
