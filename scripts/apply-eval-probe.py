#!/usr/bin/env python3
"""Apply the evaluation probe to a checkout that predates -DPICOCALC_EVAL_PROBE.

**You usually do not need this.** The probe is a CMake option now:

    cmake -G Ninja -DPICO_BOARD=pico -DPICOCALC_EVAL_PROBE=ON -B build/pico-probe -S .

This script exists for the other half of an A/B pass — instrumenting a *released
tag* that shipped before the option existed (v0.3.2 and earlier), so the baseline
carries the same probe as the build under test. That symmetry is the whole point
of the measurement; hand-editing two trees is how it stops being symmetric.

    git worktree add /tmp/v032 v0.3.2
    python3 scripts/apply-eval-probe.py /tmp/v032
    cmake -G Ninja -DPICO_BOARD=pico -B /tmp/v032/build/pico -S /tmp/v032

The edits it makes are deliberately the same three the option compiles in, so a
`diff` of the two probes should be empty. See D52 and
docs/notes/measurements/phase5.2/README.md.

The window is: top of evaluate_input -> entry to push_entry. Every branch of
evaluate_input funnels through push_entry, so this covers evaluation plus result
formatting, and excludes the SD history write (persist_history_line, called
after) and rendering. Both are what made the naive whole-submit_line timer
useless: `2+3*4` measured 17 ms, of which almost none was arithmetic.

Usage: apply-eval-probe.py <tree-root>
"""

import sys
from pathlib import Path

root = Path(sys.argv[1])
hs_cpp = root / "src/apps/home_screen.cpp"
hs_hpp = root / "src/apps/home_screen.hpp"
main_cpp = root / "src/main.cpp"


MARKER = "§9 evaluator probe"

for f in (hs_cpp, hs_hpp, main_cpp):
    if MARKER in f.read_text():
        sys.exit(f"{f}: probe already applied (found {MARKER!r})")


def patch(path, old, new, count=1):
    s = path.read_text()
    n = s.count(old)
    if n != count:
        sys.exit(f"{path}: expected {count} match for {old[:60]!r}, found {n}")
    path.write_text(s.replace(old, new, count))
    print(f"  patched {path.relative_to(root)}")


# 1. The timer itself, in home_screen.cpp.
patch(
    hs_cpp,
    "void HomeScreen::push_entry(const char* expr, const char* result, ResultKind kind) {\n"
    "    ++entries_total_;",
    "// Evaluation probe (5.2.12, D52), injected by scripts/apply-eval-probe.py.\n"
    "// Unconditional on purpose: this tree predates -DPICOCALC_EVAL_PROBE.\n"
    "// Started at the top of evaluate_input and\n"
    "// stopped here, on entry to push_entry -- every evaluation branch funnels\n"
    "// through this function, and the SD history write happens after it. So the\n"
    "// window is evaluation + result formatting, with no I/O and no rendering.\n"
    "namespace {\n"
    "uint64_t g_probe_t0 = 0;\n"
    "uint32_t g_probe_us = 0;\n"
    "}  // namespace\n"
    "\n"
    "uint32_t home_eval_us() { return g_probe_us; }\n"
    "\n"
    "void HomeScreen::push_entry(const char* expr, const char* result, ResultKind kind) {\n"
    "    if (g_probe_t0 != 0) {\n"
    "        g_probe_us = static_cast<uint32_t>(platform::uptime_us() - g_probe_t0);\n"
    "        g_probe_t0 = 0;\n"
    "    }\n"
    "    ++entries_total_;",
)

patch(
    hs_cpp,
    "void HomeScreen::evaluate_input(bool force_decimal) {\n"
    "    if (input_.empty()) {\n"
    "        return;\n"
    "    }",
    "void HomeScreen::evaluate_input(bool force_decimal) {\n"
    "    if (input_.empty()) {\n"
    "        return;\n"
    "    }\n"
    "    g_probe_t0 = platform::uptime_us();\n"
    "    g_probe_us = 0;",
)

patch(
    hs_cpp,
    '#include "apps/home_screen.hpp"',
    '#include "apps/home_screen.hpp"\n#include "platform/system.hpp"  // §9 probe',
)

# 2. Declare the accessor.
patch(hs_hpp, "namespace apps {", "namespace apps {\n\n// §9 evaluator probe (5.2.12): microseconds for the last evaluation.\nuint32_t home_eval_us();")

# 3. Report it on the inject line, appended last so one parser reads a build
#    with the probe and one without.
s = main_cpp.read_text()
if 'kind=%s us=%lu' in s:
    old = ('printf("inject: \\"%s\\" -> \\"%s\\" kind=%s us=%lu\\n", inject_buf, result, kind,\n'
           '                           static_cast<unsigned long>(elapsed_us));')
    new = ('printf("inject: \\"%s\\" -> \\"%s\\" kind=%s us=%lu eval_us=%lu\\n", inject_buf,\n'
           '                           result, kind, static_cast<unsigned long>(elapsed_us),\n'
           '                           static_cast<unsigned long>(apps::home_eval_us()));')
elif 'kind=%s\\n' in s:
    old = 'printf("inject: \\"%s\\" -> \\"%s\\" kind=%s\\n", inject_buf, result, kind);'
    new = ('printf("inject: \\"%s\\" -> \\"%s\\" kind=%s eval_us=%lu\\n", inject_buf, result,\n'
           '                           kind, static_cast<unsigned long>(apps::home_eval_us()));')
else:
    sys.exit("main.cpp: could not find the inject echo")
patch(main_cpp, old, new)

print("probe applied")
