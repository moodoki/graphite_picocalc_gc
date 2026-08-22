#pragma once

namespace platform {
class Storage;
}

// Persistence for the A-Z/theta/Ans variables (variables.dat).
//
// This lived as two private methods on HomeScreen until 6B.3, when
// calc.store() became a second writer: a value a script stores has to survive
// a power cycle the same way a typed one does, and scripting/ must not reach
// into apps/. One copy of the image format is the point — two would drift, and
// the format is versioned precisely because it already changed once (4D.15).
//
// It takes a Storage& rather than calling platform::storage() for the same
// reason math::lists()/matrices()/named_lists() do: math/ knows the type by
// forward declaration and nothing more.
namespace math {

void save_variables(platform::Storage& fs);

// Missing, truncated or written by a pre-PCV1 build: leaves the variables at
// their defaults rather than failing. The pre-PCV1 file was a raw vars[] dump
// with no header, so it fails the magic check and is ignored — a one-time
// variable reset on first boot after that change.
void load_variables(platform::Storage& fs);

}  // namespace math
