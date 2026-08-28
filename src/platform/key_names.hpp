#pragma once

#include "platform/keyboard.hpp"

namespace platform {

// The names for keys that carry no character, resolved in ONE place.
//
// This lives here, next to the Key enum, because two tables that answer
// "which key is this" will diverge -- and the divergence is silent. That
// argument is D87's, made when the MicroPython bindings needed names for
// `ev["name"]` and `calc.key_held()`; it applies again in Phase 6.4.4,
// where a key script names keys to replay and a wrong name produces a
// *plausible* screenshot rather than an error.
//
// Deliberately covers only the keys with no `ch`: navigation, editing and
// F1-F6. Letters, digits and punctuation arrive as characters and are
// matched that way, and adding them here would change what a running
// script sees from `ev["name"]` -- today "" for anything printable.

// Static storage in every case, so the name outlives any event it
// describes. "" rather than nullptr for an unnamed key, so a script can
// compare without a null check first.
const char* key_name(Key key);

// Key::kNone when the name is not one of ours.
Key key_from_name(const char* name);

// The Key a printable character arrives as. Letters and digits have their
// own enumerators (the driver sets both `key` and `ch` for them), so a
// synthesised event must set the same one a real keypress would or
// is_held() and any handler switching on `key` will disagree with the
// hardware. Anything else is Key::kPrintable, read through `ch`.
Key key_from_char(char c);

}  // namespace platform
