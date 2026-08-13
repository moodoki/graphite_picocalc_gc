# Phase 6 Spec: Non-Calculator Functions

**Prerequisite phases**: Phase 4 (pre-release milestone — full TI-83/84+-class
GC functionality). Phase 5 (CAS) is not a hard prerequisite for Phase 6 as
a whole, but sub-phase 6B's `calc` module wants it for the `diff`/`integ`/
`solve` bindings — per the intended phase order (4 → 5 → 6), Phase 5 will
already exist by the time 6B is implemented.

**Scope**: everything that isn't graphing-calculator functionality. Where
Phases 1–4 built out a complete TI-83/84+-class calculator and Phase 5
adds symbolic math on top of it, Phase 6 is about turning the device into
a platform that can run things that aren't calculator features at all —
starting with user-written MicroPython programs. **Phase 6's sub-phases
are deliberately scoped to be completable in any order** (mod the one
structural dependency noted in §1.1) — this phase exists specifically so
that kind of open-ended, non-serialized work has a home instead of forcing
a strict week-by-week sequence the way Phases 1–5 do.

**End state**: the calculator has an app launcher distinct from its fixed
calculator screens; MicroPython programs can be written, saved, and run
on-device with access to the calculator's math/graph/matrix/complex/CAS
functions; a plain-text Notepad app ships alongside it as the app
framework's other consumer of the same shared editing widget (§3.5,
D54); and the platform is positioned to add future non-calculator apps
without needing a new phase carved out each time.

**Status**: Specced, not started. This document replaces what was Phase 4
sub-phase 4E (MicroPython) — see
[decisions.md](../notes/decisions.md) D33 for why it moved here rather
than staying in Phase 4, and [phase4-spec.md](phase4-spec.md) for the
sub-phase it vacated.

**Two dotted sub-phases now sit ahead of this one** (2026-08-08):
[Phase 5.1](phase5.1-spec.md) (serial line injection) and
[Phase 5.2](phase5.2-spec.md) (the unified evaluator). Neither is a
prerequisite in the strict sense, but two things here depend on how 5.2
lands: its tagged-`Value` sizing pass competes for the same Pico 1 headroom
6B's **48 KB MicroPython heap** needs (see §1.1 and Risk 6), and 6B's `calc`
module bindings target the evaluator 5.2 would replace. Check 5.2's status
before sizing 6B.

---

## 0. Pre-flight checklist — before starting Phase 6

