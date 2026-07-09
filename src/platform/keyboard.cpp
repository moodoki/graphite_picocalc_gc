#include "platform/keyboard.hpp"

#include "hardware/i2c.h"
#include "pico/time.h"

extern "C" {
#include "i2ckbd/i2ckbd.h"
}

namespace platform {

namespace {

// STM32 keyboard FIFO register and event states.
constexpr uint8_t kRegFifo = 0x09;
constexpr uint8_t kStatePressed = 1;
constexpr uint8_t kStateHold = 2;
constexpr uint8_t kStateReleased = 3;

// Scan codes from drivers/coyote_reference/keyboard_definition.h plus
// the 0x7E ctrl code observed in the Coyote OS driver.
constexpr uint8_t kCodeAlt = 0xA1;
constexpr uint8_t kCodeShiftL = 0xA2;
constexpr uint8_t kCodeShiftR = 0xA3;
constexpr uint8_t kCodeSym = 0xA4;
constexpr uint8_t kCodeCtrl1 = 0xA5;
constexpr uint8_t kCodeCtrl2 = 0x7E;
constexpr uint8_t kCodeEsc = 0xB1;
constexpr uint8_t kCodeLeft = 0xB4;
constexpr uint8_t kCodeUp = 0xB5;
constexpr uint8_t kCodeDown = 0xB6;
constexpr uint8_t kCodeRight = 0xB7;
constexpr uint8_t kCodeCaps = 0xC1;
constexpr uint8_t kCodeInsert = 0xD1;
constexpr uint8_t kCodeHome = 0xD2;
constexpr uint8_t kCodeDel = 0xD4;

// Delay between FIFO register select and data read. The vendored driver
// uses 16 ms; 10 ms is reliable per community drivers and keeps latency
// acceptable when polled at frame rate.
constexpr uint64_t kReadDelayUs = 10000;

// I2C transfer timeout. The keyboard bus runs at 10 kHz, so a 2-byte
// read takes ~3.5 ms and the register-select write ~2.5 ms — the
// timeout must comfortably exceed those. It only blocks for the full
// duration on failure (the STM32 keyboard is normally present).
constexpr uint32_t kI2cTimeoutUs = 100000;

Key key_from_char(char c) {
    if (c >= '0' && c <= '9') {
        return static_cast<Key>(static_cast<int>(Key::k0) + (c - '0'));
    }
    if (c >= 'a' && c <= 'z') {
        return static_cast<Key>(static_cast<int>(Key::kA) + (c - 'a'));
    }
    if (c >= 'A' && c <= 'Z') {
        return static_cast<Key>(static_cast<int>(Key::kA) + (c - 'A'));
    }
    switch (c) {
        case '.':
            return Key::kDot;
        case '+':
            return Key::kPlus;
        case '-':
            return Key::kMinus;
        case '*':
            return Key::kMultiply;
        case '/':
            return Key::kDivide;
        case '^':
            return Key::kPower;
        case '(':
            return Key::kLParen;
        case ')':
            return Key::kRParen;
        case ',':
            return Key::kComma;
        case '=':
            return Key::kEquals;
        case ' ':
            return Key::kSpace;
        default:
            return Key::kPrintable;
    }
}

}  // namespace

void Keyboard::init() {
    init_i2c_kbd();  // Vendored: configures i2c1 @ 10 kHz on GPIO 6/7
}

KeyEvent Keyboard::decode(uint8_t state, uint8_t code) {
    KeyEvent ev;
    const bool pressed = (state == kStatePressed || state == kStateHold);
    ev.pressed = pressed;

    switch (code) {
        case kCodeAlt:
            ev.key = Key::kAlt;
            alt_held_ = pressed;
            break;
        case kCodeShiftL:
        case kCodeShiftR:
            ev.key = Key::kShift;
            shift_held_ = pressed;
            break;
        case kCodeSym:
            ev.key = Key::kSym;
            break;
        case kCodeCtrl1:
        case kCodeCtrl2:
            ev.key = Key::kCtrl;
            ctrl_held_ = pressed;
            break;
        case kCodeEsc:
            ev.key = Key::kEscape;
            break;
        case kCodeLeft:
            ev.key = Key::kLeft;
            break;
        case kCodeUp:
            ev.key = Key::kUp;
            break;
        case kCodeDown:
            ev.key = Key::kDown;
            break;
        case kCodeRight:
            ev.key = Key::kRight;
            break;
        case kCodeCaps:
            ev.key = Key::kCapsLock;
            break;
        case kCodeInsert:
            ev.key = Key::kInsert;
            break;
        case kCodeHome:
            ev.key = Key::kHome;
            break;
        case kCodeDel:
            ev.key = Key::kDel;
            break;
        case 0x08:
            ev.key = Key::kBackspace;
            break;
        case 0x09:
            ev.key = Key::kTab;
            break;
        case 0x0A:
        case 0x0D:
            ev.key = Key::kEnter;
            break;
        default:
            if (code >= 0x81 && code <= 0x86) {
                ev.key = static_cast<Key>(static_cast<int>(Key::kF1) + (code - 0x81));
            } else if (code >= 0x20 && code < 0x7F) {
                ev.key = key_from_char(static_cast<char>(code));
                ev.ch = static_cast<char>(code);
            } else {
                ev.key = Key::kNone;
            }
            break;
    }

    ev.shift_held = shift_held_;
    ev.ctrl_held = ctrl_held_;
    ev.alt_held = alt_held_;

    if (ev.key != Key::kNone) {
        held_[static_cast<int>(ev.key)] = pressed;
        // Suppress auto-repeat "hold" reports for a key already down as
        // press events; callers see press once, then is_held().
        if (state == kStateHold) {
            ev.key = Key::kNone;
            ev.ch = 0;
        }
    }
    return ev;
}

KeyEvent Keyboard::poll() {
    KeyEvent none;
    const uint64_t now = time_us_64();

    if (phase_ == Phase::kIdle) {
        uint8_t reg = kRegFifo;
        const int rc =
            i2c_write_timeout_us(I2C_KBD_MOD, I2C_KBD_ADDR, &reg, 1, false, kI2cTimeoutUs);
        if (rc < 0) {
            return none;
        }
        phase_ = Phase::kAwaitData;
        phase_deadline_us_ = now + kReadDelayUs;
        return none;
    }

    // Phase::kAwaitData
    if (now < phase_deadline_us_) {
        return none;
    }
    phase_ = Phase::kIdle;

    uint8_t buf[2] = {0, 0};
    const int rc = i2c_read_timeout_us(I2C_KBD_MOD, I2C_KBD_ADDR, buf, 2, false, kI2cTimeoutUs);
    if (rc < 0 || (buf[0] == 0 && buf[1] == 0)) {
        return none;
    }
    return decode(buf[0], buf[1]);
}

bool Keyboard::is_held(Key k) const {
    return held_[static_cast<int>(k)];
}

Keyboard& keyboard() {
    static Keyboard instance;
    return instance;
}

}  // namespace platform
