# Serial line injection for on-device test automation — plan

**Status**: plan only, not built. Scoped 2026-08-08 from the `wishlist.md`
entry "Serial key injection for on-device test automation" (raised 2026-08-05).
Dev tooling, not a calculator feature. No phase home yet.

**Scope**: option 2 from the wishlist entry — **whole-line submission to the
home screen**. Option 1 (per-keystroke `KeyEvent` synthesis) is deliberately
deferred; see "Deferred: option 1" for the trigger that should revive it.

## Why now

The wishlist entry's motivation was "judgment calls that need a bench session".
The 2026-08-08 D48 session is the strongest evidence yet: roughly fifteen
round-trips of *"type this expression, read me the `stack: peak` line"*, across
two boards, to arrive at a single integer (`kMaxParseDepth = 3`) and one leaf
fix. Every one of those inputs was a whole line typed at the home screen.

Two frictions were removed by accident during that session, which is what makes
this worth doing now:

- **Flashing no longer needs the BOOTSEL button.** `picotool load -f -x <uf2>`
  reboots the connected board, writes, and restarts it.
- **The result kind is already machine-readable.** `HomeScreen::ResultKind`
  (`home_screen.hpp:36`) is `{kPlain, kError, kSymbolic}` — exactly the
  white/amber/error distinction. A serial readback answers "is rung 4 amber?"
  **without** the screenshot wishlist item, which had been assumed to be a
  prerequisite.

With injection, input is the last manual step in the loop.

## What exists, what doesn't

Confirmed by reading, 2026-08-08:

| | state |
|---|---|
| `stdio_init_all()` | called, `main.cpp:325` — output only today |
| stdin reads | **none anywhere in the firmware**; `getchar_timeout_us()` is unused SDK capability, not a missing dependency |
| key drain loop | `main.cpp:649-676`, core 0, `keyboard().poll()` → `power::note_key()` → `mgr.handle_key(ev)` |
| Enter handling | `home_screen.cpp:827-848` — trim, `handle_command()`, else `evaluate_input()`, `invalidate()` |
| typed commands | `handle_command()`, `home_screen.cpp:694` (`cls`/`diag`/`cas`/`plot`/…) |
| newest result | `result_full_[128]`, `home_screen.hpp:68` |
| host test coverage of `HomeScreen` | **none** — no host test includes it |

## Design

### 1. Extract `HomeScreen::submit_line()`

The Enter body at `home_screen.cpp:827-848` does trim → `handle_command()` →
else `evaluate_input()` → `invalidate()`. Injection must run *that exact
sequence*, not call `handle_command()` alone (which would silently skip math
evaluation — a subtly wrong tool that reports success on `2+2`).

Move that body verbatim into `bool HomeScreen::submit_line(const char* line)`
and have the Enter case call it. **Both paths then cannot drift**, which is the
whole correctness argument for trusting injected results.

Behaviour-preserving refactor; no host coverage exists to protect it, so it
wants careful review plus the on-device round-trip below.

### 2. Non-blocking stdin poll in the main loop

In the `while (true)` body (`main.cpp:401`), beside the key drain and before
`render_frame()`:

- `getchar_timeout_us(0)` in a bounded loop, accumulating into a **static** line
  buffer (bss, not the main-loop frame — same reflex as D47/D48).
- Terminate on `\n`; cap at `config::kMaxExprLen` (256) and discard overlong
  lines with a printed error rather than truncating into a valid-looking
  expression.
- Bound the per-frame character count so a chattering host cannot starve
  rendering, mirroring the key drain's existing `kMaxEventsPerFrame` /
  `kDrainBudgetUs` guards.

### 3. Wire protocol

Deliberately minimal. Nothing reads stdin today, so there is no collision risk
and no need for framing beyond newlines.

```
<line>\n            submit <line> to the home screen
```

and the firmware auto-echoes one result line per submission:

