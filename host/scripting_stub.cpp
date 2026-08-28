// scripting::PythonInterpreter with no runtime behind it (Phase 6.4.0).
//
// Not in the spec's section 3.7 file list -- it turned up on first link.
// home_screen.cpp calls scripting::python() (the `py` command), so the
// spike cannot link without either MicroPython or this, and 6.4.0
// deliberately excludes MicroPython so that it stays a fast fail on two
// OSes rather than a port of the embed build.
//
// init() returning false is the interface's own "runtime unavailable"
// path, which the program screen already handles -- it is the same answer
// a board gives when the heap cannot be claimed. So the calculator runs
// with Python reporting itself absent, rather than with a fake that
// accepts scripts and silently does nothing.
//
// 6.4.3 replaces this file with the real embed build.

#include "scripting/calc_api.h"
#include "scripting/micropython_embed.hpp"

namespace scripting {

bool PythonInterpreter::init(std::size_t /*heap_bytes*/) {
    return false;
}

bool PythonInterpreter::init() {
    return false;
}

void PythonInterpreter::shutdown() {
    initialized_ = false;
}

bool PythonInterpreter::exec(const char* /*code*/) {
    return false;
}

bool PythonInterpreter::exec_file(const char* /*path*/) {
    return false;
}

std::size_t PythonInterpreter::heap_free() const {
    return 0;
}

std::size_t PythonInterpreter::heap_capacity() {
    return 0;
}

std::size_t PythonInterpreter::stack_limit() {
    return 0;
}

void PythonInterpreter::emit(const char* text, std::size_t len) {
    if (output_ != nullptr) {
        output_(text, len);
    }
}

bool PythonInterpreter::poll_interrupt() {
    return false;
}

void PythonInterpreter::begin_run() {}

bool PythonInterpreter::end_run(bool ok) {
    return ok;
}

PythonInterpreter& python() {
    static PythonInterpreter instance;
    return instance;
}

}  // namespace scripting

// The two pieces of calc_api's C boundary that ProgramScreen asks about
// after a run, rather than during one. They live in calc_api.cpp, which
// is a MicroPython translation unit and so not in the spike -- but the
// questions they answer ("did the script ask for the graph screen?", "did
// it take the panel?") are asked unconditionally, so they still have to
// link. With no interpreter the answer to both is no.
extern "C" {

int calc_api_take_show_graph(void) {
    return 0;
}

int calc_api_canvas_owns_display(void) {
    return 0;
}

}  // extern "C"
