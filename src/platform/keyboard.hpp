#pragma once

#include <cstdint>

namespace platform {

// Logical key codes — mapped from the STM32 co-processor's I2C reports.
// The STM32 firmware pre-applies Shift for printable keys (it reports
// ASCII), so letter keys arrive already cased; KeyEvent::ch preserves it.
enum class Key : uint8_t {
    kNone = 0,

    // Digits and decimal
    k0,
    k1,
    k2,
    k3,
    k4,
    k5,
    k6,
    k7,
    k8,
    k9,
    kDot,

    // Operators
    kPlus,
    kMinus,
    kMultiply,
    kDivide,
    kPower,
    kLParen,
    kRParen,
    kComma,
    kEquals,

    // Letters (A-Z, case-folded; see KeyEvent::ch for the typed case)
    kA,
    kB,
    kC,
    kD,
    kE,
    kF,
    kG,
    kH,
    kI,
    kJ,
    kK,
    kL,
    kM,
    kN,
    kO,
    kP,
    kQ,
    kR,
    kS,
    kT,
    kU,
    kV,
    kW,
    kX,
    kY,
    kZ,

    // Other printable (punctuation etc. — use KeyEvent::ch)
    kPrintable,

    // Navigation / editing
    kUp,
    kDown,
    kLeft,
    kRight,
    kEnter,
    kBackspace,
    kDel,
    kEscape,
    kTab,
    kSpace,
    kHome,
    kInsert,

    // Function keys. F1-F5 are physical; F6-F9 arrive via Shift+F1-F4,
    // which the STM32 translates into scan codes 0x86-0x89 before we
    // see them. F10 is decoded (0x8A) but the STM32 firmware never
    // emits it — Shift+F5 arrives as plain F5.
    // NB: the STM32 swallows Shift on arrow keys entirely (only the
    // kShift event arrives, no arrow); Alt and Ctrl pass through with
    // their flags intact. All re-verified on fw v1.6 (HW 2026-07-11).
    kF1,
    kF2,
    kF3,
    kF4,
    kF5,
    kF6,
    kF7,
    kF8,
    kF9,
    kF10,

    // Modifiers (reported as their own press/release events)
    kShift,
    kCtrl,
    kAlt,
    kSym,
    kCapsLock,

    // Virtual modes (managed by the UI layer, not the hardware)
    kSecond,
    kAlpha,
};

struct KeyEvent {
    Key key = Key::kNone;
    char ch = 0;           // Printable ASCII as typed, else 0
    bool pressed = false;  // true = press, false = release
    bool shift_held = false;
    bool ctrl_held = false;
    bool alt_held = false;
};

// Non-blocking wrapper around the STM32 I2C keyboard protocol.
//
// The vendored Coyote OS read_i2c_kbd() sleeps 16 ms between the register
// select and the data read; this wrapper runs the same protocol as a
// two-phase state machine so poll() never blocks (decision D7).
class Keyboard {
public:
    void init();

    // Call once per main-loop iteration. Returns the next key event, or
    // an event with key == Key::kNone when there is nothing new.
    KeyEvent poll();

    // True when no register-select is in flight. Other STM32 traffic
    // (e.g. the battery register) must only run while idle — a read
    // injected between poll()'s two phases would be misinterpreted as
    // FIFO data and produce phantom key events.
    bool bus_idle() const { return phase_ == Phase::kIdle; }

    // True when the most recent *completed* FIFO read found no event.
    // A kNone return from poll() usually just means a read is in
    // flight (the two-phase machine spends >=10 ms per cycle), so a
    // caller draining a key backlog must not stop on kNone alone —
    // keep polling until this reports empty (HW 2026-07-18 offline
    // spin: breaking on the first kNone capped draining at one event
    // per frame and held-key backlogs played out after release).
    bool fifo_empty() const { return fifo_empty_; }

    bool is_held(Key k) const;

    // Printable character for an event (0 if not printable).
    static char to_char(const KeyEvent& ev) { return ev.ch; }

private:
    enum class Phase : uint8_t { kIdle, kAwaitData };

    Phase phase_ = Phase::kIdle;
    uint64_t phase_deadline_us_ = 0;
    bool fifo_empty_ = true;
    bool shift_held_ = false;
    bool ctrl_held_ = false;
    bool alt_held_ = false;
    bool held_[128] = {};  // Indexed by static_cast<int>(Key)

    KeyEvent decode(uint8_t state, uint8_t code);
};

Keyboard& keyboard();

}  // namespace platform
