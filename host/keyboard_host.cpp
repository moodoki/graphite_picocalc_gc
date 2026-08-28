// platform::Keyboard fed from a queue instead of a keypad (Phase 6.4.0,
// queue added 6.4.3).
//
// The contract is easy to get subtly wrong, and the bug looks like a hang:
// poll() returning kNone does NOT mean the queue is empty on hardware. The
// STM32 read is a two-phase state machine, so kNone usually means "read in
// flight" (keyboard.hpp:147-154) and callers drain until fifo_empty() is
// true. A host backend with nothing to give must therefore report
// fifo_empty() == true, or the drain loop spins its whole 250 ms budget
// every frame.
//
// This is the queue half of D97's key scripts. The parser and the file
// format are 6.4.4; what is here is the thing they will push into, plus
// enough of a name table to drive it from the command line. It exists this
// early because 6.4.3's own gate needed it: the periodic-table app loads,
// draws, and then blocks in calc_wait_key forever, which is correct
// behaviour with no keyboard attached.

#include "host/keyboard_host.hpp"

#include <cstring>
#include <deque>

#include "platform/keyboard.hpp"

namespace {

std::deque<platform::KeyEvent> g_queue;

}  // namespace

namespace host {

// D97 says to reuse the name table micropython_embed.cpp already has, for
// the reason D87 gave: two tables that answer "which key is this" diverge,
// and here the divergence would be silent, because a wrong name produces a
// *plausible* screenshot rather than an error.
//
// That table is in an anonymous namespace inside a MicroPython translation
// unit, so reusing it means extracting it first -- and it covers only
// navigation plus F1-F6, no letters or digits. Both are 6.4.4's problem.
// Until then this handles the named keys and passes anything else through
// as a printable character, which is what a key script mostly needs.
bool queue_key(const char* name) {
    if (name == nullptr || name[0] == 0) {
        return false;
    }
    struct Named {
        const char* name;
        platform::Key key;
    };
    static const Named kNames[] = {
        {"up", platform::Key::kUp},          {"down", platform::Key::kDown},
        {"left", platform::Key::kLeft},      {"right", platform::Key::kRight},
        {"enter", platform::Key::kEnter},    {"esc", platform::Key::kEscape},
        {"space", platform::Key::kSpace},    {"tab", platform::Key::kTab},
        {"back", platform::Key::kBackspace}, {"del", platform::Key::kDel},
        {"home", platform::Key::kHome},      {"f1", platform::Key::kF1},
        {"f2", platform::Key::kF2},          {"f3", platform::Key::kF3},
        {"f4", platform::Key::kF4},          {"f5", platform::Key::kF5},
        {"f6", platform::Key::kF6},
    };

    platform::KeyEvent ev;
    for (const Named& n : kNames) {
        if (std::strcmp(name, n.name) == 0) {
            ev.key = n.key;
            break;
        }
    }
    if (ev.key == platform::Key::kNone) {
        if (std::strlen(name) != 1) {
            return false;
        }
        ev.key = platform::Key::kPrintable;
        ev.ch = name[0];
    }

    // Press then release, both queued: the firmware's drain loop ignores
    // releases but power::note_key sees them, and an app that tracks held
    // keys would otherwise believe the key never came up.
    ev.pressed = true;
    g_queue.push_back(ev);
    ev.pressed = false;
    g_queue.push_back(ev);
    return true;
}

bool keys_pending() {
    return !g_queue.empty();
}

}  // namespace host

namespace platform {

void Keyboard::init() {}

KeyEvent Keyboard::poll() {
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