```
inject: "<expr>" -> "<result>" kind=plain|symbolic|error
```

Auto-echo rather than a separate readback command: one round trip, and it
composes with the existing `stack: peak` line, which already prints
immediately on any new high-water mark (`main.cpp:545`). A host script gets
both the answer and the cost from one write.

**Screen targeting**: if the current screen is not the home screen, call
`mgr.pop_to_root()` first and say so in the echo (`inject: popped to home`).
Deterministic, and matches what a human would do. The alternative — reject with
an error — was considered and is worse for unattended scripts.

**Compile-time gate**: a CMake option (`PICOCALC_SERIAL_INJECT`, default ON)
following the existing `PICOCALC_*` `add_compile_definitions` convention
(`CMakeLists.txt:229`). Not a security control — it's a calculator — but it
keeps a shipping build free of a path that lets the USB port drive the UI.

### 4. Host script

Extend the read-side conventions already proven in `scripts/serial-capture.py`
into `scripts/serial-console.py`:

- **Assert DTR/RTS before writing.** The read side learned this the hard way
  (`serial-capture.py:9-13`); a plain non-interactive write is likely to be
  dropped the same way `cat` silently reads nothing.
- Reconnect across reboots. The scratch `serial-follow.py` written during the
  D48 session already does this and should be folded in — `serial-capture.py`
  spins on a dead fd after a device drop, which silently lost a whole test pass
  on 2026-08-08.
- Interface: send a line, wait for the matching `inject:` echo, return
  `(result, kind)` plus any `stack:`/`fault:` lines seen. That's the exact
  primitive the D48 session needed fifteen times.

## Risks

- **The refactor is unprotected.** No host test touches `HomeScreen`. Mitigation:
  keep the extraction literal (move, don't rewrite), and use the diag screen's
  existing key echo (`main.cpp:214-215`) as a zero-new-code inject→verify
  round-trip.
- **Injection is not a keyboard.** It cannot exercise APD wake, the HOME
  intercept, modifier chords, or the STM32 path. Anything shaped like a
  *keyboard* bug stays a hand test. Write that down where the automated results
  are read, so a green script is not mistaken for a green keyboard.
- **APD interaction.** Injected lines should mark activity so a soak run does
  not sleep mid-test — which also means APD itself becomes untestable by
  injection. Acceptable; that is option 1's territory.
- **No new stack depth.** Injection enters `evaluate_input()` at the same depth
  a typed Enter does, so it does not change any D48 budget. Worth stating
  explicitly because the temptation will be to call it from somewhere deeper.

## Work breakdown

1. `submit_line()` extraction + Enter case calls it. No behaviour change.
2. Stdin poll + line buffer in the main loop, behind `PICOCALC_SERIAL_INJECT`.
3. Auto-echo line, including `kind` and the pop-to-home notice.
4. `scripts/serial-console.py` with DTR/RTS assert and reconnect.
5. Round-trip check on the diag screen's key echo, then a real one: re-run the
   D48 det ladder unattended and confirm it reproduces the recorded peaks
   (3,860 on the Pico 2).
6. Docs: graduate the `wishlist.md` entry, note the tool in `AGENTS.md` so
   future sessions reach for it before asking for hand tests.

## Deferred: option 1 (per-keystroke `KeyEvent` synthesis)

Revive when testing friction is genuinely *per-keystroke* rather than
per-line — i.e. when a bench session needs repeated editor navigation, modifier
chords, slot-editor field walking, or APD/wake behaviour. The design is already
sketched in `wishlist.md`: synthesize `platform::KeyEvent`s
(`keyboard.hpp:10-126`, already board-agnostic and pre-translated) into the same
drain path at `main.cpp:649-676`, so injected keys exercise `power::note_key()`,
the HOME intercept and every screen identically to physical ones.

Option 2 does not block it — they share the stdin poll and the host script, and
option 1 only adds a second message type to the protocol.
