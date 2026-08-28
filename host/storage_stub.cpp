// platform::Storage reporting "no card" (Phase 6.4.0).
//
// The real POSIX backend mapping /picocalc to ~/.picocalc is 6.4.2. Until
// then this must fail honestly rather than half-work: every persisted-state
// load in the startup path is all-or-nothing and already handles storage
// being down (it is the ordinary case on a cold RP2350 boot, D14/D26), so
// an unmounted card is a state the shared tree is known to survive. A stub
// that silently succeeded and returned empty files would instead look like
// a card full of zero-length state.
//
// Replaced wholesale in 6.4.2 -- nothing here is worth carrying forward.

#include "platform/storage.hpp"

namespace platform {

bool Storage::init() {
    mounted_ = false;
    return false;
}

void Storage::on_card_removed() {
    mounted_ = false;
}

bool Storage::file_exists(const char* /*path*/) const {
    return false;
}

long Storage::file_size(const char* /*path*/) const {
    return -1;
}

int Storage::read_file(const char* /*path*/, uint8_t* /*buf*/, size_t /*max_len*/) const {
    return -1;
}

int Storage::read_file_range(const char* /*path*/, size_t /*offset*/, uint8_t* /*buf*/,
                             size_t /*max_len*/) const {
    return -1;
}

bool Storage::write_file(const char* /*path*/, const uint8_t* /*buf*/, size_t /*len*/) const {
    return false;
}

bool Storage::append_file(const char* /*path*/, const uint8_t* /*buf*/, size_t /*len*/) const {
    return false;
}

bool Storage::delete_file(const char* /*path*/) const {
    return false;
}

bool Storage::ensure_dir(const char* /*path*/) const {
    return false;
}

bool Storage::rename_file(const char* /*old_path*/, const char* /*new_path*/) const {
    return false;
}

bool Storage::delete_dir(const char* /*path*/) const {
    return false;
}

int Storage::list_dir(const char* /*path*/, DirEntry* /*entries*/, int /*max_entries*/,
                      int /*skip*/) const {
    return -1;
}

bool Storage::read_string(const char* /*path*/, char* /*buf*/, size_t /*max_len*/) const {
    return false;
}

bool Storage::write_string(const char* /*path*/, const char* /*str*/) const {
    return false;
}

Storage& storage() {
    static Storage instance;
    return instance;
}

}  // namespace platform
