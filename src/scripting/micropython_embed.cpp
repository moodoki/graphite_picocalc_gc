#include "scripting/micropython_embed.hpp"

#include <cstdio>
#include <cstring>

#include "config.hpp"
#include "platform/fault.hpp"
#include "platform/keyboard.hpp"
#include "platform/storage.hpp"
#include "platform/system.hpp"
#include "math/var_store.hpp"

// The C boundary. Everything that includes a MicroPython header is on
// the far side of it.
#include "scripting/calc_api.h"
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

// What calc.store() calls once it has written a variable (6B.3). The `calc`
// module deliberately knows nothing about platform::storage — that one
// dependency is what would stop calc_api.cpp compiling in the host test
// harness, and the harness is where the name rules and the reentrancy guard
// are actually checked. Installing it here rather than in main() keeps it
// next to the rest of the interpreter's bring-up.
void persist_variables() {
    math::save_variables(platform::storage());
}

// Is there room below us for a path that needs `need` bytes? A local's
// address is the current stack pointer to within a few bytes, and the floor
// is __StackTop minus the bank size — the same absolute floor
// picocalc_mp_init hands MicroPython, so the two agree.
//
// This exists because the calculator's evaluator has deeper frames than
// MicroPython does, and MICROPY_STACK_CHECK cannot see them: it guards the
// VM's own recursion, and by the time a binding is running, control has left
// the VM. calc.eval("solve(x^2-4,x,0,10)") overran SCRATCH_Y into core 1's
// stack on 2026-08-15 and hung the board — the D48 failure mode, reached from
// a new direction.
int stack_room(std::size_t need) {
    const char probe = 0;
    const auto sp = reinterpret_cast<std::uintptr_t>(&probe);
    const auto floor = reinterpret_cast<std::uintptr_t>(__StackTop) - platform::stack_total();
#if PICOCALC_STACK_PROBE
    std::printf("py-stack: free %u, need %u\n", static_cast<unsigned>(sp - floor),
                static_cast<unsigned>(need));
#endif
    return sp > floor && sp - floor > need ? 1 : 0;
}

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
    calc_api_set_persist_hook(&persist_variables);
    calc_api_set_stack_hook(&stack_room);
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
