# Phase 5.1 Spec: Serial Line Injection (on-device test automation)

**Prerequisite phases**: Phase 5 (CAS) — not a technical dependency, only a
sequencing one. The firmware surfaces this phase touches (`HomeScreen`'s Enter
path, the core-0 main loop) have been stable since Phase 3.

**Scope**: Dev tooling, not a calculator feature. Add a non-blocking stdin read
to the core-0 main loop so a host script can submit **whole lines** to the home
screen over the existing USB serial link, and have the firmware echo back the
result and its `ResultKind`. The goal is to turn hand-driven bench verification
into repeatable, scriptable checks.

Explicitly **out of scope**: per-keystroke `KeyEvent` synthesis (option 1 in
the originating wishlist entry). See §7 for the trigger that should revive it.

**Status**: **Code-complete and HW-verified on the Pico 2, 2026-08-09.** All six
tasks shipped; the D48 det ladder (5.1.6) now runs unattended and reproduces its
recorded behaviour. Pico 1 not re-flashed — the code is board-independent and
board swaps are batched to stage closures. Originally promoted 2026-08-08 from
[serial-injection-plan.md](../notes/serial-injection-plan.md), which was itself
scoped from the `wishlist.md` entry raised 2026-08-05. Sequenced ahead of
[Phase 5.2](phase5.2-spec.md) because injection is the main practical mitigation
available for 5.2's regression risk.

**Why "5.1" and not "5E"** — per the naming convention in `AGENTS.md`: lettered
sub-phases are planned work that a phase's completion depends on, dotted ones
are significant units that *turned up* and sit outside the parent phase's goals.
This is the latter. Phase 5 (CAS) was closed and merged before this existed, and
its completion never depended on it; test tooling was no part of that phase's
brief. It is also too small to warrant its own phase number.

**Reference reading**: [decisions.md](../notes/decisions.md) **D48** — the
session that motivated this. Its bench work needed roughly fifteen round-trips
of *"type this expression, read me the `stack: peak` line"*, across two boards,
to arrive at one integer (`kMaxParseDepth = 3`) and one leaf fix. Every one of
those inputs was a whole line typed at the home screen.

---

## 1. Overview

Two frictions in the bench loop were removed incidentally during the D48
session, which is what makes this worth building now rather than later:

- **Flashing no longer needs the BOOTSEL button.** `picotool load -f -x <uf2>`
  reboots the connected board, writes, and restarts it.
- **The result kind is already machine-readable.** `HomeScreen::ResultKind`
  (`src/apps/home_screen.hpp:36`) is `{kPlain, kError, kSymbolic}` — exactly the
  white / error / amber distinction. A serial readback can answer "is this
  result an exact form?" **without** the screenshot capture item in
  `wishlist.md`, which had been assumed to be a prerequisite for that.

With injection, keyboard input is the last manual step remaining in the loop.

## 2. Current state

Confirmed by reading, 2026-08-08:

| | state |
|---|---|
| `stdio_init_all()` | called, `src/main.cpp:325` — output only today |
| stdin reads | **none anywhere in the firmware**; `getchar_timeout_us()` is unused SDK capability, not a missing dependency |
| key drain loop | `src/main.cpp:649-676`, core 0: `keyboard().poll()` → `power::note_key()` → `mgr.handle_key(ev)` |
| Enter handling | `src/apps/home_screen.cpp:827-848` — trim, `handle_command()`, else `evaluate_input()`, `invalidate()` |
| typed commands | `HomeScreen::handle_command()`, `src/apps/home_screen.cpp:694` (`cls`/`diag`/`cas`/`plot`/…) |
| newest result | `result_full_[128]`, `src/apps/home_screen.hpp:68` |
| host test coverage of `HomeScreen` | **none** — no host test includes it |

## 3. Design

### 3.1 Extract `HomeScreen::submit_line()`

The Enter body at `home_screen.cpp:827-848` does trim → `handle_command()` →
else `evaluate_input()` → `invalidate()`. Injection must run *that exact
sequence*.

Calling `handle_command()` directly — as the original wishlist entry suggested —
would silently skip math evaluation, producing a tool that reports success on
`cls` and nothing at all on `2+2`. That is worse than no tool, because it fails
in the direction of false confidence.

Move the body verbatim into `bool HomeScreen::submit_line(const char* line)` and
have the Enter case call it. **Both paths then cannot drift**, which is the
entire correctness argument for trusting an injected result as equivalent to a
typed one.

Behaviour-preserving refactor. No host coverage protects it (§2), so keep the
extraction literal — move, do not rewrite.

