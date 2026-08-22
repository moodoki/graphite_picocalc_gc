#include "scripting/micropython_embed.hpp"

#include <cstdio>
#include <cstring>

#include "config.hpp"
#include "platform/fault.hpp"
#include "platform/keyboard.hpp"
#include "platform/storage.hpp"
#include "platform/system.hpp"
#include "math/lists.hpp"
#include "math/matrix.hpp"
#include "math/var_store.hpp"
#include "graph/graph_state.hpp"

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

// Contiguous free bytes below which the interpreter is considered unusable.
// The observed failure was a 512-byte request; 1 KB leaves room for the
// next statement to be a little larger than the one that noticed.
constexpr std::size_t kMinCompileBytes = 1024;

// What calc.store() calls once it has written a variable (6B.3). The `calc`
// module deliberately knows nothing about platform::storage — that one
// dependency is what would stop calc_api.cpp compiling in the host test
// harness, and the harness is where the name rules and the reentrancy guard
// are actually checked. Installing it here rather than in main() keeps it
// next to the rest of the interpreter's bring-up.
void persist_state(CalcPersistTarget what, int index) {
    switch (what) {
        case kCalcPersistVars:
            math::save_variables(platform::storage());
            break;
        case kCalcPersistGraph:
            graph::state().save(platform::storage());
            break;
        case kCalcPersistList:
            math::lists().save(platform::storage(), index);
            break;
        case kCalcPersistMatrix:
            math::matrices().save(platform::storage(), index);
            break;
    }
}

// ---- 6B.9: the key queue (D81) ----
//
// The hook used to poll once every 20 ms and throw away anything that was not
// ESC — harmless while nothing else wanted key events, and a silent loss of
// type-ahead once something did. Now every event is queued and ESC is
// recognised on the way past, so the interrupt still works and a script can
// have the rest.
//
// D81 said "one poller". Building it showed the rule has to be ONE DRAIN, ONE
// QUEUE: a blocking calc.wait_key() sits inside a binding where the VM hook
// never runs, so it must be able to drive the drain itself.
constexpr int kKeyQueueSize = 16;
CalcKeyEvent g_key_queue[kKeyQueueSize];
int g_key_head = 0;  // next to read
int g_key_count = 0;
bool g_esc_seen = false;

// ---- Opt-in ESC delivery (issue #55) ----
//
// By default ESC never reaches a script: drain_keys() turns it into an
// interrupt, which is what makes a runaway loop always escapable. That
// also means a script CANNOT implement "ESC goes back one level" inside
// itself, which is what the periodic table's legend screen wanted.
//
// With capture on, ESC is queued as an ordinary key event instead. The
// guarantee is kept by a backstop that needs no timer and no heuristic:
// count the ESC presses the script has not picked up. A script that is
// reading keys consumes each one and the count returns to zero, so it
// never fires. A script that has stopped reading — the only dangerous
// case — never consumes any, so the second press interrupts it.
//
// The consequence is deliberate and worth stating: a rapid double-tap
// force-quits even a well-behaved script. That is the escape hatch, and
// it is the same gesture users already expect from one.
constexpr int kEscForceQuit = 2;
bool g_capture_esc = false;
int g_esc_unconsumed = 0;

// Names resolved HERE, where platform::Key is visible. A table of hardcoded
// enumerator values in calc_api.cpp would go quietly wrong the first time the
// enum gained a member.
//
// ONE table, read in both directions: script_key_held() looks a name up, and
// queue_push() looks a key's name up on the way out. Two tables would let
// `ev["name"] == "up"` and `calc.key_held("up")` disagree, which is precisely
// the sort of thing that is never noticed until an app behaves differently
// depending on which of the two it happened to use.
struct NamedKey {
    const char* name;
    platform::Key key;
};
constexpr NamedKey kNamedKeys[] = {
    {"up", platform::Key::kUp},
    {"down", platform::Key::kDown},
    {"left", platform::Key::kLeft},
    {"right", platform::Key::kRight},
    {"enter", platform::Key::kEnter},
    {"esc", platform::Key::kEscape},
    {"space", platform::Key::kSpace},
    {"tab", platform::Key::kTab},
    {"back", platform::Key::kBackspace},
    {"del", platform::Key::kDel},
    {"home", platform::Key::kHome},
    // The function keys are here because an app drawing its own softkey bar
    // has no other way to read them: they carry no character, so `ch` is 0.
    {"f1", platform::Key::kF1},
    {"f2", platform::Key::kF2},
    {"f3", platform::Key::kF3},
    {"f4", platform::Key::kF4},
    {"f5", platform::Key::kF5},
    {"f6", platform::Key::kF6},
};

