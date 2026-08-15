# Contributing

Graphite is a personal project, built in the open. Issues and pull requests are
welcome; so is forking it and going your own way. This file is the map for
anyone reading or changing the source.

Before anything else: [README](README.md#quick-start-building-from-source) for the build, then
[docs/dev-environment.md](docs/dev-environment.md) for the full toolchain
setup (macOS Apple Silicon is the developed-on platform; CI builds on Ubuntu).

## Finding something to work on

Open work lives in [GitHub Issues](https://github.com/moodoki/graphite_picocalc_gc/issues).
Issues carry three axes of label: a `type:`, an `area:` mirroring `src/`, and
where it applies `hw-pending` (cannot be verified without a board on the desk)
or `board:pico1` / `board:pico2` (reproduces on one target only). Milestones are
phases.

If you work on this regularly, `./scripts/gh-issues.py` mirrors the whole
backlog into a gitignored local file so you can read it offline. What is tracked
as an issue versus what stays in the repo is settled in
[docs/notes/issue-tracking.md](docs/notes/issue-tracking.md).

## Ground rules

- **Both boards, always.** Every change must build for `pico` and `pico2`. They
  differ in core, RAM, flash and FPU, and the differences belong in
  `src/platform/`, not sprinkled through the app code.
- **`drivers/` is read-only by default.** Vendored third-party code. If
  behaviour must change, wrap it in `src/platform/`. When editing in place is
  genuinely unavoidable, record it under *Local modifications* in
  [drivers/README.md](drivers/README.md) so a re-vendor cannot silently drop
  the change.
- **The stack is 4 KB and shared.** Core 0 runs the app; core 1 runs the
  display service. Recursion without a stated, measured depth cap has crashed
  this firmware more than once — see D45, D47, D48 and D76 in
  [decisions.md](docs/notes/decisions.md) before adding any. Anything called
  from a Python binding starts ~1.8 KB down and must be measured, not
  reasoned about: `-DPICOCALC_STACK_PROBE=ON` reports free stack at each
  binding.
- **Decisions get written down.** Anything non-obvious — a rejected
  alternative, a constraint discovered on hardware, a number that came from a
  measurement — goes in the decision log with its reasoning, not just its
  conclusion.

## Building

**First clone only:** this repo has one git submodule (MicroPython, Phase 6B).
The build fails with a pointed error if it is missing.

```bash
git submodule update --init --recursive
```

The MicroPython embed package is *generated* at CMake configure time, so the
build also needs `make` and a host C compiler. Both are already present in any
working toolchain setup.

```bash
./scripts/build-all.sh          # both boards (--clean to reconfigure)
./scripts/size-report.sh        # flash/RAM budget per section
./scripts/flash.sh              # copy the UF2 to a mounted BOOTSEL volume
./scripts/monitor.sh            # USB serial console
```

## Testing

The math engine, layout builder, graph subsystem, CAS and the unified evaluator
all have host-side unit tests that run on your development machine — no Pico
hardware and no cross-toolchain needed. **This is the primary correctness check
between hardware sessions.**

```bash
./scripts/host-tests.sh   # 21 suites, 3,203 checks
./scripts/lint.sh         # clang-format check + clang-tidy (warnings are errors)
./scripts/format.sh       # apply formatting
```

Coverage spans expression evaluation and the unified evaluator, the function
catalog (the same table the parser registers from), angle modes, number
formatting, variables and store, viewport transforms, the
function/parametric/polar/sequence point sources, trace stepping, the
mode-aware table model, lists and statistics and regression, distributions and
inference, matrices and the numeric solver, graph analysis, complex arithmetic
and display, and the CAS passes, plus the `calc` Python module's C++ side — which is
host-testable precisely because `src/scripting/calc_api.cpp` depends on
`math/` and nothing else (D74).

### On-device testing

Phase 5.1 added **serial line injection**: the firmware accepts expressions
over USB serial and reports back the result and its type, so on-device checks
are scripted rather than typed by hand.

```bash
python3 scripts/serial-console.py     # interactive
python3 scripts/ab-measure.py         # the A/B timing harness
```

Note that the serial port needs DTR asserted — `cat /dev/tty.*` will not work;
use the scripts.

Two probes are compiled out of shipped builds and enabled per-configure:
`-DPICOCALC_EVAL_PROBE=ON` for per-expression timings, and
`-DPICOCALC_STACK_PROBE=ON` for free stack at each `calc` binding entry. The
second exists because `stack: peak` is a high-water mark since boot and so
cannot answer "how much was free at *this* call" — the question D76 turned
on.

## Repository layout

```
src/
├── platform/   # HAL — the only layer that touches hardware
├── gfx/        # Framebuffer, fonts, drawing primitives
├── ui/         # Screen manager, widgets
├── math/       # Evaluator, function catalog, lists/stats/distributions,
│               #   inference, matrices, complex numbers, cas/
├── render/     # Natural math layout-node renderer
├── graph/      # Viewport, plotter, modes, point sources, trace, GraphState
└── apps/       # Screen implementations (home, graph, table, help, …)

drivers/        # Vendored C drivers and fonts — read-only, see drivers/README.md
tests/host/     # Host-side unit tests
scripts/        # build, format, lint, flash, serial, measurement
docs/           # Developer documentation — see below
```

The layer rules — what may include what, and why — are in
[docs/architecture.md](docs/architecture.md). They are enforced by review, not
by the build, so read them before adding a dependency edge.

## Developer documentation

**Start here**

- [docs/architecture.md](docs/architecture.md) — layers, ownership, the rules
- [docs/hardware.md](docs/hardware.md) — PicoCalc hardware reference: pinouts,
  the STM32 keyboard protocol, PSRAM, display
- [docs/dev-environment.md](docs/dev-environment.md) — toolchain setup
- [docs/dependencies.md](docs/dependencies.md) — what we link and why
- [AGENTS.md](AGENTS.md) — context file for AI coding agents; also the most
  compact statement of the project's conventions

**The record**

- [docs/notes/decisions.md](docs/notes/decisions.md) — the decision log (D1…),
  the single most useful file here. Every entry states the alternatives that
  were rejected and why
- [docs/notes/worklog.md](docs/notes/worklog.md) — dated session history
- [docs/notes/next-session.md](docs/notes/next-session.md) — handoff for
  whoever picks the work up next, including what is currently unverified
- [ROADMAP.md](ROADMAP.md) — phase status and the specs behind each

**Deeper**

- [docs/notes/unified-evaluator-changes.md](docs/notes/unified-evaluator-changes.md)
  — every behaviour change from the Phase 5.2 rewrite, classified
- [docs/notes/measurements/phase5.2/](docs/notes/measurements/phase5.2/) —
  on-device timing datasets, method included
- [docs/references/risch-algorithm.md](docs/references/risch-algorithm.md) —
  symbolic integration reading list, and why the integrator stops where it does
- [docs/notes/size-optimization-ideas.md](docs/notes/size-optimization-ideas.md)
  — flash/RAM levers, costed
- [docs/notes/pre-phase5-review.md](docs/notes/pre-phase5-review.md) — a
  stocktake taken before the CAS work, still the reference for RAM headroom
- [docs/notes/feasibility.md](docs/notes/feasibility.md) — the original
  feasibility analysis, kept because its estimates are worth checking against

## Conventions

- **C++17**, 4-space indent, 100-column rulers. `clang-format` and `clang-tidy`
  configs are in the repo root and CI enforces both.
- **Markdown** is validated by `scripts/validate_md.py` (math mode, links, code
  fences). Run it, or let CI tell you.
- **Commits** follow `type(scope): summary`, where scope is often a task or
  decision ID (`feat(5.2.10):`, `fix(tinyexpr):`, `docs(D53):`).
- **Branches**: `phase-N` for phase work, `fix/…`, `docs/…`, `chore/…`
  otherwise. `main` is protected by CI — build (both boards), lint, and docs
  validation must pass.

## Licensing your contribution

The project's own code is MIT; the combined firmware binary is GPL-2.0 because
of the vendored display and keyboard drivers. Contributions to `src/`, `tests/`,
`scripts/` and `docs/` are taken as MIT. See [NOTICE.md](NOTICE.md) for the
full component table — including the font headers in `src/gfx/fonts/`, which
carry their upstream fonts' licenses rather than MIT.