Gathered 2026-08-13 from this document's own open items plus what was
already on record elsewhere (`pre-phase5-review.md`, GitHub issue #27).
Not a task list for Phase 6 itself — these are the things that should
be checked, decided, or built *before* (or right at the start of) 6A/6B
work, so scoping doesn't rest on stale numbers or unconfirmed hardware
assumptions.

### 0.1 The one that actually gates 6B — Pico 1 SRAM headroom

**Re-measure before sizing 6B's heap. Do not trust either stale
number.** `pre-phase5-review.md` (2026-08-02, **before** Phase 5 CAS and
Phase 5.2 landed) measured ~12 KB spare after the shared scratch arena.
The `picocalc-phase5.2-state` memory separately put it at ~5-10 KB
post-5.2. Those two numbers come from different points in the
codebase's history and were never directly comparable.

**Measured 2026-08-14, current `main` (`564406f`, pre-6A):**
`./scripts/size-report.sh build/pico` → **bss+data = 210,764 B
(205.8 KB) → 58.2 KB nominal headroom** on the Pico 1. This reconciles
the stale ~5-10 KB figure rather than contradicting it — that number
was already netting out the 48 KB heap (58.2 − 48 ≈ 10.2 KB), just
computed against an earlier point in the codebase.

**This is only 2.2 KB above the 56 KB threshold** (48 KB heap + 8 KB
C-stack) — with **zero 6A code written yet**. Rather than build 6A
against 48 KB and hope, **D61 (2026-08-14) pre-committed the Pico 1
heap to 40 KB now**: §4.4's table and Risk 6 (§7) reflect this as the
shipped number, not a conditional lever. **A post-6A `size-report.sh`
re-run is still owed** once 6A's static footprint actually lands — this
pre-commit removes the *scoping* risk (6B no longer depends on a number
that doesn't exist yet), it doesn't remove the need to confirm the
final margin once 6A is real.

### 0.2 Cross-phase blocker already on the issue tracker

- ~~Re-verify 6B's `calc` bindings against the post-5.2 unified
  evaluator~~ — **closed 2026-08-13, see §4.7** —
  [issue #27](https://github.com/moodoki/graphite_picocalc_gc/issues/27).
  Resolved at the design level (entry points identified, a concrete
  `calc.eval()` implementation shape recorded against 6B.3, two real
  hazards found with mitigations — list/matrix result lifetime,
  GC-triggered reentrancy). No dead entry points, no example needing a
  rewrite. Implementation-time follow-through (does the real binding
  code actually match §4.7's shape) is 6B.3's job, not a re-open of
  this issue unless something new turns up.

### 0.3 Hardware verification spike

- **P6-14 — resolved 2026-08-14 (D65), hardware-confirmed on the
  Pico 1.** Does a real power-cycle actually deassert `PICO_EN` (a true
  POR), i.e. does `watchdog_caused_reboot()` read `false` after a
  physical power-cycle? **Yes.** A new permanent diagnostic
  (`main.cpp`, `"boot: watchdog_caused_reboot=%d"` on the existing 30 s
  heartbeat cadence) measured `=1` after a non-power reboot
  (`picotool load -f -x`'s flash-and-relaunch) and `=0` after a genuine
  physical power-cycle via the case's power button. Confirms the
  schematic's prediction directly.
- **P6-5 — resolved 2026-08-14 (D66): self-sufficient, no `uf2loader`
  dependency.** No hardware spike needed after all — the question
  ("does `uf2loader`'s behavior matter to the automatic boot path")
  dissolved once the decision was made not to depend on it at all.
  `uf2loader` is now purely an optional, user-installed recovery tool.
  See §3.4 and D66 for the two corrections this surfaced (dedicated
  reset-reason marker; bootstrap must be a separate permanent
  component).
- Both P6-5 and P6-14 are now resolved — §3.4's own stated feasibility
  spike (parse one real `.uf2`, write it to a scratch flash region,
  reboot into it successfully, confirm the untouched-bootloader
  recovery path actually recovers) is what's left before §3.4
  implementation, whenever it's picked up.

### 0.4 Small build/config additions needed (no hardware required, just not done yet)

- **A version/build identifier** — `pico_set_program_version` or
  equivalent. Confirmed zero hits anywhere in `CMakeLists.txt` today.
  Needed by D59 (§3.4's self-snapshot mismatch check) and generically
  useful (this project currently has no way to ask its own firmware
  "what version is this" at all).
- **An exposed build-size symbol** — linker-provided or baked into a
  fixed flash offset at build time. Also needed by D59; also doesn't
  exist yet.
- **MicroPython embed build must include `json`** — confirmed required
  by §4.6 entry 1's periodic-table walkthrough; 6B.1's acceptance
  criteria already reflects this, listed here so it isn't only visible
  three sections deep.

### 0.5 Open policy calls (not hardware-gated, just not decided)

None currently open — the last item here (P6-12, sensor catalog) was
resolved 2026-08-14, see §0.7.

### 0.6 Already resolved this session (2026-08-13) — listed so they aren't re-litigated

D54 (editor generalized into 6A, Notepad added as 6C), D55 (file
management scoped in, `FilesScreen` generalized in place), D56 (6.1
convenience scripts: abort on error, `/picocalc/scripts/`), D57
(MicroPython heap lazily allocated), D58 (launcher: both entry points,
`ESC`-only exit routing), D59 (return-to-calculator: fetch-fresh,
lazy self-snapshot on launcher entry). Full detail in
[decisions.md](../notes/decisions.md).

### 0.7 Resolved 2026-08-14

D61 (Pico 1 MicroPython heap pre-committed to 40 KB, §0.1/§4.4/Risk 6),
D62 (P6-13: editing vendored `pwm_sound.h`/`.c` is acceptable for the
sound demo's tone extension — the README's own documented exception
process, matching the D51/tinyexpr precedent), D63 (P6-12: sensor
catalog resolved against the user's actual DHT11/DS18B20/LM393 box —
LM393 needs only generic primitives plus a new `calc.adc_read`; DHT11/
DS18B20 get dedicated C++ bindings, a narrow exception justified by
single-wire timing plus this project's GC-pause exposure), D64 (build
order: Notepad/6C ships before MicroPython/6B, proving the shared
widget on a real app first — see §1 and §5's build-order note), D65
(P6-14 hardware-confirmed on the Pico 1: a real power-cycle does
deassert `PICO_EN`, `watchdog_caused_reboot()` reliably reads false for
it — see §0.3/§3.4), D66 (P6-5 resolved: §3.4 goes self-sufficient, no
`uf2loader` dependency — surfaced that the reset-reason check needs a
dedicated marker, not bare `watchdog_caused_reboot()`, and that the
bootstrap must be a separate permanent component, not logic inside
`main()` — see §3.4). Full detail in [decisions.md](../notes/decisions.md).

---

## 1. Overview and phasing within Phase 6

| Sub-phase | Content | Depends on |
|---|---|---|
| 6A: App framework | Launcher screen, app registration/lifecycle, screen-ownership handoff, shared text-editing widget (§3.5), generalized file browser incl. management (§3.7) | Phase 4 |
| 6B: MicroPython (first base app) | Embedded interpreter, `calc` module bindings, program editor (thin wrapper on 6A's widget), SD card scripts | 6A |
| 6C: Notepad (first concrete future app) | Thin wrapper on 6A's shared text-editing widget, `.txt` files, no execution | 6A |
| 6C+: further future apps (unscoped) | Remaining candidates in §9 — additional apps riding the 6A framework, plus §9.3's home-screen convenience scripts (candidate 6.1, not app-launcher-shaped, reuses Phase 5.1's `submit_line()` instead) and §9.4's PCM sampler audio engine (candidate 6.2, real driver work, spun off from the §4.6 sound demo) | 6A (loosely) |
| Release engineering (unscoped, can run in parallel) | Docs site ([docs-site-plan.md](../notes/docs-site-plan.md)), versioned firmware releases | none |

Unlike Phases 1–5's strictly ordered sub-phases, once 6A exists, 6B, 6C,
and any further 6C+ app have no *structural* dependency on each other —
pick whichever is most wanted next, or work on more than one across
sessions without worrying about blocking order. Release engineering
doesn't even need 6A; it could start any time (see §9).

**Recommended build order, decided 2026-08-14 (D64): 6C before 6B.**
Notepad only needs 6A, not 6B (D54), and is the cheapest possible real
app — no interpreter, no `calc` bindings, no Phase 5 dependency — to
prove §3.5's shared `TextEditorWidget` end-to-end (edit → save →
power-cycle → reload, including on hardware) *before* 6B's ~66 hrs
commit to building the Python program editor as a second consumer of
it. See §5's build-order note for the resulting task sequence across
6A/6C/6B.

### 1.1 Why an app framework at all, and why before MicroPython

The original phase4-spec (as 4E) scoped MicroPython as a single fixed
"program editor screen" — one more screen bolted directly onto the
existing screen manager, the same way the matrix editor or stats screen
are. That's a reasonable design if MicroPython is the *only*
non-calculator thing this platform will ever do. But scoping this as its
own phase, with room for "ideally sub-phases that can be done in any
order," implies more than one such feature is expected eventually.
Building a small app-launcher/registration layer first (6A) means
MicroPython becomes the **first instance of an app pattern** rather than
a one-off special case — so a second non-calculator feature later slots
into the same launcher instead of needing its own bespoke integration
into `apps/nav.{hpp,cpp}`.

6A is deliberately scoped small. This is **not** TI's Apps ecosystem —
sideloadable binaries, a computer-link install flow, an app store (see
the [parity doc](../notes/ti-parity.md)'s "deliberately not
chasing TI" list, which already rules this out). It's an in-firmware
launcher screen plus a lightweight registration table: closer to "a
second menu of screens, entered from Home" than a real OS with
dynamically loaded code. Every app in scope here ships compiled into the
same firmware image as everything else.

---

## 2. New source files

```
src/
├── apps/
│   ├── launcher_screen.hpp / .cpp  # NEW (6A): app launcher/menu screen
│   └── program_screen.hpp / .cpp   # NEW (6B): MicroPython editor + runner
│                                     #   (moved here from the old 4E plan)
├── platform/
│   └── app_registry.hpp / .cpp     # NEW (6A): app registration table + lifecycle
└── scripting/
    ├── micropython_embed.hpp/.cpp  # NEW (6B): MicroPython interpreter wrapper
    ├── calc_module.hpp / .cpp      # NEW (6B): Python 'calc' module (C++ bindings)
    └── script_runner.hpp / .cpp    # NEW (6B): load + execute .py from SD card
```

---

## 3. Sub-phase 6A: App framework

### 3.1 App registry

```cpp
namespace platform {

// One entry per launchable app. Apps are statically registered at boot
// (compiled-in, not dynamically loaded — see §1.1).
struct AppEntry {
    const char* name;          // "MicroPython", shown in the launcher
    const char* icon_glyph;    // optional single-glyph icon (font slot map)
    void (*launch)();          // hands control to ui::ScreenManager
};

class AppRegistry {
public:
    static void register_app(const AppEntry& entry);
    static int count();
    static const AppEntry* get(int index);
};

}  // namespace platform
```

Registration happens once at boot (`main.cpp`, alongside the existing
catalog/screen-manager setup) — each app's translation unit calls
`AppRegistry::register_app` for itself, the same pattern the function
catalog already uses to stay a single source of truth (README: *"function
catalog driven by the same table the parser registers from"*).

### 3.2 Launcher screen

**Resolved 2026-08-13 (D58, P6-3): both.** Entered from Home via **a
dedicated softkey and a typed command** (`apps`/`app`, mirroring the
`lists`/`stats` typed-command convention from Phase 3) — not
either/or, both entry points ship. `UP`/`DOWN` selects, `ENTER`
launches. This is intentionally the simplest possible menu — a
vertical list, no icons grid, no categories — because with one app
(MicroPython) at launch there's nothing to organize yet. Revisit the
layout once a second or third app exists (§9).

If §3.4's compiled-app stretch goal is ever built, its `on_activate`
is also where P6-6's version-stamp check (and, on mismatch, the
firmware self-snapshot to `/picocalc/firmware.uf2`) hooks in — no new
lifecycle needed, just an addition to a hook 6A.2 already has.

### 3.3 Screen-ownership handoff

Apps take over the full screen the same way any existing full-screen
`apps/*` module does (matrix editor, stats screen, etc.) — there's no
new windowing/compositing model. The only new behavior is *how you get
there*: through the launcher (§3.2's softkey or command), and *how you
leave*: a consistent "exit to launcher" convention — `ESC` returns to
the launcher, not directly to Home, so a user who came from the
launcher doesn't lose their place. **Resolved 2026-08-13 (D58, P6-4):
`ESC` only.** `HOME` keeps its existing system-wide behavior unchanged
— it still short-circuits straight to the home screen from any screen,
apps included, the same as every other screen today. Two distinct
keys, two distinct destinations: `ESC` is the "step back" convention,
`HOME` is the global "start over" convention, and apps don't get a
special case on the latter.

### 3.4 Compiled app launcher entries (self-sufficient reboot-based, **stretch — not core 6A scope**)

Promoted here from a deferred-future-phase candidate on 2026-07-21 (D34
follow-up) once it became clear this can plausibly appear as **a third
`AppEntry` kind, selected from the same launcher as everything else** —
not a separate reboot-into-a-different-menu experience. Explicitly
**stretch**: not part of 6A's committed 14 hrs, attempted only if there's
appetite after the core sub-phase ships, same status as the old
`program_screen`'s "syntax highlighting is a stretch goal."

**Mechanism**: each compiled app is a complete, independently built
`.uf2` — same fixed-address pico-sdk toolchain, no relocation, no PIC,
no shared-ABI table with the running calculator firmware (it never runs
concurrently with it, see §9.1 for why that matters). A manifest entry
(§4.5's format, extended with `type=native`) points at the `.uf2` file
on SD instead of a `.py` entry script. Selecting it in the launcher:

1. Parses the `.uf2` directly off the SD card — UF2 is a simple,
   well-specified block format (512-byte self-describing blocks: magic
   numbers, target address, payload, block count), designed from the
   ground up to be robust against partial reads/writes. This is real
   work, but it's implementing a well-specified format, not inventing a
   write-safety protocol from scratch — a materially smaller lift than
   the "homegrown write-verify-activate protocol for an arbitrary
   relocatable blob" that in-process loading would need (§9.1).
2. Writes the parsed payload into a **reserved app-boot flash region**,
   using the Pico SDK's flash write primitives (code executing this step
   must run from RAM, not flash, while flash is being written — the same
   constraint this project already documents for other flash operations).
3. Triggers a reset. On boot, a small, standalone bootstrap component —
   this project's own, **resolved 2026-08-14 (D66): self-sufficient,
   not `uf2loader`-dependent** — auto-boots whatever's in that region:
   the freshly written app, with no interactive menu step, because the
   *selection* already happened in our own launcher. `uf2loader` is not
   part of this path at all; see the safety-net paragraph below for
   where it fits instead.
4. **Returning to the calculator** is the same operation run in reverse:
   re-flash the calculator's own image into that region and reset. Apps
   built from this project's own template know how to do this (a `calc`
   module binding, analogous to 6B's other bindings); a completely
   foreign `.uf2` would not, and would rely on the hold-a-key-at-boot
   fallback below instead. **Resolved 2026-08-13 (D59, P6-6): fetched
   fresh from a known SD path** (`/picocalc/firmware.uf2`, matching the
   top-level-singleton-file convention `graphstate.dat` already uses),
   not bundled per-app — see P6-6 for the full mechanism (self-snapshot,
   checked lazily on launcher entry).

**What this does and doesn't remove, versus §9.1's in-process approach**:
removes the relocator/PIC problem (#2) and the ABI/symbol-table
versioning tax (#4) and the concurrent-execution memory-protection
problem (#3) entirely — nothing runs concurrently, so there's nothing to
protect or version against. It does **not** remove the need for a real,
carefully tested flash-write step — that's still genuine risk, just a
substantially smaller and better-specified version of it (a known block
format, not an arbitrary blob).

**Safety net, not a nice-to-have**: the reserved app-boot region must be
strictly separate from wherever this project's own bootstrap (D66)
lives. If the self-flash routine has a bug, the worst case must be "the
app slot is corrupted, hold the boot key and reflash from SD via the
untouched bootloader" — never "the device won't boot at all." This
constraint should be treated as non-negotiable in any implementation,
not an optimization. **`uf2loader` lives here now, not in the automatic
path (D66)**: purely optional, user-installed, reached only by
deliberately holding a boot key — an extra personal safety net a user
may add, never something the calculator's own logic checks for or
depends on.

**A power cycle recovers to the calculator automatically — even from a
hung third-party app (2026-08-13 schematic read, confirmed on hardware
2026-08-14 — D65)**: the mainboard's POWER section
(`clockwork_Mainboard_V2.0_Schematic.pdf`) shows `U101`, an **AXP2101**
PMIC, with the physical power button wired into its `PWRON` pin — a
*soft* power sequence, not a raw battery-voltage cutoff — and a
dedicated `PICO_EN` line gating a regulator (`U102`) whose output is
named `PICO_VSYS`, the Pico module's own supply rail. That topology
predicted that a real power-off fully removes the Pico's VDD, making
the next power-on a genuine hardware power-on reset (POR) —
indistinguishable from unplugging a standalone Pico.

**Confirmed on the Pico 1, 2026-08-14 (P6-14, D65)**: `watchdog_caused_reboot()`
read `false` after a genuine physical power-cycle (button off, wait,
button on — USB observably dropped and reappeared ~13 s later) and
`true` after a non-power reboot. The actual off-sequencing lives in the
STM32 keyboard MCU's firmware, so this couldn't be settled from the
schematic alone — it needed exactly this hardware measurement, and now
has one. This gives a clean mechanism with **no app cooperation required**: use
`watchdog_reboot()` (not a generic reset) for the deliberate
launch-into-app handoff in step 3 above, and have the bootstrap check
reset reason on every boot — **app slot marker present → boot the app
slot; anything else (a real power cycle, including a POR from a hung
app forcing the user to physically power-cycle; or a watchdog reboot
for an unrelated reason) → always boot the calculator, regardless of
what's sitting in the app slot.** Because this is a bootstrap decision
keyed on hardware reset reason, not something the app itself has to
implement or call back into, it works uniformly for **every app,
including completely foreign third-party ones that know nothing about
this project's `calc` return binding.**

**Correction, 2026-08-14 (D66): bare `watchdog_caused_reboot()` is not
enough on its own** — it's already shared by D47's hard-fault recovery
reboot, the bulk-PSRAM self-test's watchdog guard, and (measured via
D65) even an ordinary `picotool load -f -x` flash-and-relaunch. As
first written above, a hard fault inside the calculator itself would
misread as "boot the app slot." The fix matches this codebase's own
existing pattern (`fault.cpp`'s `g_crash.magic`, `main.cpp`'s
`kBulkTestMarker`): write a dedicated marker to a free watchdog scratch
register (`scratch[2]`/`[3]` — `[0]`/`[1]` are the bulk test's,
`[4]-[7]` are boot-ROM-reserved) immediately before the deliberate
`watchdog_reboot()` call, and have the bootstrap check that marker, not
the bare flag.

**Also D66: the bootstrap must be a genuinely separate, permanent
component, not logic embedded in the calculator's own `main()`.** The
"always recovers to the calculator" guarantee above can't be satisfied
from inside the calculator's own image when an app is what's currently
resident in the boot region — the calculator's code isn't running to
make that check in that case. Something has to run first, on every
reset, regardless of what's currently flashed — a small, standalone
bootstrap binary at the true reset vector, distinct from both the
calculator and any app. This is more new engineering than "a few lines
in `main()`" (its own linker script, its own flash placement, a
one-time install step on a fresh device) — §3.4's ~25-35 hr estimate
predates this and should be revisited when §3.4 is actually scoped for
implementation.

This also meaningfully improves the safety-net paragraph above: a
hung/misbehaving app's *common-case* recovery becomes "just power-cycle
it," not "remember to hold the boot key" — hold-a-key stays as the
fallback only for the rarer case of a corrupted app-slot write itself.

**Feeds into P6-5, doesn't resolve it**: this reset-reason check needs
to run as custom decision logic very early in boot — a step that leans
toward the self-sufficient option, since a generic third-party
bootloader wasn't designed with this project's specific "app slot vs.
calculator" concept and may not expose a hook for it. Not a resolution
of P6-5, just a new consideration for whichever way it's decided.

**Open questions before implementation**:

- ~~Does this depend on `uf2loader` being separately installed... or
  does the calculator firmware become fully self-sufficient~~ —
  **resolved 2026-08-14 (D66): self-sufficient.** No dependency on
  `uf2loader`'s behavior anywhere in the automatic path — it's demoted
  to a purely optional, user-installed, manually-invoked recovery tool.
  Working through what "self-sufficient" requires surfaced two
  corrections, both folded into the mechanism/safety-net text above:
  bare `watchdog_caused_reboot()` is ambiguous with other reboot causes
  already in this codebase and needs a dedicated marker; and the
  bootstrap must be a genuinely separate, permanent component (not
  logic inside the calculator's own `main()`), which the ~25-35 hr
  estimate below predates.
- ~~Where does the calculator's own `.uf2` come from at "return"
  time~~ — **resolved, see P6-6**: fetched fresh from
  `/picocalc/firmware.uf2`, kept in sync by the running firmware
  self-snapshotting there. Two prerequisites this codebase doesn't have
  yet: an exposed build-size symbol, and a version/build identifier to
  gate the write (`pico_set_program_version` or similar — not currently
  set anywhere in `CMakeLists.txt`).

**Rough estimate**: **~25–35 hrs — likely understated as of D66**
(2026-08-14): this predates working through what "self-sufficient"
requires (a genuinely separate, permanent bootstrap component, not a
few lines in `main()`). Revisit when §3.4 is actually scoped. Gated by
a feasibility spike first
(parse one real `.uf2`, write it to a scratch flash region, reboot into
it successfully, confirm the untouched-bootloader recovery path actually
recovers) — flash-write code should prove itself in isolation before the
rest is built on top of it, the same "spike before committing" principle
§9.1 recommends for anything touching a homegrown flash/loader path.

### 3.5 Shared text-editing widget

**Added 2026-08-13 (D54).** §4.3's program editor was originally scoped
as a single Python-specific screen. Nothing about line-numbered text
editing — buffer, cursor, arrow-key navigation, insert/backspace/delete
— is Python-specific, and §3.6's Notepad app is a second, concrete
consumer of the same behavior. Per Risk 10's own guardrail ("expand only
when a second app actually needs more"), that's now true, so the widget
moves here rather than staying private to 6B.

```cpp
namespace ui {

// Generic line-numbered text editor. Owns the buffer, cursor, and
// arrow-key/insert/backspace/auto-indent behavior. Callers configure
// everything app-specific: file extension and save directory, which
// softkeys appear and what they do, and whether a character triggers
// auto-indent on ENTER.
struct TextEditorConfig {
    const char* save_dir;       // e.g. "/picocalc/programs" or "/picocalc/notes"
    const char* file_ext;       // e.g. ".py" or ".txt"
    char auto_indent_after = 0; // 0 = disabled; ':' for Python
    bool has_run_key = false;   // F1:RUN shown/wired only if true
    void (*on_run)(const char* path) = nullptr; // called with saved path
};

class TextEditorWidget {
public:
    void configure(const TextEditorConfig& cfg);
    void on_activate();                       // load last buffer, or blank
    bool on_key(const platform::KeyEvent& ev); // nav/edit/softkeys
    void render(gfx::Framebuffer& fb);
    // F3:LOAD opens the file browser (§3.7) scoped to save_dir/file_ext
    // and returns the chosen path here.
};

}  // namespace ui
```

Two screens each own an instance, configured differently: §4.3's Python
program editor (`has_run_key = true`, `.py`, auto-indent after `:`,
`on_run` calls `PythonInterpreter::exec_file()`) and §3.6's Notepad
(`has_run_key = false`, `.txt`, no auto-indent). Syntax highlighting
(§4.3's stretch goal) stays a Python-editor-only concern layered on top
of the shared widget's render pass, not part of the widget's core.

### 3.6 Notepad app

**Added 2026-08-13 (D54).** First concrete 6C app — promoted out of
§9.2's unscoped candidates because it's now near-free once §3.5 exists.
A thin `TextEditorWidget` wrapper: `.txt` files under
`/picocalc/notes/`, no `RUN` softkey, registered with `AppRegistry` as
"Notepad". Depends only on 6A (§3.5 + the launcher), **not** 6B — it
needs no MicroPython interpreter and nothing from Phase 5, so it isn't
affected by issue #27's note that 6B's `calc` bindings need
re-verifying against the post-5.2 evaluator.

Notepad's `F3:LOAD` uses §3.7's generalized file browser in picker mode,
scoped to `save_dir`/`file_ext` — same call §3.5's widget makes for the
Python editor (D55 resolved this; see §3.7).

### 3.7 Generalized file browser (D54 raised it, D55 scoped it in)

`FilesScreen` (`src/apps/files_screen.cpp`) exists today but is a
single-level, read-only diagnostic added 2026-07-18 (test-drive
request) — hardcoded to `/picocalc`, no `ENTER`-to-descend into `[DIR]`
rows, no picker calling convention, and `platform::Storage` has no
`rename`/`delete_dir`. **D55: generalized in place** rather than
duplicated — one component serves the standalone `Files` diagnostic,
§3.5's widget `F3:LOAD`, and Notepad's `F3:LOAD`, in two modes:

```cpp
namespace apps {

enum class FileBrowserMode { kBrowse, kPick };

struct FileBrowserConfig {
    FileBrowserMode mode = FileBrowserMode::kBrowse;
    const char* start_dir = "/picocalc"; // kPick: e.g. save_dir from
                                          // TextEditorConfig (§3.5)
    const char* ext_filter = nullptr;    // kPick: e.g. ".py"; nullptr = no filter
    // kPick only: called with the chosen file's full path. kBrowse
    // never calls this — ENTER on a file is a no-op there (view only).
    void (*on_picked)(const char* path) = nullptr;
};

class FileBrowserScreen : public ui::Screen {
public:
    void configure(const FileBrowserConfig& cfg);
    // ... on_activate/on_key/render as today, plus:
    // - ENTER on a [DIR] row descends; a bound key returns to parent
    //   (tracks a path stack, capped at depth 4 — added 2026-08-14:
    //   /picocalc/{programs,notes,apps/<name>} is 3 levels deep at
    //   its deepest, so 4 leaves one spare level of user-created
    //   subfolder before the cap bites, without sizing the stack for
    //   arbitrary nesting nothing on this SD layout needs).
    // - Management, available in both modes: DEL (confirm-gated) calls
    //   Storage::delete_file/delete_dir; a rename key reuses the
    //   existing filename text-entry (same prompt §4.3's F4:NEW uses);
    //   a new-folder key calls Storage::ensure_dir.
};

}  // namespace apps
```

`platform::Storage` gains two primitives to support management:

```cpp
// Renames/moves a file or directory. Returns false if the destination
// exists or the source doesn't.
bool rename_file(const char* old_path, const char* new_path) const;

// Removes an empty directory. Returns false if it doesn't exist or is
// non-empty — deliberately non-recursive (D55): emptying a populated
// directory is a separate, explicit step, not a side effect of one
// delete keypress.
bool delete_dir(const char* path) const;
```

The standalone `Files` diagnostic entry point keeps its current
behavior as `FileBrowserMode::kBrowse` with `start_dir = "/picocalc"` —
D55 generalized the component, not the diagnostic's defaults — and
gains navigation and management for free since it's the same code path
§3.5/§3.6 call in `kPick` mode.

---

## 4. Sub-phase 6B: MicroPython programming (first base app)

Unchanged in substance from the original 4E plan — only the entry point
changes (launched via 6A's launcher rather than a direct Home softkey)
and the framing (this is now explicitly *an app*, not a fixed calculator
mode).

### 4.1 Embedding strategy

MicroPython provides an `embed` port specifically designed for hosting
MicroPython inside a larger C/C++ application. The firmware includes the
MicroPython interpreter as a library, not a standalone runtime.

```cpp
namespace scripting {

class PythonInterpreter {
public:
    // Initialize the MicroPython runtime.
    // heap_size: bytes allocated for the Python heap.
    //   Pico 1: 40 KB from SRAM (D61, 2026-08-14 — pre-committed ahead
    //     of 6A landing; see phase6-spec.md §0.1)
    //   Pico 2: 96 KB from SRAM
    // PSRAM is NOT directly usable as Python heap (too slow for
    // GC scanning), but individual large allocations can be proxied.
    bool init(size_t heap_size);

    void shutdown();

    // Execute a Python string (single statement or block).
    // Returns true if execution succeeded.
    // Output is captured and passed to output_callback.
    bool exec(const char* code);

    // Execute a .py file from the SD card.
    bool exec_file(const char* path);

    // Register a C function as a Python built-in.
    // Used to expose calculator APIs to scripts.
    void register_function(const char* module, const char* name,
                           void* c_func);

    // Check if interpreter is initialized.
    bool is_running() const;

    // Set callback for print() output.
    using OutputCallback = void (*)(const char* text);
    void set_output_callback(OutputCallback cb);

    // Set callback for input() requests.
    using InputCallback = const char* (*)(const char* prompt);
    void set_input_callback(InputCallback cb);

private:
    bool initialized_ = false;
};

} // namespace scripting
```

### 4.2 Calculator API bindings (`calc` Python module)

User scripts import a `calc` module that exposes calculator functionality:

```python
import calc

# Expression evaluation
result = calc.eval("2 + 3 * sin(pi/4)")
# Numeric bracketed root-find on an arbitrary expression string — the
# expression language's own solve(f,x,lo,hi) (D28, math::numeric_solve),
# reachable through eval(). Prefer this over calc.solve (below) whenever
# the equation isn't symbolically solvable (§4.6 entry 3, TVM): it takes
# no named Y-slot, so it can't clobber the user's own Y1-Y7 graphs.
i_rate = calc.eval("solve(pv+pmt*n+fv, n, 1e-6, 1)")

# Variables
calc.store("A", 42)
val = calc.recall("A")

# Graphing
calc.plot("sin(x)", color="blue")
calc.plot("cos(x)", color="red")
calc.window(-10, 10, -2, 2)
calc.show_graph()

# CAS operations (Phase 5 — see prerequisite note at top of this doc)
d = calc.diff("x^3 - 2*x", "x")    # Returns "3*x^2 - 2"
i = calc.integ("sin(x)", "x")       # Returns "-cos(x)"
s = calc.solve("x^2 - 4 = 0", "x") # Returns ["-2", "2"]
s2 = calc.solve("x^2 + 1 = 0", "x")# Returns ["i", "-i"] (complex-aware)
f = calc.factor("x^2 - 4")          # Returns "(x-2)*(x+2)"

# Complex numbers
z = calc.complex(3, 2)              # 3 + 2i
mag = calc.c_abs(z)                 # 3.606...
ang = calc.c_arg(z)                 # 0.588...
cj = calc.c_conj(z)                 # 3 - 2i

# Graph analysis (numeric)
root = calc.graph_zero("Y1", -5, 5)      # numeric root in bracket
mx   = calc.graph_max("Y1", 0, 10)       # local maximum
area = calc.graph_integral("Y1", 0, 3.14)# numeric definite integral
slope= calc.graph_deriv("Y1", 2.0)       # numeric dy/dx at x=2

# Matrix operations
m = calc.matrix([[1, 2], [3, 4]])
det = calc.det(m)
inv = calc.inverse(m)

# Lists (from Phase 3)
calc.set_list(1, [1, 2, 3, 4, 5])
mean = calc.stat_mean(1)
# Incremental write, added per §4.6's data-logging walkthrough: writes
# one sample directly into list storage (PSRAM-backed, outside the
# Python heap — Risk 6) instead of growing a Python list in-heap over
# an open-ended logging run.
calc.list_append(1, sample_value)

# GPIO/ADC/1-Wire (§4.6 entry 2, D63 — added per the user's actual
# sensor box: LM393-comparator boards need only the generic primitives;
# DHT11/DS18B20 get dedicated bindings, see D63 for why)
calc.gpio_mode(28, "in")
level = calc.gpio_read(28)          # LM393 digital threshold output
volts = calc.adc_read(28)           # LM393 analog tap, if present
t, h = calc.dht11_read(28)          # (temperature_c, humidity_pct)
temp_c = calc.ds18b20_read(28)      # single device on the bus (v1)

# Display — color is a named string (the existing platform::colors
# palette) OR an (r,g,b) tuple, added per §4.6's periodic-table
# walkthrough: ~10 category colors don't fit the ~6-entry named
# palette, and C++ already has Color::from_rgb() to bind straight to.
calc.clear_screen()
calc.draw_text(10, 10, "Hello from Python!")
calc.draw_line(0, 0, 319, 319, "white")
calc.draw_rect(50, 50, 100, 80, "blue", fill=True)
calc.draw_rect(50, 50, 100, 80, (255, 140, 0), fill=True)

# Input
key = calc.wait_key()               # blocks until a key is pressed
text = calc.input("Enter value: ")  # blocks, line-edited
# Non-blocking (added per §4.6 entry 5's real-time-game walkthrough):
# calls platform::Keyboard::poll() once and returns immediately.
key = calc.key_pressed()            # key code, or None if nothing new
down = calc.key_held("left")        # True/False, wraps Keyboard::is_held

# File I/O (SD card)
calc.write_file("/picocalc/data.txt", "hello")
content = calc.read_file("/picocalc/data.txt")
```

Each `calc.*` function is a thin C++ wrapper that calls into the existing
`math::Engine`, `math::cas::*` (Phase 5), `math::Matrix`,
`platform::Display`, `platform::Keyboard`, and `platform::Storage`
classes.

### 4.3 Program editor screen

**As of D54, this is a thin `ui::TextEditorWidget` (§3.5) wrapper**, not
its own editor implementation — the buffer/cursor/nav/edit behavior
below now lives in 6A, and this section describes 6B's configuration of
it (`has_run_key = true`, `.py`, `/picocalc/programs/`, auto-indent
after `:`) plus the Python-only pieces (RUN wiring, syntax highlighting
stretch). Launched from 6A's app launcher rather than a Home softkey:

```
┌──────────────────────────────────┐
│  Edit: program.py                 │
├──────────────────────────────────┤
│  1│ import calc                    │
│  2│                                │
│  3│ for i in range(10):            │
│  4│   y = calc.eval(f"sin({i})")   │
│  5│   calc.draw_text(10, i*20, str │
│  6│ |                              │
│  7│                                │
│                                    │
├──────────────────────────────────┤
│ F1:RUN F2:SAVE F3:LOAD F4:NEW    │
└──────────────────────────────────┘
```

**Features** (from §3.5's shared widget, unless marked Python-only):

- Line-numbered display with cursor (row + column)
- Arrow key navigation, `ENTER` inserts newline
- `BACKSPACE` / `DEL` work as expected
- Auto-indent on `ENTER` after `:` — 6B configures the trigger char; the
  widget itself is trigger-agnostic (Notepad, §3.6, configures none)
- `F1` (RUN, **Python-only**, `has_run_key = true`): save current buffer,
  execute via `PythonInterpreter::exec_file()`
- `F2` (SAVE): write to `/picocalc/programs/<name>.py` (the widget's
  configured `save_dir`/`file_ext`)
- `F3` (LOAD): §3.7's `FileBrowserScreen` in `kPick` mode, scoped to
  `/picocalc/programs/`, `.py` only
- `F4` (NEW): clear buffer, prompt for filename
- Syntax highlighting is a stretch goal, **Python-only** — basic keyword
  coloring (`import`, `def`, `for`, `if`, `while`, `return`) layered on
  top of the shared widget's render pass, if time permits

**Output capture**: when a script runs, `print()` output goes to a
scrollable output pane that replaces the editor temporarily. Press
`ESCAPE` to return to the editor (per 6A's convention, `ESCAPE` from the
top-level program screen returns to the launcher). Errors display with
the line number and exception message.

### 4.4 Memory budget for MicroPython

| Component | Pico 1 (SRAM) | Pico 2 (SRAM) |
|-----------|---------------|---------------|
| MicroPython interpreter + stdlib | ~60 KB flash | ~60 KB flash |
| Python heap (GC-managed) | **40 KB** (D61, 2026-08-14) | 96 KB |
| C stack for Python calls | 8 KB | 8 KB |
| **Total SRAM impact** | **~48 KB** | **~104 KB** |

On Pico 1, a fresh 2026-08-14 measurement (§0.1) puts pre-6A headroom at
58.2 KB — this leaves **~10 KB** of that free for 6A's own static
footprint plus whatever else lands before the heap allocates, after the
48 KB → 40 KB heap cut. Tight, but workable, because the CAS pool lives
in PSRAM and the framebuffer uses line-buffer rendering, and because the
cut was taken deliberately rather than discovered as a shortfall. The
MicroPython interpreter is only initialized when the user enters the
program screen — it doesn't consume memory when doing normal calculator
work. **Formalized as D57 (§8 P6-1): lazy allocation** — the Python heap
comes up on entering the program/app screen and is freed on leaving
(6B.14), not reserved at boot; this applies to any SD-discovered Python
app (§4.5) too, since they all run through the same `PythonInterpreter`.
Notepad (6C) never touches this budget at all — it has no interpreter.
**A post-6A `size-report.sh` re-run (§0.1) is still owed** once 6A's
static footprint actually exists, to confirm the ~10 KB estimate above
against 6A's real cost rather than the low-single-digit-KB comparables
D61 reasoned from.

On Pico 2, it's comfortable — 520 KB SRAM minus ~104 KB for Python minus
~200 KB for framebuffer still leaves ~216 KB.

**A data point from §4.6's periodic-table walkthrough**: a ~118-record
dataset parsed into Python objects (dicts/lists from `json.loads`) is a
meaningful, non-disqualifying slice of the Pico 1's 48 KB budget —
rough order tens of bytes/object overhead plus string data likely lands
in the low single-digit KB, but this is an estimate, not a measurement.
Worth an actual `gc.mem_free()` check once 6B.1 exists, as the first
real test of "how much of the 48 KB does a modest reference dataset
actually cost," not just working-variable/script-buffer usage.

### 4.5 SD-discovered app manifests

Accepted into 6B's scope on 2026-07-21 (was scoped as a §9 "candidate,"
promoted once the complexity assessment showed it's low-risk — see D34).
Extends 6B so that scripts under `/picocalc/apps/<name>/` show up as
their own named tiles in 6A's launcher, instead of every script being
reached through one generic program-editor "load" flow.

```cpp
namespace platform {

// A second, dynamically populated tier of AppRegistry entries (§3.1),
// scanned once at boot from the SD card. Bounded, no heap allocation —
// same fixed-capacity-table shape as ArrayStore's slabs.
struct SdAppManifest {
    char name[24];
    char icon_glyph[8];   // optional, UTF-8 glyph or empty
    char entry_path[64];  // e.g. "/picocalc/apps/finance/main.py"
};

// Scans /picocalc/apps/*/app.txt (flat key=value: name=, icon=, entry=),
// populates up to kMaxSdApps entries, registers each with AppRegistry.
// Malformed manifests are skipped and logged, never fatal.
void scan_sd_apps();

}  // namespace scripting
```

Each discovered entry's `launch()` calls
`PythonInterpreter::exec_file(entry_path)` — exactly 6B's existing
program-runner path (§4.1), just reached from a named launcher tile
instead of the generic editor's file browser. No new execution model, no
new failure mode beyond "bad manifest, skip it."

### 4.6 Candidate apps used to pressure-test the `calc` module (running list)

**Added 2026-08-13.** §4.2's `calc` module was written speculatively,
before any concrete app tried to use it. Walking through real app ideas
against it — what's already sufficient vs. what's actually missing — is
cheaper now than discovering gaps mid-6B. Entries accumulate here as
they come up; each records what it needed, what §4.2 already covered,
and what changed because of it.

**1. Periodic table** (SD-discovered app, §4.5:
`/picocalc/apps/periodic/{app.txt, main.py, elements.json}`)

- **Already sufficient**: `calc.clear_screen`/`draw_rect`/`draw_text`
  for the ~18-column grid (fits at ~17 px/cell with the small font),
  `calc.wait_key` for cell navigation, `calc.read_file` for loading the
  data file.
- **Data source: SD JSON, parsed in Python** — decided over a native
  `calc.element()` binding or an embedded Python literal, to keep the
  data user-editable with no firmware rebuild, matching how
  `/picocalc/apps/*` is meant to work generally. Costs: 6B.1 must build
  the embed lib with `json` in scope (§5's task row updated), and the
  parsed dataset is a real, if modest, draw on the Python heap (§4.4).
- **New requirement surfaced**: draw primitives need **RGB tuples**, not
  just the ~6-entry named color palette — added to §4.2's `calc.draw_*`
  signatures. Category coloring (alkali metals, halogens, noble gases,
  …) needs roughly 10 distinct colors.
- **Not yet needed**: no grid/table-layout helper in `calc` itself —
  plain per-cell `draw_rect`/`draw_text` calls in a Python loop are
  expected to be fine for a one-time render (~118 cells, not a hot
  loop). Revisit only if that turns out too slow on hardware.

**2. Sensor / data-logging app (GPIO and/or I2C → lists)**

**P6-12 (sensor catalog) resolved 2026-08-14 (D63)** against the user's
actual sensor box — **DHT11**, **DS18B20**, and assorted
**LM393-comparator** breakout boards (sound/IR/tilt/flame/raindrop-class
Arduino hobbyist modules). See the "calc surface" bullet below for what
this changed. **The pin map (P6-11) is fully answered**, cross-checked
against two primary sources read directly 2026-08-13:
`clockwork_Mainboard_V2.0_Schematic.pdf` (PICO section, J301/J302 — the
Pico module's own 40-pin socket) and the official
`Clockwork_PicoCalc_Assembly_Guidelines.pdf`'s "The Interfaces" page,
which independently list the same two external headers pin-for-pin.
Full physical port row (top to bottom, per the manual): 3.5mm
headphone, USB-C charging, power key (top edge); SD card (right side);
volume knob plus these two GPIO headers (left side):

- **Core GPIOs** header: `3V3_OUT, GP2, GP3, GP4, GP5, GP21, GP28, GND`
- **Mainboard GPIOs** header: `3V3_OUT, UART0_RX, UART0_TX, UART1_RX, UART1_TX, USB_DP, USB_DM, GND`

Cross-referenced against the schematic's full Pico GPIO map (every pin
not on either header above: GP6/GP7 = I2C1 keyboard bus; GP10–GP13 =
SPI1 LCD bus; GP14/GP15 = LCD_DC/LCD_RST; GP16–GP19 = SPI0 SD card;
GP22 = SD_DET card-detect, matching this project's own D26; GP20 = PSRAM
CS; GP23/24/25/29 aren't header pins on any RP2040/2350 board at all;
GP26/GP27 = audio PWM) and against this repo (`CMakeLists.txt:316-317`:
`pico_enable_stdio_usb(...1)` / `pico_enable_stdio_uart(...0)`, and no
`uart_init`/UART1 call anywhere in `src/`or `drivers/`), the two headers
split into three tiers, not one free-vs-committed line:

| Header pins | Status |
|---|---|
| **GP28** | Truly free — no peripheral, no debug circuit, nothing claims it at all |
| **UART0_RX/TX (GP0/1), UART1_RX/TX (GP8/9)** | Unclaimed by *this firmware* (stdio is USB-only, UART is never initialized) — but physically wired to on-board debug-bridge circuitry (CH340C, analog switches); the project wiki's "Setting Up Arduino Development for PicoCalc keyboard" page suggests UART1 may double as the STM32 keyboard MCU's reflash/debug path, so repurposing isn't risk-free even though the calculator firmware itself doesn't touch it |
| **GP2/GP3/GP4/GP5/GP21** | Hard-committed to PSRAM (quad-SPI data + SCK) — present on the "Core GPIOs" header electrically, but touching them risks corrupting PSRAM traffic |
| **USB_DP/USB_DM** | Shared with the main USB-C port's alternate routing (the DEBUG section's SEL1/SEL2 analog-switch mux) — not independent spare pins |

**No second I2C bus is possible** regardless of tier — there are no
spare pins for one anywhere on the Pico. Any I2C sensor shares I2C1
with the keyboard controller (different address, same bus, same timing
risk flagged below), because that's the only I2C peripheral this
hardware has, not a choice. **GPIO-only sensors have one clean pin**
(GP28, also one of the three ADC-capable pins) plus two riskier ones
(UART0/UART1, usable but not firmware-free of consequence).

- **Goal**: read sensor data over GPIO and/or I2C, log samples over
  time, save into calculator lists so existing list tools (§4.2's
  `calc.set_list`/`calc.stat_mean`, the stats/plot screens from Phase 3)
  work on the result unmodified — no new analysis path needed, only a
  new data-entry path into the same lists.
- **New HAL layer, not just new `calc` bindings**: per this project's
  strict HAL discipline (D-prelude-2 — app code never touches the Pico
  SDK directly), this needs a `platform::Gpio` wrapper — GP28 cleanly,
  UART0/UART1's pins usable but not risk-free (three-tier table above)
  — and I2C access through the **existing**
  keyboard/south-bridge `I2cBus` — a second bus is off the table, there
  are no spare pins for one. Sharing that bus with arbitrary third-party
  sensor traffic risks the same class of timing problem D7 already
  solved once for keyboard polling (a blocking 16 ms sleep was
  unacceptable there; unpredictable sensor-library blocking on a shared
  bus is worse, and this time it's user-authored Python, not a vendored
  driver) — this needs a real mitigation (e.g. bus access serialized
  through core 0's existing poll loop, sensor I2C transactions bounded
  the same way key-drain and stdin-poll already are, per §3.2 of
  [phase5.1-spec.md](phase5.1-spec.md)) before this is safe to build,
  not an open question to defer.
- **`calc` surface would be generic bus primitives, not per-sensor
  drivers, for most of the catalog**: `calc.gpio_mode(pin, mode)`,
  `calc.gpio_read(pin)`, `calc.gpio_write(pin, value)`,
  `calc.adc_read(pin)` (added 2026-08-14, D63 — GP28 was already flagged
  ADC-capable but nothing bound it; needed for LM393 boards that expose
  an analog tap rather than a bare digital threshold), `calc.i2c_scan()`,
  `calc.i2c_read(addr, reg, len)`, `calc.i2c_write(addr, reg, data)`.
  Specific sensor support becomes pure-Python glue on top (e.g. a
  `bme280.py` helper script), the same "platform provides primitives,
  scripts provide behavior" split 6B already uses everywhere else. **The
  user's LM393-comparator boards land squarely here** — digital
  threshold output (occasionally an analog tap), no protocol, fully
  covered by `gpio_read`/`adc_read` with zero new C++ work.
- **DHT11 and DS18B20 are the exception (D63, 2026-08-14): dedicated
  bindings, not generic-primitive glue.** Both are timing-critical
  single-wire protocols (tens-of-microseconds pulse-width bit encoding)
  that a MicroPython bytecode loop can't reliably time — and this
  project's embedding sharpens that risk specifically: the interpreter's
  GC can pause at an arbitrary bytecode boundary, corrupting a
  bit-banged read mid-sequence in a way indistinguishable from a flaky
  board. `calc.dht11_read(pin)` and `calc.ds18b20_read(pin)` do the
  whole timed exchange inside one synchronous C++ call — the same
  reason MicroPython's own upstream ports implement DHT/1-Wire via a
  C-level `machine.bitstream()` helper rather than pure Python. DS18B20
  v1 is single-device-per-bus (skip-ROM); multi-device ROM search is a
  stretch.
- **Command/config sensors, not just passive-read ones (2026-08-13)**:
  some sensors need an active conversation, not a bare read —
  write a command or config register, poll a status/ready bit, then
  read the result, possibly re-issuing config mid-stream. The `_reg`
  pair above already covers register-addressed versions of this (a
  "write a command byte to register X" is just `calc.i2c_write`, no new
  primitive needed) — but not every command/response sensor is
  register-addressed at all; some expect a raw command frame with no
  separate register field. Add the raw-transfer pair alongside the
  register-addressed one, matching MicroPython's own `machine.I2C`
  naming (`writeto`/`readfrom` vs. `writeto_mem`/`readfrom_mem`) rather
  than inventing a bespoke shape: `calc.i2c_writeto(addr, data)`,
  `calc.i2c_readfrom(addr, len)`. A command-loop script then looks like
  `calc.i2c_writeto(addr, [CMD_START]); while not
  (calc.i2c_readfrom(addr, 1)[0] & READY_BIT): pass; val =
  calc.i2c_readfrom(addr, 2)` — ordinary Python control flow, no new
  execution model.
- **I2C errors need to be catchable, not silent**: a poll-for-ready loop
  depends on distinguishing "not ready yet" from "the sensor NACKed" —
  `calc.i2c_read*`/`calc.i2c_write*` should raise a Python exception on
  bus error (NACK, timeout) rather than returning a sentinel value a
  script could mistake for real data, so `try`/`except` around a retry
  is the natural pattern, not silent misreads.
- **Guardrail**: reject reserved/out-of-range pin numbers with a clear
  Python exception rather than letting a bad pin number reach the SDK —
  matches this project's general "clean error over undefined behavior"
  stance elsewhere (e.g. `kMaxStack`/depth caps). The reserved list is
  now concrete: hard-block GP2/3/4/5/20/21 (PSRAM) and GP6/7/10-19/22/26/27
  (LCD/SD/keyboard/audio) outright; GP28 is unconditionally allowed;
  whether GP0/1/8/9 (UART0/1) are allowed or also blocked is a design
  call for whoever implements this, given the reflash/debug-path caveat
  above — not decided here.
- **Confirms a gap already found**: `calc.list_append` (added to §4.2's
  code block this session) — a logging loop needs O(1) Python-heap cost
  per sample, not "hold a growing Python list, then one `set_list` call
  at the end," which is the wrong shape for an open-ended logging run.
- **Deliberately not assumed**: per-sensor drivers *beyond DHT11/DS18B20*
  (D63's exception is narrow and reasoned, not a general policy) and
  timer/interrupt-driven background sampling —
  a plain Python `while` loop is the obvious v1 shape for **either**
  case, passive-read (`calc.i2c_read*`/`gpio_read` + `list_append` +
  `time.sleep`) or active command/config (interleaving
  `calc.i2c_writeto`/`i2c_write` calls into the same loop, per above) —
  both are ordinary Python control flow over the same primitive set, no
  separate execution model for the "active" case. Revisit
  timer/interrupt-driven sampling only if polling-loop jitter turns out
  to matter for a real sensor.
- **Bus contention scales with how "active" the loop is**: a
  command/config sensor issues more I2C transactions per sample than a
  bare read (write-command, poll-status one or more times, read-result
  — several transactions per logged value instead of one), which
  multiplies pressure on the keyboard-bus-sharing mitigation above
  proportionally. Worth stating plainly rather than leaving implicit:
  that mitigation has to hold up under dense command traffic, not just
  occasional polling reads.
- **UART sensors are a natural sibling, not scoped in**: the pin-map
  research (§4.6 entry 2's table) already surfaced GP0/1 and GP8/9 as
  unclaimed-by-firmware UART pins — a command/response protocol over
  UART is the same shape as an I2C one and the pins physically exist,
  but the user's original ask was GPIO/I2C specifically, so this stays
  a flagged possibility, not an added requirement.

**3. TVM (time-value-of-money) solver** — the TI-83/84+'s Finance app:
`N`/`I%`/`PV`/`PMT`/`FV`/`P/Y`/`C/Y` fields, cursor to any one field,
solve for it given the other four.

Unlike entries 1 and 2, **this one needed nothing new** — worth
recording precisely because it's the first data point the other
direction. Everything it needs was already spec'd or already exists in
the expression language:

- **The numeric solve itself**: not `calc.solve` (§4.2's CAS/symbolic
  binding) — the TVM equation is transcendental in `I%` for `N > 1`, not
  generally symbolically solvable. The right tool is already in the
  expression language and needs no new `calc` binding at all: `solve(f,
  x, lo, hi)` (`src/math/catalog.cpp:101`, backed by
  `math::numeric_solve`, D28) is a bracketed numeric root-finder over an
  **arbitrary expression string** and an arbitrary single-letter
  variable — it does not require a named graph Y-slot the way
  `calc.graph_zero` does, so nothing here risks clobbering the user's
  own Y1–Y7 graphs. Reachable today through the already-spec'd
  `calc.eval()`: `calc.eval("solve(pv+pmt*n+fv, n, 1e-6, 1)")` finds
  `I%` directly. This is worth calling out in §4.2 itself so a future
  app author reaches for `solve(...)` inside `calc.eval` rather than
  `calc.solve`, whose CAS path is the wrong tool for this shape of
  problem.
- **Form UI**: a field grid with arrow-key navigation and in-place edit
  is buildable from already-spec'd primitives (`calc.wait_key`,
  `calc.draw_text`, `calc.input` for the simplest per-field edit shape)
  — no new display/input primitive needed, just more Python.
- **Persisting last-used values**: `calc.read_file`/`calc.write_file`
  (already spec'd) are sufficient — a flat `key=value` file, no `json`
  dependency, unlike entry 1.
- **Packaging**: another SD-discovered app (§4.5),
  `/picocalc/apps/tvm/{app.txt, main.py}` — a second concrete example of
  that pattern, reinforcing rather than changing it.

**This corrects §9.2**, which named "a finance/TVM solver" as a case
that "might still warrant a compiled app via §3.4 ... for speed/polish
reasons." Having walked it through: there's no performance argument
here — a form redraw and one small bracketed root-find are not remotely
hot paths — so nothing about this example supports compiled-app
treatment. §9.2 updated accordingly.

**4. Sound demo** — play a tone/melody through the speakers.

Confirmed by reading `src/`: `drivers/pwm_sound` (vendored, D-prelude-1;
GP26/27 PWM, mono — both channels driven identically, square-wave
tone only, no sample/WAV playback, no stereo) is **completely unused
today** — no `sound_init`/`sound_play`/`sound_set_enabled` call anywhere
in `src/platform/` or `src/apps/`. Unlike every other §4.6 entry, this
one needs a whole new HAL layer from nothing, not a binding or a tweak
to something that already exists.

- **The driver already does more than its header admits**: the
  interrupt handler (`pwm_sound.c`) computes an arbitrary
  `(frequency, duration)` square wave internally — but the public API
  (`pwm_sound.h`) only exposes 3 fixed effects (`SND_BEEP`,
  `SND_TAB_SWITCH`, `SND_ERROR`), no way to play an arbitrary tone
  through it as shipped.
- **Vendored-code tension (P6-13, resolved 2026-08-14 — D62: yes,
  editing is acceptable)**: exposing an
  arbitrary tone needs one new public entry point in `pwm_sound.h`/`.c`
  itself, not just a `platform::` wrapper — the tone state
  (`sound_frequency`/`sound_duration`/slice handles) is `static`
  (file-private) to `pwm_sound.c`, unreachable from outside without a
  header change. D-prelude-1 treats `drivers/` as read-only third-party
  code, wrapped rather than modified; D7 hit a similar wall once (a
  vendored driver's public API was inadequate) and resolved it by
  reimplementing the missing piece in the wrapper instead of touching
  the vendored driver — not viable here, since the ISR and its state are
  private to the `.c` file, so there's nothing to reimplement against
  from outside it.
- **`calc` surface**: `calc.tone(freq_hz,
  duration_ms)` — fire-and-forget, since the ISR already runs async. A
  melody is just a Python loop of `calc.tone(...)` +
  `time.sleep(duration_ms/1000)` calls; no new "wait for tone to finish"
  primitive needed for a v1 demo.
- **Mute already covered by the vendored driver**:
  `sound_set_enabled`/`sound_is_enabled` already exist — `platform::Sound`
  should just forward to them. Volume itself needs no `calc` binding at
  all: per the assembly manual (§4.6 entry 2), there's a physical volume
  knob, analog/hardware-controlled, not software.
- **New source files this would actually add** (not yet in §2's list,
  unlike everything else this session): `src/platform/sound.hpp/.cpp`.

**Scope decided 2026-08-13**: the demo app stays at the minimal tone
extension above — monophonic square-wave melodies via `calc.tone`, not
sample playback. Researching the vendored driver's real ceiling turned
up two more capable references (both on this same GP26/27 hardware):
GP26/GP27 share one PWM slice (confirmed independently on the
ClockworkPi forum, "Audio with PIO PWM" — a real RP2040/2350 limit, not
vendored-driver laziness; RP2350 also gets 16-bit PWM resolution vs
RP2040's 10-bit, a real board-quality gap either way), and a separate,
MIT-licensed project (LofiFren's PicoCalc firmware) ships `picosampler`
— a **DMA-paced PWM sampler mixing up to 8 voices of 8-bit PCM streamed
from SD**, fixed-point throughout (the audio interrupt never touches
the FPU), proving real sampled/multi-voice music is achievable on this
hardware class. That's real driver-writing effort (tens of hours), not
"a demo app," and that project targets RP2350/Pico 2 specifically (no
stated Pico 1 support) — a real tension against this project's
dual-board requirement. **Spun off as its own candidate, §9.4
("PCM sampler audio engine," candidate sub-phase **6.2**)** rather than
folded into this demo — see there for the full writeup. P6-13 (above)
stays scoped to the minimal tone extension only; the sampler idea
carries no vendored-code tension at all, since it would be original
code in `platform::Sound`, not an edit to `pwm_sound.c`.

**5. Real-time game** (e.g. a Snake/Pong-shaped SD-discovered app,
`/picocalc/apps/snake/{app.txt, main.py}`) — added 2026-08-14 to
pressure-test a part of §4.2 no prior entry touched: continuous
per-frame input during an active redraw loop, as opposed to the
menu/form-driven blocking-input shape entries 1-3 all share.

- **Found a real gap, not just confirmed coverage**: every input
  primitive §4.2 specced before this session blocks — `calc.wait_key()`
  and `calc.input()` — which is exactly wrong for a game loop that must
  keep redrawing and moving whether or not a key is currently down. The
  underlying HAL already has what's needed
  (`platform::Keyboard::poll()` is non-blocking by design — D7 — and
  `is_held()` gives continuous key state) but **nothing in §4.2 exposed
  either to Python**. Added this session: `calc.key_pressed()` (wraps
  one `poll()` call, returns immediately) and `calc.key_held(name)`
  (wraps `is_held()`) — see §4.2's updated Input section. Both are
  direct wrappers over existing HAL surface, no new C++ capability
  needed, so this doesn't change 6B.9's estimate.
- **Draw-loop performance is a real open question, not assumed fine**:
  a full-screen-redraw-every-frame game is a different access pattern
  from entries 1-3's one-time render, and `calc.draw_*` performance
  under a tight Python loop hasn't been measured. Not blocking — v1
  games can redraw only changed cells (the existing screens' strip/dirty
  conventions already do this) — but worth a `gc.mem_free()`-style
  on-device timing check the first time an app like this actually gets
  built, the same way §4.4 flagged for the periodic table's Python-heap
  cost.
- **Everything else already sufficient**: `calc.draw_rect`/`draw_line`
  for the board, `calc.clear_screen`, `time.sleep` for frame pacing
  (already assumed available by entry 2's sensor-logging walkthrough).

**6. Graph-analysis combo app** (e.g. a root/extrema-finder wrapper
that calls `calc.plot`, `calc.window`, `calc.show_graph`, then
`calc.graph_zero`/`graph_max` on what it just plotted) — added
2026-08-14, the first candidate to exercise §4.2's graphing surface at
all (entry 3 deliberately avoided it, preferring `solve()` inside
`calc.eval()` specifically *because* it needs no Y-slot).

- **Confirmed by reading `src/graph/graph_state.hpp`**: Y1-Y7 are a
  real, fixed-size, persisted array (`kFunctionSlots = 7`,
  `graph::GraphState`) — the same slots the Y= editor and Graph screen
  use, not a sandboxed Python-only copy. This is consistent with
  `calc.store`/`calc.recall` already operating on real calculator
  variables (§4.2), so it's not a new design principle — but it does
  mean a script that plots leaves Y1-Y7 changed after it exits, the
  same way a script that stores leaves a variable changed, and this is
  worth stating plainly in any app-author-facing docs rather than
  leaving it to be discovered.
- **New requirement surfaced, recorded as P6-15 (§8)**: §4.2's
  `calc.plot()` examples show two calls in a row
  (`calc.plot("sin(x)", ...)`, `calc.plot("cos(x)", ...)`) with no slot
  argument and no stated clearing behavior. With only 7 real slots
  that persist and that the user's own graphs also live in, "which
  slot, and does it clear first" is user-facing behavior that needs
  deciding before 6B.6, not an implementation detail — see P6-15 for
  the options.
- **Everything else already sufficient**: `calc.window`/`show_graph` as
  specced, `calc.graph_zero`/`graph_max` need no changes once P6-15
  settles which slot they're pointed at.

### 4.7 Re-verified against the unified evaluator (2026-08-13, closes issue #27)

§4.2 was written against the evaluator Phase 5.2 replaced
(`math::matexpr`/`complexexpr`/`listexpr`). Issue #27 asked four
things before 6B could be scoped for real; answered here by reading
`src/math/unified_eval.hpp`, `src/math/unified_home.hpp`, and
`HomeScreen::evaluate_input`'s actual call sequence
(`src/apps/home_screen.cpp:398-499`) — not by re-reading the spec's own
assumptions.

**1. Which entry points still exist, under what names.** More layers
than the issue's phrasing suggested, and the answer matters for which
one `calc.eval()` should actually call:

- `math::unified::compile(src, Program&, err)` / `run(Program&, Value*, err, Mode, Commit*)` —
  the lowest level (task 5.2.2-5.2.4). Not what a binding should call
  directly: no formatting, no CAS, no `solve()` substitution.
- `math::unified::evaluate_scalar(expr, Complex* out, err)` — a
  lighter probe-only wrapper (scalar-only, no store, no CAS/`solve()`
  pipeline). Built for the list/matrix cell editors, not shaped for a
  general `calc.eval()`.
- `math::unified::evaluate_home(expr, to_frac)` → `HomeResult` (kind,
  formatted `text`, `store_label`, `Commit`) — the layer
  `HomeScreen::evaluate_input` actually calls (`home_screen.cpp:499`),
  **after** two earlier steps it does NOT include itself: `expr` has
  already been through `math::cas::evaluate_home(expr, allow_complex)`
  (a **separate, same-named function in a different namespace** —
  tried first, `home_screen.cpp:404`) and, if that declines,
  `math::solveexpr::contains_solve(expr)` /
  `substitute(expr, size, err)` (`home_screen.cpp:445-447`) for inline
  `solve(f,x,lo,hi)` rewriting — the exact function §4.6 entry 3's TVM
  walkthrough leaned on. Both pipeline steps are already standalone
  `math::` functions, not trapped in the UI layer, which is the good
  news: **`calc.eval()` can replicate `HomeScreen::evaluate_input`'s
  real pipeline (`cas::evaluate_home` → `solveexpr::contains_solve`/
  `substitute` → `unified::evaluate_home`) by calling the same three
  functions in the same order** — a wiring task once 6B is
  implemented, not a missing capability. (The `>frac`/`>dec` suffix
  strip is ~10 lines inline in the screen, `home_screen.cpp:468-484` —
  trivial to reproduce or skip for v1.)
- Everything else in §4.2 (`calc.matrix`/`det`/`inverse`,
  `calc.set_list`/`stat_mean`, `calc.complex`/`c_abs`, `calc.store`/
  `recall`) calls `math::Matrix`/list-store/`Variables` APIs directly,
  **never through the unified evaluator at all** — confirmed by
  §4.2's own closing line ("thin C++ wrapper... into the existing
  `math::Engine`, `math::Matrix`..."). None of that surface is affected
  by 5.2's evaluator swap; the re-verification narrows to `calc.eval()`
  alone.

**2. The lifetime contract** (`unified_eval.hpp:340-344`): a `kList`
`Value` from `run()` is valid only until the next `run()`; a `kMatrix`
`Value` in `Mode::kCommit` is safe (copied into `MatAns` immediately).
This is a non-issue for the `evaluate_home()` path specifically, since
it already returns **formatted text**, not a live `Value` reference —
but §4.2's own example (`result = calc.eval("2 + 3 * sin(pi/4)")`)
implies Python wants a *number* back, not a string, and a `calc.set_list`-adjacent
call returning a Python list from a `kList` result would be reaching
past the formatted-text layer into the underlying data. **Binding
requirement, not just a risk to note**: any `calc.eval()` implementation
that hands back list/matrix contents must copy them into native Python
objects (a real Python list of floats, etc.) synchronously, inside the
same call that produced them — never store the `Array*`/`Value`
reference and read it later. Trivial to satisfy by construction if
stated as a rule up front; a real bug class if left implicit.

**3. Reentrancy** (`unified_eval.hpp:349`'s comment, "non-reentrant
singletons" per the issue): `compile()`/`run()` share bss-resident state
(one `Program` buffer, one 64-slot operand stack — `unified_eval.hpp:233-254`).
A second `run()` invoked while the first is still in progress corrupts
that shared state — not a clean error, memory corruption. §4.2's
`calc.eval()` as speced is synchronous and doesn't let Python code run
*during* an evaluation, so there's no natural re-entry path from the
expression language itself. The narrow, real risk is MicroPython's own
GC: an allocation inside the binding could trigger collection, and a
Python `__del__` finalizer can run arbitrary code — including, in
principle, another `calc.eval()` call — mid-evaluation. **Recommend an
explicit reentrancy guard in the C++ binding** (a simple in-call flag,
returning a clean Python exception if re-entered) rather than trusting
"this shouldn't happen" — matches this project's existing "clean error
over undefined behavior" stance (`kMaxStack`, pin validation, §4.6
entry 2's I2C error handling).

**4. Which of the four retired parsers the spec's examples assumed.**
Phase 5.2 retired three (`matexpr`, `complexexpr`, `listexpr`) and
absorbed a fourth's home-screen use: `tinyexpr` stops handling
home-screen scalar spans (`unified-evaluator-changes.md` G5, "the
escape hatch is gone") but **is deliberately not retired** for the
graphing/table/stats path (`unified_eval.hpp:16-18`, P7) — a measured
~1.75x performance guardrail (D52/D50), unaffected by any of this.
§4.2's examples are all ordinary expressions with no boundary
conditions, so none obviously land on a recorded divergence in
`unified-evaluator-changes.md`'s W/N/F register — the closest is
sign-handling around `(-2)^2`-shaped inputs (F1), already agreed
between paths as of v0.3.2. Nothing in §4.2 needs rewriting on this
count; 6B's own host tests should check real binding code against the
register once written, the same way every other evaluator consumer
does.

**Net effect on scoping**: 6B is unblocked. The re-verification found
no dead entry points and no example that needs rewriting — it found a
concrete implementation shape (`calc.eval()` wraps `cas::evaluate_home`
→ `solveexpr` → `unified::evaluate_home`, copies out list/matrix
results eagerly, guards against re-entry) that 6B.3 should build
against directly rather than re-deriving.

---

## 5. Task breakdown

Solo developer, part-time (~20 hrs/week).

### Sub-phase 6A: App framework

**Build-order note (2026-08-14, D64), spans 6A/6C/6B — not the same as
the table order below.** The table lists tasks by sub-phase for
estimating/accounting; the recommended *build* sequence interleaves
6A and 6C ahead of 6B:

```
6A.1 → 6A.2 → 6A.3 → 6A.4        (framework: registry, launcher,
                                   handoff, entry points)
     → 6A.6                       (FileBrowserScreen navigate+pick —
                                   moved ahead of the widget so F3:LOAD
                                   is wired for real, not stubbed)
     → 6A.5                       (shared TextEditorWidget)
     → 6C.1                       (Notepad — first real app on the
                                   launcher; proves the widget's full
                                   edit/save/load loop end-to-end,
                                   including on hardware, before 6B
                                   commits to a second consumer of it)
     → 6A.7                       (file management — doesn't block
                                   Notepad's core loop, can trail)
     → 6B.1 ... 6B.16             (MicroPython; 6B.11's editor is now
                                   an explicit wrapper over the widget
                                   Notepad already proved)
```

This supersedes the previous plain 6A.1→6A.7 reading (which had 6A.5
built before 6A.6 and stubbing `F3:LOAD`) — building 6A.6 first removes
the need for a stub. Nothing about *which* task belongs to which
sub-phase changed, only the order they're tackled in.

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 6A.1 | `AppRegistry` + static registration pattern | 3 | Apps self-register at boot, `count()`/`get()` work |
| 6A.2 | Launcher screen (list, select, launch) | 5 | Launch a stub app from the launcher |
| 6A.3 | Screen-ownership handoff + exit-to-launcher convention | 4 | `ESC` from an app returns to launcher, not Home |
| 6A.4 | Home-screen entry points (D58): dedicated softkey **and** `apps`/`app` typed command, both ship | 2 | Either the softkey or the `apps`/`app` command opens the launcher |
| 6A.5 | Shared `TextEditorWidget` (§3.5): buffer, cursor, arrow nav, insert/backspace/auto-indent, config-driven softkey row, load/save-to-path | 7 | Widget instantiated standalone edits/saves a scratch file |
| 6A.6 | `FileBrowserScreen` (§3.7): generalize `FilesScreen` in place — directory navigation (`ENTER` descends, parent key), `kBrowse`/`kPick` modes, `ext_filter`, `on_picked` callback | 5 | Diagnostic `Files` entry navigates subdirs; a `kPick` call scoped to `/picocalc/programs` returns a chosen `.py` path |
| 6A.7 | File management (§3.7, D55): `Storage::rename_file`/`delete_dir` primitives, delete confirm dialog, rename via existing text-entry, new-folder key | 5 | Delete (confirmed), rename, and new-folder all work from both `kBrowse` and `kPick` |
| | **Subtotal** | **~31 hrs** | |

### Sub-phase 6B: MicroPython programming (first base app)

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 6B.1 | Build MicroPython embed lib (both boards), incl. deciding which stdlib modules are compiled in (`json` needed per §4.6's periodic-table walkthrough — not just a bare interpreter) | 8 | `print(1+1)` → "2" on serial; `import json; json.loads("{}")` works |
| 6B.2 | `PythonInterpreter` wrapper | 4 | Init/exec/shutdown clean |
| 6B.3 | `calc` module: eval, variables, store/recall — `calc.eval()` wraps `cas::evaluate_home` → `solveexpr::contains_solve`/`substitute` → `unified::evaluate_home` (§4.7), eager-copies list/matrix results, reentrancy-guarded | 6 | `calc.eval("sin(pi/4)")` correct |
| 6B.4 | `calc` module: CAS bindings (incl. complex solve) | 4 | `calc.solve("x^2+1=0","x")` → `["i","-i"]` |
| 6B.5 | `calc` module: complex bindings | 3 | `calc.c_abs(calc.complex(3,4))` = 5 |
| 6B.6 | `calc` module: graph-analysis + `plot`/`window`/`show_graph` bindings — P6-15 (§8, §4.6 entry 6) must be settled first, since `plot`'s Y-slot semantics are unspecified | 4 | `calc.graph_zero`, `graph_integral` work |
| 6B.7 | `calc` module: matrix bindings | 3 | Create/multiply/invert from Python |
| 6B.8 | `calc` module: display primitives | 4 | Script draws graphics |
| 6B.9 | `calc` module: keyboard input — blocking (`wait_key`, `input`) and non-blocking (`key_pressed`, `key_held`, §4.6 entry 5) | 3 | Read keys, text input, poll without blocking |
| 6B.10 | `calc` module: file I/O | 2 | Read/write SD files |
| 6B.11 | Python program editor: thin `TextEditorWidget` wrapper (§4.3) — RUN wiring, `.py` ext/dir, auto-indent-after-`:` config, syntax highlighting stretch, registered as a 6A app | 3 | Write a 20-line script on-device |
| 6B.12 | Execution: output capture, error display | 4 | print output + line-numbered errors |
| 6B.13 | Load/save scripts to SD | 3 | Save, power cycle, reload, run |
| 6B.14 | Memory management: lazy init, cleanup | 3 | Heap freed on leaving program screen |
| 6B.15 | SD app manifest parser + second `AppRegistry` tier (§4.5) | 6 | Malformed manifest skipped + logged, not fatal |
| 6B.16 | Boot-time SD app scan + launcher integration (§4.5) | 6 | `/picocalc/apps/finance/` shows as a named launcher tile |
| | **Subtotal** | **~66 hrs** | |

### Sub-phase 6C: Notepad (first concrete future app)

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 6C.1 | Notepad app: thin `TextEditorWidget` wrapper (§3.6) — `.txt` ext, `/picocalc/notes/` dir, no RUN key, registered as a 6A app | 3 | Write, save, power-cycle, reload a text note on-device |
| | **Subtotal** | **~3 hrs** | |

### Summary

| Sub-phase | Hours | Deliverable |
|-----------|-------|-------------|
| 6A: App framework | ~31 | Launcher screen, app registry, screen-handoff convention, shared text-editing widget, generalized file browser (navigate/pick/manage) |
| 6B: MicroPython | ~66 | Interpreter, `calc` module, editor (thin wrapper on 6A's widget), SD scripts, SD-discovered app manifests — first app on 6A |
| 6C: Notepad | ~3 | Second thin wrapper on 6A's widget — plain-text notes, no MicroPython dependency (D54) |
| **Total (committed)** | **~100 hrs** | |
| *Compiled app launcher entries (§3.4, stretch)* | *~25–35 (likely understated, D66), gated on a feasibility spike* | *Self-sufficient reboot into a full app `.uf2` via this project's own bootstrap (D66 — not `uf2loader`-dependent), selected the same way as a Python app* |
| *Native dynamically-loaded (in-process) apps* | *deferred* | *own future phase, not Phase 6 — see §9.1* |
| *6C+ other future apps, release engineering* | *unscoped* | *see §9.2 — no estimate until something is actually picked up* |

---

## 6. Performance expectations

### App framework (6A)

Negligible — a static registration table and a list-menu screen, the
same cost class as any other menu screen already in the app. No new
performance-sensitive path.

### MicroPython (6B)

Unchanged from the original 4E estimate: interpreter init/exec cost is
dominated by MicroPython's own bytecode VM, not anything this firmware
adds. The one guardrail worth repeating — the Python heap is only
allocated when the program screen is entered (§4.4), so MicroPython has
zero steady-state cost during normal calculator use, graphing included.

### Notepad (6C)

Negligible, same class as 6A — a thin wrapper around an already-built
widget, no interpreter, no heap of its own beyond the text buffer.

---

## 7. Risks and mitigations

*(Risks 6 and 7 below were originally numbered this way in phase4-spec.md
before MicroPython moved here — numbering kept for continuity with any
existing cross-references.)*

### Risk 6: MicroPython heap too small on Pico 1

**Resolved 2026-08-14 (D61): pre-committed to 40 KB**, down from the
original 48 KB, ahead of 6A landing — a fresh pre-6A `size-report.sh`
measurement found only 2.2 KB of margin above the threshold this risk's
lever was meant to guard, with no 6A code written yet (§0.1). The lever
that used to be conditional ("drop to 40 KB if headroom is tight") is
now just what 6B is scoped against. **Mitigation, otherwise unchanged**:
document the limit; store large data in `calc`-module lists/matrices
(PSRAM, outside the Python heap); Pico 2 doubles the heap (unaffected —
its headroom was never close).

### Risk 7: Building MicroPython for both RP2040 and RP2350

**Mitigation**: MicroPython officially supports both. Build the embed lib
as a separate CMake external project per board target. Test the embed
build early (6B.1) before writing bindings.

### Risk 10: App framework becomes over-engineered for one app

With only MicroPython as a concrete consumer at launch, 6A risks being
speculatively generalized for apps that don't exist yet. **Mitigation**:
keep 6A to exactly what §3 specs — a static table, a list menu, and an
exit convention. No dynamic loading, no per-app permission model, no
icon grid. Expand only when a second app (§9) actually needs more.

### Risk 11: `calc` module CAS bindings ship before Phase 5 is stable

If Phase 6 starts before Phase 5 (CAS) has fully landed, `calc.diff`/
`calc.integ`/`calc.solve` (§4.2) have nothing to bind to yet.
**Mitigation**: per the intended phase order (4 → 5 → 6) this shouldn't
happen, but if 6B ever gets picked up out of order, ship the non-CAS
`calc` bindings (eval, graphing, matrices, complex, display, I/O) first
and add the CAS bindings as a follow-up task once Phase 5 exists — the
module is additive, nothing else in 6B depends on the CAS bindings being
present.

---

## 8. Open questions

*(P6-1/P6-2 below carry the same content as phase4-spec.md's former
P4-4/P4-5, renumbered into this document.)*

| # | Question | Options | When |
|---|----------|---------|------|
| P6-1 | Python heap: static at boot or lazy on first use? | **Resolved 2026-08-13 (D57): lazy** — allocated on entering the program/app screen, freed on leaving (matches §4.4/§6's prose, now formalized) | 6B implementation |
| P6-2 | `calc.plot()` from Python: immediate graph switch or buffered? | Immediate vs. buffered | 6B implementation |
| P6-3 | Launcher entry point: dedicated Home softkey, or typed-command-only like `lists`/`stats`? | **Resolved 2026-08-13 (D58): both** — a softkey and the `apps`/`app` command ship together | 6A implementation |
| P6-4 | Does leaving an app via `HOME` (not `ESC`) skip the launcher entirely, or route through it? | **Resolved 2026-08-13 (D58): skips it** — `HOME` keeps its existing system-wide short-circuit-to-Home behavior unchanged; only `ESC` routes through the launcher | 6A implementation |
| P6-5 | §3.4 compiled apps: depend on `uf2loader` being installed, or make the calculator self-sufficient for the flash-write/reboot step? | **Resolved 2026-08-14 (D66): self-sufficient.** `uf2loader` demoted to a purely optional, user-installed, manually-invoked recovery tool — never depended upon by the automatic boot path. Surfaced two corrections in the process: bare `watchdog_caused_reboot()` is ambiguous (needs a dedicated scratch-register marker, matching this codebase's existing `g_crash.magic`/`kBulkTestMarker` pattern); the bootstrap must be a genuinely separate, permanent component, not logic inside the calculator's own `main()` — §3.4's ~25-35 hr estimate predates this and is likely understated | Resolved; bootstrap design itself still owed at §3.4 implementation |
| P6-6 | §3.4 "return to calculator": bundle the calculator's own `.uf2` as a resource apps carry, or fetch it fresh from a known SD path at return time? | **Resolved 2026-08-13 (D59): fetched fresh**, from `/picocalc/firmware.uf2`. Kept in sync by the firmware self-snapshotting its own running image there (safe: on-device flash is memory-mapped/XIP, so this is an ordinary read, not the write-into-flash risk the restore step has) — **checked and, only on mismatch, written lazily on every launcher entry** (§3.2's `on_activate`), not at boot. Needs two prerequisites this codebase doesn't have yet: an exposed build-size symbol, and a version/build identifier to gate the write (`pico_set_program_version` or similar — zero hits in `CMakeLists.txt` today) | 3.4 implementation |
| P6-7 | §3.7 file browser: how much is in scope for Phase 6 — navigate+pick only, or also delete/rename/new-folder management? | **Resolved 2026-08-13 (D55): management is in scope** — delete (confirm-gated), rename, new folder, plus `Storage::rename_file`/`delete_dir` (non-recursive) | 6A.7 implementation |
| P6-8 | §3.7: does the existing diagnostic `Files` entry point (`FilesScreen`, `/picocalc`-only, read-only) change to support navigation, or does a separate picker-mode component get added alongside it unchanged? | **Resolved 2026-08-13 (D55): generalized in place** — one `FileBrowserScreen` with `kBrowse`/`kPick` modes; the diagnostic keeps its current `start_dir`/behavior as `kBrowse`'s default and gains navigation/management for free | 6A.6/6A.7 implementation |
| P6-9 | §9.3 home-screen scripts: does a line that errors abort the rest of the script, or continue and report at the end? | **Resolved 2026-08-13 (D56): abort** on the first error | 9.3 implementation, if promoted |
| P6-10 | §9.3: where do script files live on SD? | **Resolved 2026-08-13 (D56): `/picocalc/scripts/`** | 9.3 implementation, if promoted |
| P6-11 | §4.6 entry 2 (sensor logging): this project's hardware pin map is undocumented — which pins do LCD SPI/keyboard I2C/SD SPI/PSRAM/audio PWM already commit, and is a second I2C peripheral available on any exposed expansion header? | **Resolved 2026-08-13**: cross-checked from two primary sources, `clockwork_Mainboard_V2.0_Schematic.pdf` and the official `Clockwork_PicoCalc_Assembly_Guidelines.pdf` ("The Interfaces" page) — full pin table and three-tier free/risky/committed breakdown in §4.6 entry 2. Confirmed: a real case-exterior "Core GPIOs"/"Mainboard GPIOs" header exists (left side); only GP28 is unconditionally free; no second I2C bus exists anywhere on the chip | §4.6 entry 2's remaining open items (P6-12) resolved 2026-08-14, D63 |
| P6-12 | §4.6 entry 2: which specific sensors/protocols to support | **Resolved 2026-08-14 (D63)**, against the user's actual sensor box: **DHT11**, **DS18B20**, assorted **LM393**-comparator boards. LM393 needs only the existing generic primitives plus a new `calc.adc_read(pin)`; DHT11/DS18B20 get dedicated `calc.dht11_read`/`calc.ds18b20_read` C++ bindings — single-wire timing plus this project's GC-pause exposure make pure-Python glue unreliable for those two specifically | §4.6 entry 2 implementation, if/when picked up |
| P6-13 | §4.6 entry 4 (sound demo): playing an arbitrary tone needs one new public entry point in the vendored `pwm_sound.h`/`.c` (the ISR already computes arbitrary frequency/duration internally; only the enum-limited public API is missing it) — is editing vendored driver code acceptable here, given D-prelude-1 treats `drivers/` as read-only third-party, wrapped rather than modified? | **Resolved 2026-08-14 (D62): yes** — `drivers/README.md`'s own policy already documents this exact exception (minimal patch, recorded under Local modifications), matching the D51/tinyexpr precedent. D7's usual answer (reimplement in the wrapper) doesn't apply — the tone state is `static`/file-private to `pwm_sound.c` | §4.6 entry 4 implementation |
| P6-14 | §3.4: does a real power-cycle actually deassert `PICO_EN` (POR on the Pico), confirming `watchdog_caused_reboot()` reliably distinguishes "user power-cycled" from "calculator deliberately handed off to an app"? | **Resolved 2026-08-14 (D65), hardware-confirmed on the Pico 1**: yes — `watchdog_caused_reboot()` read `false` after a genuine physical power-cycle and `true` after a non-power reboot. New permanent diagnostic in `main.cpp` (`boot: watchdog_caused_reboot=%d`, 30 s heartbeat) | Resolved |
| P6-15 | §4.6 entry 6: `calc.plot()` writes into one of the real, persisted Y1-Y7 slots (`graph::GraphState`, `kFunctionSlots = 7`) — same shared-state model as `calc.store`/`recall` touching real variables. Not yet specified: does the first `calc.plot()` call in a script clear existing Y-slots first, append to the next free one, or something else? What happens on a 9th call, or if all 7 are already in use by the user's own graphs? | Auto-clear-on-first-call (predictable, but silently drops the user's own Y1-Y7 the moment a script plots anything) vs. explicit slot argument (`calc.plot(expr, slot=1, ...)`, no silent clobber but pushes bookkeeping onto script authors) vs. append-with-error-on-full (matches `calc.store`'s "touches real state" philosophy most closely, but a script re-run without clearing accumulates stale functions) | 6B.6 implementation |

---

## 9. Candidate future sub-phases (unscoped)

Deliberately left open rather than pre-scoped — the point of Phase 6's
structure is that these can be picked up whenever, in whatever order,
without a spec rewrite each time. Listed here so they're not lost, not
because any of them is committed.

### 9.1 Native dynamically-loaded (in-process) apps — its own future phase

**History (2026-07-21, two corrections in the same session)**: this
section originally covered both SD-discovered Python apps and *all*
compiled-app approaches as one "SD-card app loading" item. The Python
half was low-risk and has since been accepted into 6B's scope (§4.5).
It then turned out the original write-up had missed
[feasibility.md](../notes/feasibility.md) §4.4, this project's own
pre-Phase-1 research, which had already named `uf2loader` as a simpler
alternative to in-process loading — corrected by adding what was then
"Path B" (reboot into a separate firmware image). That approach turned
out to be viable as **a launcher menu item indistinguishable in shape
from a Python app entry** (self-flash the selected `.uf2` into a
reserved region, then reboot — no interactive bootloader menu needed),
which is enough of a UX and complexity win that it has since been
promoted out of this section entirely — **see §3.4** for the accepted
stretch-goal version. What remains deferred to a genuinely separate
future phase, covered below, is the harder problem: **loading code that
runs concurrently with the calculator firmware**, in the same process,
without a reboot.

**Goal**: run genuinely new machine code inline — no reboot, app and
calculator coexist in one running process — without a firmware rebuild.
This is the "OS-style" version: copy a relocatable blob into RAM or a
reserved flash partition and jump into it while the calculator firmware
keeps running. It is hard on a microcontroller with no MMU, for six
compounding reasons:

1. **The SD card isn't executable memory** — a loaded app has to be
   staged into RAM or a reserved on-chip flash partition first (RAM
   competes with Pico 1's already-tight ~188.8 KB/264 KB bss; flash
   staging needs a homegrown write-verify-activate protocol, since a
   partial write from power loss mid-flash can corrupt that partition).
2. **Position-independent code, or a real relocator** — either every
   app is built PIC (a fiddlier Cortex-M toolchain path than this
   project's existing fixed-address build) or the loader parses
   relocation records and patches addresses at load time — a minimal
   dynamic linker written from scratch. The hardest, most bug-prone
   piece, and bugs here are memory corruption, not a clean error.
3. **Memory protection is asymmetric across boards.** RP2040 (Pico 1)
   has no MPU at all — a bad app can corrupt anything it can address.
   RP2350 (Pico 2) has a real ARMv8-M MPU that could sandbox loaded code
   with hardware-enforced fault isolation, which does meaningfully
   improve the risk picture **if scoped Pico-2-only** — turning "silent
   corruption" into a catchable fault, and removing the "no cheap
   dual-board answer" objection this section originally raised, since
   there's no attempt to protect Pico 1 at all. Building the MPU
   configuration and a fault handler that can unwind back to the
   launcher is itself new work, though, so this buys risk reduction more
   than effort reduction.
4. **A stable, versioned ABI/symbol table** the app links against — and
   unlike MicroPython (plain text, re-interpreted every run, immune to
   firmware internals changing), a compiled app blob silently breaks the
   moment a firmware rebuild changes this table's layout, with no
   compiler around to catch the mismatch.
5. **A build/packaging pipeline maintained indefinitely** — a second
   linker script, a binary header format (magic/ABI-version/CRC), and an
   SD-card layout convention every future app has to keep working
   against.
6. **A new failure mode** — a hung or corrupting app needs
   watchdog-driven recovery and a boot-loop guard, a class of problem
   nothing in the compiled-in-firmware model has today.

**Estimate**: **120–200 hrs** if scoped Pico-2-only (dropping the
dual-board tooling tax roughly offsets the added MPU/fault-handler work),
or **150–250+ hrs** if attempted for both boards.

**Recommendation**: not a Phase 6 item at all, stretch or otherwise —
this is the one approach left that's a genuine embedded-systems R&D
project in its own right (a homegrown relocator is still the hardest,
most bug-prone piece, and nothing above removes it). §3.4's reboot-based
approach delivers "run code that wasn't compiled into this firmware" for
a fraction of the cost and risk; there'd need to be a concrete need that
specifically requires in-process, no-reboot execution — not just "more
apps" — before this is worth opening. If that need ever materializes, it
deserves a standalone feasibility spike (prove the relocator in
isolation) before committing to the rest, and its own dedicated phase,
not a line item anywhere in Phase 6.

### 9.2 Other candidates

- **Additional built-in apps**, TI-Apps-style: a periodic table
  reference, a probability simulator, and others. On this platform the
  general answer to "TI ships this as an app" is usually "write it in
  MicroPython" (6B), or as an SD-discovered app (§4.5), rather than a
  new C++ app. **The TVM solver (§4.6 entry 3) has been walked through
  and needs no compiled treatment** — no new bindings, no performance
  case — so it's dropped from this "might warrant compiled" framing;
  evaluate any *other* candidate case by case, but don't assume one is
  needed by default.
- **Desktop emulator build target**: a third build target (alongside
  Pico 1/Pico 2) that runs the UI on a development machine. Flagged
  independently in the [wishlist](../notes/wishlist.md) (font
  antialiasing testing, D31) and the
  [docs-site-plan.md](../notes/docs-site-plan.md) (screenshot tooling)
  — both would benefit, but neither depends on 6A/6B, so this could
  start any time. It's dev/user tooling more than a user-facing "app,"
  so it sits loosely in this phase rather than squarely in it.
- **Release engineering**: standing up the [docs site
  plan](../notes/docs-site-plan.md) for real, and a versioned firmware
  release process (tagged builds, changelog). Doesn't depend on 6A/6B
  either — could run in parallel with any of the above, or even before
  Phase 6 starts in earnest.

### 9.3 Home-screen convenience scripts

**Raised 2026-08-13, not yet scoped.** Deliberately **not** full
programmability — no loops, no conditionals, no parameters. That's what
6B/MicroPython is for. The goal is narrower: let a user save and replay
a multi-step calculation they'd otherwise have to retype by hand —
several `Home` lines in sequence, exactly as if typed one at a time.

**Mechanism — reuses two things that already exist**: Phase 5.1's
`HomeScreen::submit_line()` (shipped, HW-verified) already does
trim → `handle_command()` → else `evaluate_input()` → `invalidate()`,
the literal sequence a typed line goes through. A new typed command
(e.g. `run <path>`) would read an SD text file and feed it to
`submit_line()` one line at a time — the same thing Phase 5.1's
serial-console host script already does over stdin, just fed from an SD
file instead of the serial link. Bare `run` (no path) could open §3.7's
`FileBrowserScreen` in `kPick` mode instead of requiring a typed path.
Blank lines and a comment prefix (e.g. `#`) are skipped.

**What a script line can be**: anything typeable at Home today —
expressions (`B=sin(A)+2`), and every existing typed command, including
the screen-switching ones already in `handle_command()` (`plot`,
`lists`, `matrix`, `cas`, …) and `mode`. No new mechanism needed for
either of those — they already run through `submit_line()` when
injected (Phase 5.1 proved that path).

**The one real gap**: window bounds (`graph::GraphWindow`) and Y=
function slots (`Y1..Y7`, `graph::kFunctionSlots`) currently have **no
typed-command equivalent** — only their dedicated screens
(`WindowScreen`, the Y= editor) can set them, the same way `mode` was
the only state-setting command before this. Scripting a "set up and
switch to a graph" step needs one or two more entries in that same
`handle_command()` dispatcher — e.g. a `window <xmin> <xmax> <ymin>
<ymax>` command and a `y1=<expr>`-style setter — mirroring exactly how
`mode` already writes into `graph::state()` and calls
`save_graph_state()`. Small, fixed additions to an existing dispatcher,
not a new subsystem.

**Resolved 2026-08-13 (D56, §8 P6-9/P6-10)**: a line that errors
**aborts the rest of the script** — multi-step calculations typically
have later steps depend on earlier ones, so continuing past a failure
would compound it. Script files live under **`/picocalc/scripts/`**,
parallel to `/picocalc/programs/` and `/picocalc/notes/`. Still
unscoped as a task (no hour estimate) — these two answers just remove
the open design questions ahead of that.

**Naming, if promoted**: this reuses Phase 5.1 machinery, wasn't part of
Phase 6's original 6A/6B/6C plan, and surfaced mid-brainstorm rather
than being scoped up front — the same shape that made 5.1/5.2 dotted
sub-phases rather than lettered ones (`picocalc-subphase-naming`:
letters are planned work, dots are what turned up outside a phase's
stated goals). If this moves forward, **6.1** fits that convention
better than a new `6D`.

### 9.4 PCM sampler audio engine (candidate: 6.2)

**Raised 2026-08-13**, spun off from §4.6 entry 4's sound demo once
research turned up a materially bigger opportunity than "add a tone
call" — see that entry for the full research trail (forum-confirmed
GP26/GP27 shared-PWM-slice limit, RP2350's 16-bit vs RP2040's 10-bit
PWM resolution, and LofiFren's MIT-licensed `picosampler`: a DMA-paced
PWM sampler mixing up to 8 voices of 8-bit PCM streamed from SD, fully
fixed-point). **Not the same naming case as §9.3** — this doesn't reuse
existing project machinery the way home-screen scripts reuses Phase
5.1's `submit_line()`; it surfaced mid-brainstorm as a "there's more
here than the demo needs" finding while researching a different item,
which is exactly `picocalc-subphase-naming`'s dots-vs-letters
distinction from the other direction: unplanned work discovered outside
Phase 6's stated goals. **6.2** (next available dotted slot after 6.1)
fits that convention.

**Deliberately unscoped — no task breakdown, no hour estimate.** What's
already known, to save re-deriving it later:

- **This is real driver work, not app work**: a DMA-paced interrupt
  mixer reading PCM from SD, fixed-point throughout (the reference
  implementation deliberately never touches the FPU in the audio ISR —
  matches this project's own stack/frame-budget discipline elsewhere,
  D47/D48). Tens of hours, its own design pass, not a `calc` binding
  added to an afternoon's work.
- **Original implementation, not a port**: `picosampler` is MIT-licensed
  (permissive enough to reference or adapt with attribution), but it's a
  different codebase built for a different firmware's architecture —
  this project's own dual-board HAL discipline (D-prelude-2), PSRAM
  conventions, and core 0/core 1 split would need original code, not a
  drop-in.
- **Dual-board tension, unresolved**: the reference implementation
  targets RP2350/Pico 2 specifically, with no stated Pico 1 support.
  Whether a PWM sampler is feasible on the Pico 1 at all (compute
  budget, DMA channels, 10-bit PWM resolution) — or whether this ships
  Pico-2-only, or degraded (fewer voices/lower rate) on Pico 1 — is a
  real open question, not assumed either way.
- **No vendored-code tension** (unlike §4.6 entry 4's minimal tone
  extension, P6-13): this would be original code in a new
  `platform::Sound`-adjacent component, not an edit to `pwm_sound.c`.

---

## 10. References

1. Phase 4 spec (prerequisite — GC completeness milestone) — [phase4-spec.md](phase4-spec.md)
2. Phase 5 spec (CAS — soft dependency for 6B's `calc` module) — [phase5-spec.md](phase5-spec.md)
2a. Phase 5.1 spec (serial line injection — `HomeScreen::submit_line()`, the mechanism §9.3's convenience scripts would reuse) — [phase5.1-spec.md](phase5.1-spec.md)
3. TI parity stocktake ("deliberately not chasing TI" — why 6A stays small) — [ti-parity.md](../notes/ti-parity.md)
4. Docs site plan (release-engineering candidate, §9) — [docs-site-plan.md](../notes/docs-site-plan.md)
5. MicroPython embed port — https://docs.micropython.org/en/latest/develop/embed.html
6. MicroPython RP2040/RP2350 support — https://micropython.org/download/RPI_PICO/
7. Project feasibility research, App Framework section (§3.4/§9.1's
   original source, missed on first pass) — [feasibility.md](../notes/feasibility.md) §4.4
8. `uf2loader` — SD-card bootloader for RP2040/RP2350 on the PicoCalc;
   informed §3.4's design (D34) but not depended upon (D66) — an
   optional, user-installed recovery-only tool, not part of the
   automatic boot path — https://github.com/pelrun/uf2loader (GPLv3)
9. Mainboard schematic (§4.6 entry 2's pin map, P6-11) —
   `clockwork_Mainboard_V2.0_Schematic.pdf`,
   https://github.com/clockworkpi/PicoCalc/blob/master/clockwork_Mainboard_V2.0_Schematic.pdf
10. Official assembly guidelines, "The Interfaces" page (§4.6 entry 2's
    external GPIO headers, P6-11) —
    `Clockwork_PicoCalc_Assembly_Guidelines.pdf`,
    https://github.com/clockworkpi/PicoCalc/blob/master/Clockwork_PicoCalc_Assembly_Guidelines.pdf
