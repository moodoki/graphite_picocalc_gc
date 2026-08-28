// Key scripts: a text file of key names replayed into the host keyboard
#include <cctype>
// queue (D97, Phase 6.4.4).
//
// This is what makes the renderer able to photograph a screen that has to
// be NAVIGATED to rather than constructed -- which is most of them. #52's
// truncated softkey row is inside the file manager; the chrome sweep needs
// modal states (a cut armed, a subsystem unhealthy) that are nothing but
// key sequences.
//
// The names come from platform::key_names, deliberately, and not from a
// table here. That is D87's argument reused: two tables that answer "which
// key is this" diverge, and here the divergence would be SILENT, because a
// wrong name in a key script produces a plausible screenshot rather than
// an error.
//
// Format -- whitespace-separated tokens, '#' to end of line is a comment:
//
//     # reach the file manager's softkey row (issue #52)
//     f6 down down enter
//     esc
//
// Newlines carry no meaning beyond separating tokens, so a script can be
// laid out to read like the thing it is doing.

#include "host/keyscript.hpp"

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

#include "platform/key_names.hpp"
#include "platform/keyboard.hpp"

namespace {

std::deque<platform::KeyEvent> g_queue;

}  // namespace

namespace host {

bool queue_key(const char* name) {
    if (name == nullptr || name[0] == 0) {
        return false;
    }

    platform::KeyEvent ev;
    ev.key = platform::key_from_name(name);
    if (ev.key == platform::Key::kNone) {
        if (std::strlen(name) != 1) {
            return false;
        }
        // Both fields, as the real driver sets them: letters and digits
        // have their own enumerators AND carry a character, and a handler
        // switching on `key` would not see an event that only had `ch`.
        ev.key = platform::key_from_char(name[0]);
        ev.ch = name[0];
    }

    // Press then release, both queued. The main drain ignores releases,
    // but power::note_key sees them and is_held() tracks them, so an app
    // reading held keys would otherwise believe the key never came up.
    ev.pressed = true;
    g_queue.push_back(ev);
    ev.pressed = false;
    g_queue.push_back(ev);
    return true;
}

bool run_keyscript(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        std::fprintf(stderr, "keyscript: cannot open %s\n", path);
        return false;
    }
    std::string token;
    int line = 1;
    bool in_comment = false;
    bool ok = true;
    const auto flush = [&]() {
        if (token.empty()) {
            return true;
        }
        if (!queue_key(token.c_str())) {
            std::fprintf(stderr, "keyscript: %s:%d: not a key: \"%s\"\n", path, line,
                         token.c_str());
            return false;
        }
        token.clear();
        return true;
    };
    for (int c = std::fgetc(f); c != EOF; c = std::fgetc(f)) {
        if (c == '\n') {
            ok = flush() && ok;
            in_comment = false;
            ++line;
            continue;
        }
        if (in_comment) {
            continue;
        }
        if (c == '#') {
            ok = flush() && ok;
            in_comment = true;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            ok = flush() && ok;
            continue;
        }
        token.push_back(static_cast<char>(c));
    }
    ok = flush() && ok;
    std::fclose(f);
    return ok;
}

bool keys_pending() {
    return !g_queue.empty();
}

}  // namespace host

namespace platform {

void Keyboard::init() {}

KeyEvent Keyboard::poll() {
    // kNone from poll() does NOT mean "no more keys" on hardware -- the
    // STM32 read is a two-phase machine and kNone usually means a read is
    // in flight, so callers drain until fifo_empty() is true
    // (keyboard.hpp:147-154). A host backend with nothing to give must
    // report fifo_empty() == true, or the drain loop in the main loop
    // spins its entire 250 ms budget every frame.
    if (g_queue.empty()) {
        fifo_empty_ = true;
        return KeyEvent{};
    }
    const KeyEvent ev = g_queue.front();
    g_queue.pop_front();
    fifo_empty_ = g_queue.empty();
    const auto i = static_cast<unsigned>(ev.key);
    if (i < sizeof(held_)) {
        held_[i] = ev.pressed;
    }
    return ev;
}

bool Keyboard::is_held(Key k) const {
    const auto i = static_cast<unsigned>(k);
    return i < sizeof(held_) && held_[i];
}

KeyEvent Keyboard::decode(uint8_t /*state*/, uint8_t /*code*/) {
    // Only reachable from the I2C path, which does not exist here.
    return KeyEvent{};
}

Keyboard& keyboard() {
    static Keyboard instance;
    return instance;
}

}  // namespace platform
