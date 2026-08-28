#pragma once

namespace host {

// Queue one key. Accepts a name from platform::key_names ("up", "enter",
// "esc", "f1"...) or a single printable character. False if it is neither.
bool queue_key(const char* name);

// Replay a key-script file into the queue (D97). Returns false and reports
// the offending line on stderr if any token is not a key.
bool run_keyscript(const char* path);

bool keys_pending();

}  // namespace host