### 3.2 Non-blocking stdin poll

In the `while (true)` body (`src/main.cpp:401`), beside the key drain and before
`render_frame()`:

- `getchar_timeout_us(0)` in a bounded loop, accumulating into a **static** line
  buffer (bss, not the main-loop frame — same reflex as D47/D48).
- Terminate on `\n`; cap at `config::kMaxExprLen` (256). Discard overlong lines
  with a printed error rather than truncating, which would otherwise submit a
  valid-looking but wrong expression.
- Bound the per-frame character count so a chattering host cannot starve
  rendering, mirroring the key drain's existing `kMaxEventsPerFrame` /
  `kDrainBudgetUs` guards.

### 3.3 Wire protocol

Deliberately minimal. Nothing reads stdin today, so there is no collision risk
and no need for framing beyond newlines.

```
<line>\n            submit <line> to the home screen
```

The firmware auto-echoes one line per submission:

```
inject: "<expr>" -> "<result>" kind=plain|symbolic|error
```

Auto-echo rather than a separate readback command: one round trip, and it
composes with the existing `stack: peak` line, which already prints immediately
on any new high-water mark (`src/main.cpp:545`). A host script gets both the
answer and its stack cost from a single write.

**Screen targeting**: if the current screen is not the home screen, call
`mgr.pop_to_root()` first and say so (`inject: popped to home`). Deterministic,
and matches what a human would do. Rejecting with an error was considered and is
worse for unattended scripts.

**Compile-time gate**: `PICOCALC_SERIAL_INJECT`, default ON, following the
existing `PICOCALC_*` `add_compile_definitions` convention
(`CMakeLists.txt:228-229`). Not a security control — it is a calculator — but it
keeps a shipping build free of a path that lets the USB port drive the UI.

### 3.4 Host script

`scripts/serial-console.py`, carrying forward two things the read side already
learned:

- **Assert DTR/RTS before writing.** `scripts/serial-capture.py:9-13` documents
  this for reads (plain `cat` silently reads nothing); a non-interactive write
  is likely to be dropped the same way.
- **Reconnect across reboots.** `serial-capture.py` spins on a dead fd after a
  device drop and **silently lost a whole test pass on 2026-08-08**. Fold in the
  reconnecting variant written during that session.

Interface: send a line, wait for the matching `inject:` echo, return
`(result, kind)` plus any `stack:` / `fault:` lines observed. That is exactly
the primitive the D48 session needed fifteen times.

## 4. Task breakdown

| # | Task | Est. hrs | Acceptance |
|---|------|----------|------------|
| 5.1.1 | Extract `HomeScreen::submit_line()`; Enter case calls it | 1 | No behaviour change; typed Enter still evaluates, commands still dispatch |
| 5.1.2 | Non-blocking stdin poll + static line buffer, behind `PICOCALC_SERIAL_INJECT` | 2 | Overlong and empty lines rejected cleanly; render loop not starved |
| 5.1.3 | Auto-echo line with `kind`, plus pop-to-home notice | 1 | `2+2` → `kind=plain`; `sqrt(2)` → `kind=symbolic`; `1/0` or a syntax error → `kind=error` |
| 5.1.4 | `scripts/serial-console.py` with DTR/RTS assert and reconnect | 2 | Survives a `picotool load -f -x` reflash mid-session without losing the stream |
| 5.1.5 | Round-trip check on the diag screen's existing key echo (`src/main.cpp:214-215`) | 0.5 | Inject → echo observed with no new firmware code on that screen |
| 5.1.6 | Re-run the D48 det ladder unattended | 1.5 | Reproduces the recorded peaks (3,860 of 4,096 on the Pico 2) without hand typing |
| 5.1.7 | `mode [keyword]` command — read/set angle, number and display mode from the home screen | 1.5 | The DEGREE-folding and RECT/POLAR checklists run unattended |
| | **Total** | **9.5** | |

**5.1.7 was added during the build, not planned.** With the rest of 5.1 working,
angle and number mode were the only thing keeping two whole checklists
hand-driven: they live on the MODE screen behind arrow-key navigation, and none
of the sixteen typed commands touched them. A `mode` command removed that wall
for ~1.5 hrs of work and made almost the entire home screen scriptable.

Design notes worth keeping:

- **It is the only command that pushes a history entry.** Every other command is
  silent, but a setter with no feedback is unusable over serial — commands
  report only `-> command` — and echoing the new mode is what a user would want
  on screen anyway.