// "" rather than null for a key with no name, so a script can compare without
// a None check first. Static storage in every case — the name outlives the
// queued event by construction.
const char* key_name(platform::Key key) {
    for (const NamedKey& n : kNamedKeys) {
        if (n.key == key) {
            return n.name;
        }
    }
    return "";
}

// The driver fills KeyEvent::ch for printable ASCII only (keyboard.hpp) —
// Enter, Backspace, Tab and Del all arrive as 0. A Python script has nothing
// but `ch` and a raw `code` to work with, so "did they press Enter" was
// unanswerable without hardcoding an enumerator value on the C side.
//
// Resolved HERE, where platform::Key is visible, for the same reason
// script_key_held's name table is: a table of enum values anywhere else goes
// quietly wrong the first time the enum gains a member. calc.input() depends
// on it directly — on hardware 2026-08-16, ENTER simply did nothing, because
// the loop was waiting for a '\r' the driver never produces.
char control_char(platform::Key key, char printable) {
    switch (key) {
        case platform::Key::kEnter:
            return '\r';
        case platform::Key::kBackspace:
            return '\b';
        case platform::Key::kTab:
            return '\t';
        case platform::Key::kDel:
            return 127;
        default:
            return printable;
    }
}

void queue_push(const platform::KeyEvent& ev) {
    CalcKeyEvent e;
    e.code = static_cast<int>(ev.key);
    e.ch = static_cast<unsigned char>(control_char(ev.key, ev.ch));
    e.name = key_name(ev.key);
    e.shift = ev.shift_held ? 1 : 0;
    e.ctrl = ev.ctrl_held ? 1 : 0;
    e.alt = ev.alt_held ? 1 : 0;
    if (g_key_count == kKeyQueueSize) {
        // Drop the OLDEST. The newest keypress is the live one — the opposite
        // of ui::OutputLog, which keeps the tail because a traceback is at the
        // end.
        g_key_head = (g_key_head + 1) % kKeyQueueSize;
        --g_key_count;
    }
    g_key_queue[(g_key_head + g_key_count) % kKeyQueueSize] = e;
    ++g_key_count;
}

// Drain the keyboard FIFO into the queue, noting ESC as we go.
//
// Loops until fifo_empty(): a kNone from poll() usually just means a read is
// in flight, and breaking on the first one capped draining at a single event
// per frame (hardware, 2026-07-18).
void drain_keys() {
    for (int guard = 0; guard < 64; ++guard) {
        const platform::KeyEvent ev = platform::keyboard().poll();
        if (ev.pressed) {
            if (ev.key == platform::Key::kEscape && !g_capture_esc) {
                g_esc_seen = true;
            } else {
                if (ev.key == platform::Key::kEscape) {
                    ++g_esc_unconsumed;
                }
                queue_push(ev);
            }
        }
        if (ev.key == platform::Key::kNone && platform::keyboard().fifo_empty()) {
            return;
        }
    }
}

int script_key_poll(CalcKeyEvent* out) {
    drain_keys();
    if (g_key_count == 0) {
        return 0;
    }
    *out = g_key_queue[g_key_head];
    g_key_head = (g_key_head + 1) % kKeyQueueSize;
    --g_key_count;
    // Handing an ESC to the script is what "consumed" means: the script
    // is reading, so the backstop below stands down.
    if (out->code == static_cast<int>(platform::Key::kEscape)) {
        g_esc_unconsumed = 0;
    }
    return 1;
}

