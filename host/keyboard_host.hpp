#pragma once

namespace host {

// Queue one key as a press followed by a release. Accepts the names
// micropython_embed.cpp's table uses ("up", "enter", "esc", "f1"...) or a
// single printable character. False if the name is neither.
bool queue_key(const char* name);

bool keys_pending();

}  // namespace host
