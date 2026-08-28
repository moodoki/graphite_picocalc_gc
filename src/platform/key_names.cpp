#include "platform/key_names.hpp"

#include <cstring>

namespace platform {

namespace {

struct NamedKey {
    const char* name;
    Key key;
};

// ONE table, read in both directions -- see the header. The function keys
// are here because an app drawing its own softkey bar has no other way to
// read them: they carry no character, so `ch` is 0.
constexpr NamedKey kNamedKeys[] = {
    {"up", Key::kUp},       {"down", Key::kDown},   {"left", Key::kLeft},
    {"right", Key::kRight}, {"enter", Key::kEnter}, {"esc", Key::kEscape},
    {"space", Key::kSpace}, {"tab", Key::kTab},     {"back", Key::kBackspace},
    {"del", Key::kDel},     {"home", Key::kHome},   {"f1", Key::kF1},
    {"f2", Key::kF2},       {"f3", Key::kF3},       {"f4", Key::kF4},
    {"f5", Key::kF5},       {"f6", Key::kF6},
};

}  // namespace

const char* key_name(Key key) {
    for (const NamedKey& n : kNamedKeys) {
        if (n.key == key) {
            return n.name;
        }
    }
    return "";
}

Key key_from_char(char c) {
    if (c >= 'a' && c <= 'z') {
        return static_cast<Key>(static_cast<int>(Key::kA) + (c - 'a'));
    }
    if (c >= 'A' && c <= 'Z') {
        return static_cast<Key>(static_cast<int>(Key::kA) + (c - 'A'));
    }
    if (c >= '0' && c <= '9') {
        return static_cast<Key>(static_cast<int>(Key::k0) + (c - '0'));
    }
    return c == 0 ? Key::kNone : Key::kPrintable;
}

Key key_from_name(const char* name) {
    if (name == nullptr) {
        return Key::kNone;
    }
    for (const NamedKey& n : kNamedKeys) {
        if (std::strcmp(name, n.name) == 0) {
            return n.key;
        }
    }
    return Key::kNone;
}

}  // namespace platform
