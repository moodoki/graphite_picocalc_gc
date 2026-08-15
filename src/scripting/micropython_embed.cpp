#include "scripting/micropython_embed.hpp"

#include <cstring>

#include "config.hpp"
#include "platform/fault.hpp"
#include "platform/keyboard.hpp"
#include "platform/system.hpp"

// The C boundary. Everything that includes a MicroPython header is on
// the far side of it.
#include "scripting/mp_port.h"

// Linker-provided top of core 0's stack (SCRATCH_Y). Absolute symbol, so
// the address *is* the value — hence the array-typed extern, and hence
// file scope: inside an anonymous namespace it picks up internal linkage
// and no longer resolves. Same shape as src/platform/fault.cpp, which
// documents this at length. The reserved double-underscore name is the
// linker script's, not ours.
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" char __StackTop[];
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

namespace scripting {

namespace {

// The GC heap. Statically reserved because there is no allocator to be
// lazy with — see the header. 8-byte aligned: MicroPython stores doubles
// in heap blocks and MICROPY_FLOAT_IMPL_DOUBLE is set.
alignas(8) std::uint8_t g_heap[config::kPythonHeapSize];

// How much of core 0's 4 KB stack is kept BACK from MicroPython, measured
// down from __StackTop. It has to cover two things the interpreter cannot
// see: the UI frames already below us when a script runs (main loop ->
// screen manager -> on_key -> run), and an interrupt frame landing on top
// at any moment. 1 KB is the starting figure; the real one comes off
// `stack: peak` on hardware with a script compiling, which is the whole
// method D47/D48 arrived at.
constexpr std::size_t kStackReserve = 1024;

// How often the VM hook is allowed to touch the I2C keyboard. The hook
// fires every 128 branch opcodes; the keyboard's own two-phase state
// machine spends >=10 ms per cycle (D7), so anything below that is
// wasted work on the interpreter's hot path.
constexpr std::uint32_t kPollIntervalMs = 20;

}  // namespace

std::size_t PythonInterpreter::heap_capacity() {
    return sizeof(g_heap);
}

std::size_t PythonInterpreter::stack_limit() {
    const std::size_t total = platform::stack_total();
    return total > kStackReserve ? total - kStackReserve : 0;
}

bool PythonInterpreter::init(std::size_t heap_bytes) {
    if (initialized_) {
        return true;
    }
    if (heap_bytes == 0 || heap_bytes > sizeof(g_heap)) {
        heap_bytes = sizeof(g_heap);
    }
    picocalc_mp_init(g_heap, heap_bytes, __StackTop, stack_limit());
    initialized_ = true;
    interrupt_pending_ = false;
    return true;
}

bool PythonInterpreter::init() {
    return init(sizeof(g_heap));
}

void PythonInterpreter::shutdown() {
    if (!initialized_) {
        return;
    }
    picocalc_mp_deinit();
    initialized_ = false;
    running_script_ = false;
    interrupt_pending_ = false;
}

bool PythonInterpreter::exec(const char* code) {
    if (!initialized_ || code == nullptr) {
        return false;
    }
    interrupt_pending_ = false;
    last_poll_ms_ = platform::uptime_ms();
    running_script_ = true;
    const bool ok = picocalc_mp_exec_str(code) != 0;
    running_script_ = false;
    return ok;
}

std::size_t PythonInterpreter::heap_free() const {
    return initialized_ ? picocalc_mp_heap_free() : 0;
}

void PythonInterpreter::emit(const char* text, std::size_t len) {
    if (output_ != nullptr && text != nullptr && len > 0) {
        output_(text, len);
    }
}

bool PythonInterpreter::poll_interrupt() {
    if (!running_script_) {
        return false;
    }
    if (interrupt_pending_) {
        // Already raised once. Keep saying yes: a script can catch
        // KeyboardInterrupt, and if it does, ESC must still get the user
        // out rather than being swallowed by a bare `except:`.
        return true;
    }
    const std::uint32_t now = platform::uptime_ms();
    if (now - last_poll_ms_ < kPollIntervalMs) {
        return false;
    }
    last_poll_ms_ = now;
    // Draining here steals events from the main loop, which is fine: the
    // main loop is not running — it is blocked in on_key, below us.
    const platform::KeyEvent ev = platform::keyboard().poll();
    if (ev.pressed && ev.key == platform::Key::kEscape) {
        interrupt_pending_ = true;
    }
    return interrupt_pending_;
}

PythonInterpreter& python() {
    static PythonInterpreter instance;
    return instance;
}

}  // namespace scripting

// ---- The C boundary's side of the callbacks ----

extern "C" void picocalc_py_output(const char* str, std::size_t len) {
    scripting::python().emit(str, len);
}

extern "C" int picocalc_py_interrupt_requested(void) {
    return scripting::python().poll_interrupt() ? 1 : 0;
}