int script_key_held(const char* name) {
    using platform::Key;
    for (const NamedKey& n : kNamedKeys) {
        if (std::strcmp(name, n.name) == 0) {
            return platform::keyboard().is_held(n.key) ? 1 : 0;
        }
    }
    // A single letter or digit names itself.
    if (name[0] != 0 && name[1] == 0) {
        const char c = name[0];
        if (c >= 'a' && c <= 'z') {
            return platform::keyboard().is_held(
                       static_cast<Key>(static_cast<int>(Key::kA) + (c - 'a')))
                       ? 1
                       : 0;
        }
        if (c >= '0' && c <= '9') {
            return platform::keyboard().is_held(
                       static_cast<Key>(static_cast<int>(Key::k0) + (c - '0')))
                       ? 1
                       : 0;
        }
    }
    return 0;
}

// ---- 6B.10: file I/O, over platform::Storage ----

long file_size(const char* path) {
    return platform::storage().file_size(path);
}

int file_read(const char* path, long offset, char* buf, int len) {
    return platform::storage().read_file_range(path, static_cast<std::size_t>(offset),
                                               reinterpret_cast<std::uint8_t*>(buf),
                                               static_cast<std::size_t>(len));
}

int file_write(const char* path, const char* buf, int len) {
    return platform::storage().write_file(path, reinterpret_cast<const std::uint8_t*>(buf),
                                          static_cast<std::size_t>(len))
               ? 1
               : 0;
}

int file_append(const char* path, const char* buf, int len) {
    return platform::storage().append_file(path, reinterpret_cast<const std::uint8_t*>(buf),
                                           static_cast<std::size_t>(len))
               ? 1
               : 0;
}

int file_exists(const char* path) {
    return platform::storage().file_exists(path) ? 1 : 0;
}

// Registered with calc_api so the binding can reach the flag without
// calc_api.cpp knowing anything about the interpreter (D74's split).
int set_capture_esc(int on) {
    const int prev = g_capture_esc ? 1 : 0;
    g_capture_esc = on != 0;
    if (!g_capture_esc) {
        // Leaving capture mode drops the backstop's tally: presses that
        // were waiting for the script are no longer its responsibility.
        g_esc_unconsumed = 0;
    }
    return prev;
}

int file_list(const char* path, CalcDirEntry* out, int max, int skip) {
    // DirEntry and CalcDirEntry are the same shape but not the same type:
    // the C boundary may not include a C++ header (calc_api.h's own note),
    // so the copy is the price of that rule. It is bounded by `max`, which
    // the glue keeps small enough to sit on the 4 KB core-0 stack.
    // Four, matching CALC_DIR_WINDOW: this frame and the glue's are both
    // live at the leaf, so the window is paid for twice on a 4 KB stack.
    platform::Storage::DirEntry buf[4];
    const int want = max < static_cast<int>(sizeof(buf) / sizeof(buf[0]))
                         ? max
                         : static_cast<int>(sizeof(buf) / sizeof(buf[0]));
    const int n = platform::storage().list_dir(path, buf, want, skip);
    if (n < 0) {
        return -1;
    }
    for (int i = 0; i < n; ++i) {
        std::snprintf(out[i].name, sizeof(out[i].name), "%s", buf[i].name);
        out[i].is_dir = buf[i].is_dir ? 1 : 0;
        out[i].size = static_cast<unsigned long>(buf[i].size);
    }
    return n;
}

constexpr CalcFileOps kFileOps = {file_size,   file_read,   file_write,
                                  file_append, file_exists, file_list};

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
    calc_api_set_persist_hook(&persist_state);
    calc_api_set_stack_hook(&stack_room);
    calc_api_set_key_hooks(&script_key_poll, &script_key_held);
    calc_api_set_file_ops(&kFileOps);
    calc_api_set_capture_esc_hook(&set_capture_esc);
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

