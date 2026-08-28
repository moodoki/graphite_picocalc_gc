#pragma once

#include <cstddef>
#include <cstdint>

// MicroPython interpreter wrapper (Phase 6B.2, spec §4.1).
//
// MicroPython enters this project as a git submodule (drivers/micropython)
// whose embed port GENERATES the C tree we compile — see CMakeLists.txt
// and drivers/README.md. This class is the only thing the rest of the
// firmware talks to; mp_port.c holds every line that touches a
// MicroPython header, and the comment there says why.
//
// Two constraints shape everything here:
//
//   * NO ALLOCATOR EXISTS. The GC heap is a static bss array of
//     config::kPythonHeapSize (40 KB on the Pico 1, 96 KB on the Pico 2),
//     permanently reserved. What is lazy is init()/shutdown(), which run
//     on entering and leaving the program screen — not the reservation.
//     D57 says the heap is "lazily allocated, not reserved at boot"; D70
//     later established that lazy allocation "does not shrink the number
//     that has to be found", and with no allocator the bytes are
//     unavoidably static.
//
//   * MICROPYTHON RAISES BY LONGJMP, past every intervening frame, with
//     no compiler diagnostic when that frame was C++ holding something
//     with a destructor. Matters from 6B.3 onward, when `calc` bindings
//     start calling back into C++.

namespace scripting {

class PythonInterpreter {
public:
    // Receives everything the script prints, including tracebacks. NOT
    // NUL-terminated — use `len`.
    using OutputCallback = void (*)(const char* text, std::size_t len);

    // Brings up the runtime on `heap_bytes` of the static heap, clamped
    // to its actual size. Idempotent: a second call while running is a
    // no-op returning true.
    bool init(std::size_t heap_bytes);
    bool init();

    // Tears the runtime down. The heap bytes stay reserved (see above);
    // what this releases is the interpreter's claim on them, so the next
    // init() starts from a fully-free heap.
    void shutdown();

    bool is_running() const { return initialized_; }

    // Compiles and runs `code` as a module. False means the script
    // raised — including KeyboardInterrupt from ESC. The traceback has
    // already gone to the output callback by then.
    bool exec(const char* code);

    // Same, for a script on the SD card (6B.15/6B.16 — SD apps are the
    // first thing that needs it, which is why 6B.2 deferred it). False
    // means it raised OR the file could not be read; either way the
    // reason has already gone to the output callback.
    //
    // `path` is borrowed for the whole call: it backs the lexer's reader
    // and names the traceback's source, so it must outlive the run. The
    // source is streamed through a 128-byte window, never staged whole.
    bool exec_file(const char* path);

    void set_output_callback(OutputCallback cb) { output_ = cb; }

    // Free GC heap, 0 when not running. The spec (§4.4) wants a real
    // measurement of what a dataset costs, not an estimate.
    std::size_t heap_free() const;

    // Total heap bytes available to configure, i.e. the static array.
    static std::size_t heap_capacity();

    // Bytes of core 0's 4 KB stack MicroPython is allowed to reach down
    // to before it raises, measured from platform::stack_top(). Sizing this
    // is a measurement, not an argument (D47/D48).
    static std::size_t stack_limit();

    // --- Called from mp_port.c's C boundary, not by application code ---

    // Fans MicroPython's stdout out to the registered callback.
    void emit(const char* text, std::size_t len);

    // True once ESC has been seen during the current exec(). Rate-limits
    // its own keyboard polling: the VM hook fires every 128 branches and
    // an I2C round trip there would dominate the run time.
    bool poll_interrupt();

private:
    // The bracket every run needs, whether the source came from the
    // editor buffer or from a file: reset the per-run latches before,
    // persist / collect / check for fragmentation after. Factored out
    // when exec_file arrived rather than copied, because the after half
    // is where D77's heap rescue lives and two copies of that would
    // drift.
    void begin_run();
    bool end_run(bool ok);

    bool initialized_ = false;
    OutputCallback output_ = nullptr;

    // Set while exec() is on the stack, so the VM hook only polls the
    // keyboard when there is actually a script to interrupt.
    bool running_script_ = false;
    bool interrupt_pending_ = false;
    std::uint32_t last_poll_ms_ = 0;
};

PythonInterpreter& python();

}  // namespace scripting
