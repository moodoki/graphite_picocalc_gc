#pragma once

namespace host {

// Writes the composited screen to a binary PPM (P6, 320x320). False on
// any I/O failure -- the caller is a screenshot tool, so a half-written
// image must be reported, never quietly kept.
bool write_ppm(const char* path);

}  // namespace host
