// platform::Keyboard with no source of keys (Phase 6.4.0).
//
// The spike renders one frame and exits, so nothing types. What matters
// here is the *contract*, because it is easy to get subtly wrong and the
// bug looks like a hang: poll() returning kNone does NOT mean the queue
// is empty on hardware -- the STM32 read is a two-phase machine and kNone
// usually means "read in flight" (keyboard.hpp:147-154). Callers
// therefore drain until fifo_empty() is true, so a host backend with
// nothing to give must report fifo_empty() == true or the drain loop in
// main spins for its whole 250 ms budget every frame.
//
// fifo_empty_ defaults to true in the header, and this file never clears
// it. 6.4.4's key scripts and 6.4.5's SDL backend push events here.

#include "platform/keyboard.hpp"

namespace platform {

void Keyboard::init() {}

KeyEvent Keyboard::poll() {
    return KeyEvent{};
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