- **It mirrors into `graph::state()` and calls `save_graph_state()`**, exactly as
  `ModeScreen::adjust` does. Mode lives in two places; a setter that skips the
  mirror looks fine until the next MODE-screen visit or reboot reverts it.
- **Output is deliberately ASCII** — `RECT`/`POLAR`, not the screen's
  `a+bi`/`r∠θ`, which carry font glyph bytes a host script would have to decode.
- Grammar: `mode` reports; `mode rad|deg|real|rect|polar|float|sci|eng|fixN`
  sets. `fixN` is one token so the 16-byte command buffer and exact-match
  dispatch stay unchanged.

**As built, 2026-08-09 — all six done.** Three things differed from the plan
above and are worth carrying rather than quietly correcting:

1. **The line cap is 128, not 256.** §3.2 cited `config::kMaxExprLen` (256), but
   the real bound is `ui::InputLine::kCapacity` = 128, and `set_text()` is
   `strncpy`-based so it *truncates silently*. `submit_line()` rejects at 128
   instead; `serial-console.py` mirrors the limit so the sender reports the
   offending text. Truncation was the dangerous failure here — it returns a
   plausible result for an expression nobody sent.
2. **Results contain firmware glyph bytes, not UTF-8.** `\x86` is the imaginary
   unit, `\x87` the store arrow, `\x8c` a radical (full map:
   `src/gfx/font.hpp:40-54`). Decoding with `errors="replace"` collapsed them
   all to `U+FFFD`, which would have made `2i` and `2∠` compare *equal* — fatal
   in a harness whose purpose is comparing results. The script decodes latin-1
   (bijective over bytes) and renders via a `GLYPHS` table.
3. **The echo carries the serialized form, not the typeset glyphs** — `sqrt(8)`
   reports `2*sqrt(2)`, which renders on screen as `2√2`. This does not weaken
   the `ResultKind` finding: `kind=symbolic` vs `plain` *is* the amber/white
   answer, which was the capability §1 claimed.

## 5. Risks and mitigations

- **The refactor is unprotected.** No host test touches `HomeScreen`. *Mitigation*:
  keep the extraction literal; use 5.1.5's zero-new-code round-trip as the first
  on-device check.
- **Injection is not a keyboard.** It cannot exercise APD wake, the HOME
  intercept, modifier chords, or the STM32 path. *Mitigation*: state this where
  the automated results are read, so a green script is never mistaken for a
  green keyboard. Anything keyboard-shaped stays a hand test.
- **APD interaction.** Injected lines should mark activity so a soak run does not
  sleep mid-test — which also makes APD itself untestable by injection.
  Accepted; that is §7's territory.
- **No new stack depth.** Injection enters `evaluate_input()` at the same depth a
  typed Enter does, so it changes no D48 budget. Recorded because the temptation
  will be to call it from somewhere deeper.

## 6. Open questions

| # | Question | Notes |
|---|----------|-------|
| P5.1-1 | Should injected lines enter home-screen history? | History is user-facing; test traffic polluting it may be undesirable. `handle_command()` already excludes commands from history — same treatment may fit. |
| P5.1-2 | Should the echo include the `stack: peak` value inline? | It already prints separately on a new high-water mark; inlining couples two independently useful outputs. |
| P5.1-3 | Does this justify adding a host-test job to CI? | Separate work, but 5.1's automation makes it more tempting — CI currently runs build, lint and validate-docs only, never the 2,100+ host checks. |

## 7. Deferred: option 1 (per-keystroke `KeyEvent` synthesis)

**Revive when testing friction is genuinely per-keystroke** rather than
per-line — repeated editor navigation, modifier chords, slot-editor field
walking, or APD/wake behaviour.

Design already sketched in `wishlist.md`: synthesize `platform::KeyEvent`s
(`src/platform/keyboard.hpp:10-126`, already board-agnostic and pre-translated,
*not* raw STM32 scancodes) into the same drain path at `src/main.cpp:649-676`, so
injected keys exercise `power::note_key()`, the HOME intercept and every screen
identically to physical ones.

Phase 5.1 does not block it — they share the stdin poll (§3.2) and the host
script (§3.4); option 1 only adds a second message type to the protocol.

## References

- [serial-injection-plan.md](../notes/serial-injection-plan.md) — the scoping
  document this spec was promoted from
- [decisions.md](../notes/decisions.md) D47, D48 — the sessions that motivated it
- [wishlist.md](../notes/wishlist.md) — original entry, and the sibling
  screenshot-capture item (no longer a prerequisite, see §1)
