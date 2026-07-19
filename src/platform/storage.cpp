#include "platform/storage.hpp"

#include <cstring>

#include "platform/sd_card.hpp"

extern "C" {
#include "ff.h"
}

namespace platform {

namespace {
FATFS g_fs;
}

bool Storage::init() {
    mounted_ = false;
    if (!sd::card_present()) {
        return false;
    }
    if (f_mount(&g_fs, "", 1) != FR_OK) {
        return false;
    }
    mounted_ = true;
    return true;
}

void Storage::on_card_removed() {
    f_unmount("");
    mounted_ = false;
    sd::invalidate();
}

bool Storage::file_exists(const char* path) const {
    if (!mounted_) {
        return false;
    }
    FILINFO info;
    return f_stat(path, &info) == FR_OK && !(info.fattrib & AM_DIR);
}

int Storage::read_file(const char* path, uint8_t* buf, size_t max_len) const {
    if (!mounted_) {
        return -1;
    }
    FIL f;
    if (f_open(&f, path, FA_READ) != FR_OK) {
        return -1;
    }
    UINT read = 0;
    const FRESULT rc = f_read(&f, buf, max_len, &read);
    f_close(&f);
    return rc == FR_OK ? static_cast<int>(read) : -1;
}

int Storage::read_file_range(const char* path, size_t offset, uint8_t* buf, size_t max_len) const {
    if (!mounted_) {
        return -1;
    }
    FIL f;
    if (f_open(&f, path, FA_READ) != FR_OK) {
        return -1;
    }
    if (f_lseek(&f, offset) != FR_OK) {
        f_close(&f);
        return -1;
    }
    UINT read = 0;
    const FRESULT rc = f_read(&f, buf, max_len, &read);
    f_close(&f);
    return rc == FR_OK ? static_cast<int>(read) : -1;
}

bool Storage::write_file(const char* path, const uint8_t* buf, size_t len) const {
    if (!mounted_) {
        return false;
    }
    FIL f;
    if (f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        return false;
    }
    UINT written = 0;
    const FRESULT rc = f_write(&f, buf, len, &written);
    f_close(&f);
    return rc == FR_OK && written == len;
}

bool Storage::append_file(const char* path, const uint8_t* buf, size_t len) const {
    if (!mounted_) {
        return false;
    }
    FIL f;
    if (f_open(&f, path, FA_WRITE | FA_OPEN_APPEND) != FR_OK) {
        return false;
    }
    UINT written = 0;
    const FRESULT rc = f_write(&f, buf, len, &written);
    f_close(&f);
    return rc == FR_OK && written == len;
}

bool Storage::delete_file(const char* path) const {
    if (!mounted_) {
        return false;
    }
    return f_unlink(path) == FR_OK;
}

bool Storage::ensure_dir(const char* path) const {
    if (!mounted_) {
        return false;
    }
    const FRESULT rc = f_mkdir(path);
    return rc == FR_OK || rc == FR_EXIST;
}

int Storage::list_dir(const char* path, DirEntry* entries, int max_entries) const {
    if (!mounted_) {
        return -1;
    }
    DIR dir;
    if (f_opendir(&dir, path) != FR_OK) {
        return -1;
    }
    int n = 0;
    FILINFO info;
    while (n < max_entries && f_readdir(&dir, &info) == FR_OK && info.fname[0] != 0) {
        std::strncpy(entries[n].name, info.fname, sizeof(entries[n].name) - 1);
        entries[n].name[sizeof(entries[n].name) - 1] = 0;
        entries[n].is_dir = (info.fattrib & AM_DIR) != 0;
        entries[n].size = info.fsize;
        ++n;
    }
    f_closedir(&dir);
    return n;
}

bool Storage::read_string(const char* path, char* buf, size_t max_len) const {
    if (max_len == 0) {
        return false;
    }
    const int n = read_file(path, reinterpret_cast<uint8_t*>(buf), max_len - 1);
    if (n < 0) {
        return false;
    }
    buf[n] = 0;
    return true;
}

bool Storage::write_string(const char* path, const char* str) const {
    return write_file(path, reinterpret_cast<const uint8_t*>(str), std::strlen(str));
}

Storage& storage() {
    static Storage instance;
    return instance;
}

}  // namespace platform