void PythonInterpreter::begin_run() {
    interrupt_pending_ = false;
    last_poll_ms_ = platform::uptime_ms();
    // Resets D68's "has this run plotted yet" latch, so a script's graph is
    // a function of the script and not of what the last one left in Y1-Y7.
    calc_api_begin_run();
    // A run starts with no stale ESC and no stale keystrokes — and with
    // capture OFF, so a script that raised on its last run cannot leave
    // ESC swallowed for the next one.
    g_capture_esc = false;
    g_esc_unconsumed = 0;
    g_esc_seen = false;
    g_key_head = 0;
    g_key_count = 0;
    running_script_ = true;
}

bool PythonInterpreter::exec(const char* code) {
    if (!initialized_ || code == nullptr) {
        return false;
    }
    begin_run();
    const bool ok = picocalc_mp_exec_str(code) != 0;
    return end_run(ok);
}

bool PythonInterpreter::exec_file(const char* path) {
    if (!initialized_ || path == nullptr) {
        return false;
    }
    begin_run();
    const int rc = picocalc_mp_exec_file(path);
    if (rc < 0) {
        // Distinct from "the script raised": there is no traceback,
        // because nothing was ever compiled. Say so, or the launcher
        // just appears to do nothing.
        static const char msg[] = "cannot read script\n";
        std::printf("py: cannot read %s\n", path);
        emit(msg, sizeof(msg) - 1);
    }
    return end_run(rc > 0);
}

bool PythonInterpreter::end_run(bool ok) {
    running_script_ = false;
    // Lists and matrices the run wrote are saved here, once each, rather than
    // on every binding call — D82. Before the collect, so a run that ended by
    // exhausting the heap still persists what it gathered.
    calc_api_flush_run();

    // Always, not only on failure. Compiling the NEXT statement allocates,
    // so a run that leaves the heap full of garbage makes everything after
    // it fail — including `gc.collect()`, which cannot be compiled either.
    // Measured 2026-08-15: one MemoryError wedged the `py` path until a
    // power cycle. A mark-sweep over 40 KB is nothing next to having just
    // run a script, so there is no reason to make this conditional.
    picocalc_mp_gc_collect();

    // Collecting is not always enough. The GC does not compact, so a run that
    // interleaved many small short-lived objects with a few surviving ones
    // leaves the heap shredded: measured 2026-08-15, a 400-iteration loop
    // ended with 31.5 KB free and no run long enough for the 512 bytes the
    // next compile wanted. Every statement after it failed, including
    // `gc.collect()` — which cannot help, because it has to be compiled
    // first. Only a power cycle cleared it.
    //
    // So when the heap can no longer compile anything, rebuild it. This
    // discards the script's variables, which is why it is announced rather
    // than done quietly; the alternative is a Python subsystem that stays
    // dead until the battery is pulled.
    if (picocalc_mp_heap_max_free() < kMinCompileBytes) {
        // No leading newline: whatever ran last (a traceback, or print output)
        // already ended with one, and the home screen shows only the final
        // line of what a `py` statement produced.
        static const char msg[] =
            "[heap too fragmented to continue - interpreter reset, variables lost]\n";
        std::printf("%s", msg);
        emit(msg, sizeof(msg) - 1);
        shutdown();
        init();
        return false;
    }
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
    // Draining here takes events from the main loop, which is fine: the main
    // loop is not running — it is blocked in on_key, below us. Since 6B.9 the
    // non-ESC ones are QUEUED rather than discarded (D81), so a script can
    // read them and type-ahead is no longer silently lost.
    drain_keys();
    if (g_esc_seen) {
        interrupt_pending_ = true;
    }
    // Backstop for capture mode: presses the script never picked up.
    // Queue overflow drops the oldest event, so an ESC can be lost
    // before delivery — which only makes this fire sooner, and a script
    // overflowing its key queue is exactly the one to interrupt.
    if (g_esc_unconsumed >= kEscForceQuit) {
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
