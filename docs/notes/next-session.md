# Start here — next session

**Last session:** 2026-08-29 — **Phase 6.4 is most of the way done.** The
calculator now builds and runs as a native application, draws its own
documentation screenshots, and **issue #52 has been photographed and
root-caused**. Open PR:
**[#60](https://github.com/moodoki/graphite_picocalc_gc/pull/60)**, 17
commits on `phase-6.4`, all checks green.

> ## What is done, and what is left
>
> Done: **6.4.0** (spike), **6.4.1** (shared source list, both `.uf2`s
> byte-identical), **6.4.2** (POSIX storage), **6.4.3** (MicroPython),
> **6.4.4** (images + key scripts, **closes #33**), **6.4.7** (#52
> verified), most of **6.4.6** (CI) and **6.4.9** (docs).
>
> Left: **6.4.8**, the chrome sweep — unblocked and ready, and the plan is
> already written in the spec. It files every defect as an issue and fixes
> none; the fixes are a separate session. **6.4.5**, SDL, which now also
> has to build the sound seam (see below). And clang-tidy, still not in CI
> and still unable to see `host/`.
>
> Three things were left for the developer to decide, deliberately. One is
> settled: the **#52 image is posted** (comment `5461573497`), with the
> 4-character finding, which completes 6.4.7's gate. Still open: whether
> `host-shot` should join the release gate (it currently does not —
> blocking a firmware release on a screenshot seemed the wrong trade), and
> a go-ahead before 6.4.8 opens several `area:ui` issues on a public repo.

> ## Read this before trusting anything the host build says
>
> `docs/host-build.md` is the guide. The section that matters is the one
> about what it does **not** model: no SRAM ceiling, no FPU difference, no
> strip pipeline, no panel, no PSRAM. **A green host build is not a
> hardware verification**, and 6.4 changes nothing about how any other
> phase is verified.
>
> The hazard is not that anyone believes a desktop has 15.2 KB of SRAM. It
> is that the host build is *pleasant* — fast, visual, no flashing — so it
> gets used first and trusted one step further than it should.

> ## Four bugs, and three of them were the same mistake
>
> **A stub that returns a tidy answer is still returning a wrong one.**
>
> - `stack_total()` returned **0** on host, reasoning that a screenshot
>   should not claim SRAM the machine lacks. MicroPython derives its
>   recursion limit from it, so the budget of zero made `mp_cstack_check`
>   fail on its first call — and the raise happened while printing the
>   exception that raise produced, which hung. It reports the real stack
>   from `getrlimit` now.
> - `gen-doc-images.py` wrote its scratch PPM **inside the fixture
>   directory the file manager photographs**, so the picture contained the
>   scratch file and `--check` wrote one file more than a plain run. The
>   image set could never have been stable. Caught by the drift check on
>   its first run.
> - The `host-shot` CI job had **no `submodules: recursive`** — correct
>   when written in 6.4.0, wrong the moment 6.4.3 made MicroPython a
>   dependency. Impossible to catch locally, where the submodule is
>   already checked out.
>
> The fourth was months old: **20 `test_matrix` error checks relied on
> unspecified argument evaluation order** —
> `check_err(matops::add(..., &err), err, ...)` writes `err` and reads it
> in the same argument list. Clang calls first and passes; **GCC reads
> first and fails all 20**. The firmware was fine; the tests had been
> passing by luck on one compiler, and the firmware ships built by
> `arm-none-eabi-gcc`, which is not that compiler. Found within an hour of
> CI running the suite for the first time.

> ## Three things the spec asserted that reading the source disproved
>
> All three were caught before writing code, by reading the files rather
> than compiling them.
>
> - **`graph_screen.cpp` never needed a guard.** Its whole coupling was
>   two `time_us_64()` calls, and `platform::uptime_us()` had existed all
>   along. D94's guard count is **2, not 3**.
> - **`framebuffer.cpp` gates its render body on the raw `PICOCALC_PICO2`
>   macro**, not on `config::kUseFullFramebuffer`. The `config.hpp` fold
>   alone would have compiled, linked, run and **drawn nothing**, with no
>   diagnostic.
> - **There is no `platform::Sound`.** The spec said we carried the
>   abstraction the fork added; we do not, and `drivers/pwm_sound` has
>   **zero callers** — this calculator has never made a sound. 6.4.5 must
>   build the seam and wire the firmware side first, so it moved 8 → 10 hrs
>   and got *more* separable, not less. **Scoped 2026-08-29**: the firmware
>   half is written and reviewed, **not** flashed and listened to. Nothing
>   calls it, so there is nothing to verify it against; the first feature
>   that wants audio brings its own caller and verifies it then. The
>   desktop backend is still verified, because it can be run.
>
> The guard count stayed at 2 through the whole phase. Twice more a third
> guard was the obvious move and twice the answer was to put it behind
> `platform::` instead — `stack_top()` and `key_names`. Each cost tens of
> bytes of flash and **zero SRAM**.

> ## Two traps in comparing binaries, both found the hard way
>
> 6.4.1's gate is "both `.uf2`s byte-identical". Two things silently
> invalidate that, and **both are now pinnable**:
>
>   cmake -DPICOCALC_BUILD_ID=fixed -DPICOCALC_BUILD_DATE="Jan  1 2000" ...
>
> `PICOCALC_BUILD_ID` bakes in the git hash, so any comparison **across a
> commit** fails on the hash. The Pico SDK bakes in `__DATE__` through
> `pico_standard_binary_info`, so any comparison **across midnight** fails
> on the calendar — which is exactly how it was found: a comparison that
> passed in the evening failed the next morning with the source untouched.
>
> **A binary-comparison check is only as good as the number of things it
> holds fixed, and the way you discover you missed one is a false alarm.**
>
> Also worth keeping: splitting 6.4.1 into two commits was load-bearing.
> Regrouping the 118 files by target, changing nothing else, grew the Pico
> 2 by **8 bytes** of link-order padding. In one commit with the
> extraction those 8 bytes would have been indistinguishable from the
> shared list changing the firmware.

> ## How to drive it
>
>     cmake -B build/host -S host && cmake --build build/host
>     ./build/host/graphite-shot --shot home.ppm
>     python3 scripts/gen-doc-images.py           # regenerate the doc images
>     python3 scripts/gen-doc-images.py --check   # what CI runs
>
> `--eval` submits a line to the home screen, `--key` and `--keyscript`
> replay keys, `--run` executes a Python file through the same `exec_file()`
> an SD app uses. Storage is `$PICOCALC_HOME`, else `~/.picocalc`.
>
> **Key names come from `src/platform/key_names.*`**, shared with the
> MicroPython bindings so a name cannot mean one thing to a key script and
> another to an app.

---

**Previous session:** 2026-08-23 (earlier) — **Phase 6 CLOSED, merged, and tagged
`v0.5.0`** (PR #56, 58 commits). Ten issues closed, four filed, a Pico 1
board-swap pass, and two measurements that overturned the estimates they
were meant to confirm.

> ## Where the project actually is
>
> **Phase 6 is done and on `main`.** 6A (app framework, launcher, shared
> text editor, file browser), 6B (MicroPython + the `calc` module + SD app
> manifests) and 6C (Notepad) all shipped. 22 host suites / **3,386
> checks**; free SRAM **15.2 KB** (Pico 1) / **32.6 KB** (Pico 2); core-0
> stack high-water **2,432 of 4,096**.
>
> Both boards are hardware-verified. The Pico 1 currently has `v0.5.0`
> firmware on it.
>
> **There is no phase in progress.** The next one is unscoped — see
> "What's next" below.

> ## The two measurements, because both changed a standing belief
>
> **#38 is closed, and no Python-free build is needed.** 6B left 15.2 KB
> free, above the ~12 KB the issue set as its own threshold — so restoring
> D70 lever C's 16 px render strip was affordable. It was built, flashed,
> and measured at **3.4%, not the ~6.3%** the issue was built on; that
> figure predates 6B and the render mix moved under it. Reverted, because
> 10,008 bytes — two thirds of the Pico 1's whole remaining headroom — for
> 4.8 ms on a 140 ms frame is the wrong trade. The record is in
> `kStripHeight`'s own comment in `config.hpp` so nobody runs it a third
> time. **If it is ever raised again it must be Pico-1-only**: `strip_buf`
> is allocated on both boards but only the Pico 1 renders in strips.
>
> **§4.4's heap estimate was right, and my first measurement of it was
> wrong.** The periodic dataset costs **11,536 B** of the Pico 1's 40 KB
> heap against an estimate of 11,504; the app's bytecode **8,624 B**
> against ~8,500. My first run read 2.7 KB worse and I wrote it up as the
> estimate being optimistic — it was **my own REPL globals still live**.
> Runs 2 and 3 were byte-identical on a clean interpreter. *An absolute
> `mem_free` reading only means something on a clean interpreter; the
> delta across load is the robust number.*

> ## What the Pico 1 pass found
>
> **A stack regression of mine, one morning old.** Bisecting by workload
> after a clean reboot: arithmetic 2,116, CAS 2,432, matrix and
> 999-element list work no new high — then `calc.list_files` at **3,004**,
> the deepest frame in the firmware. The window is paid for **twice** on
> the way down (glue + adapter, both live at the leaf), so 576 bytes was
> really 1,152. Halved both; it no longer sets a high-water mark at all.
> **The lesson generalises: a buffer that crosses the C/C++ boundary is
> allocated on both sides of it.**
>
> **D53's fix holds, 30/30** — #24's exact shape replayed thirty times,
> element 1 correct every run against ~8/30 corrupt before the fix. Root
> cause still unknown; **#24 stays open** and still needs the diagnostic
> build its own notes describe (~1 hour).

> ## Open bugs — 3
>
> - **#52** softkey labels truncate: `MKDIR` renders `MKDI`.
>   `draw_softkeys` truncates to 6 chars a cell by design, so the new
>   text-fits lint gate deliberately cannot see it. **Diagnosed 2026-08-29
>   off the generated image:** `max_chars = (320/6 - 2) / 8 = 6` is applied
>   to the string *after* `"%d:"` has been prefixed, so the label budget is
>   **4, not 6**. `CUT`/`MOVE`/`REN` fit, `MKDIR` never could. The comment
>   at `files_screen.cpp:589` claiming both labels are "within
>   `draw_softkeys`' 6-character cell" counts the label and forgets the
>   prefix. Fix belongs to 6.4.8's sweep, which files and does not fix.
> - **#54** ESC out of an app reports the run as `raised` and prints a
>   traceback — **extended 2026-08-23** to cover the force-quit unwind: a
>   deliberate kill still needs a third press to dismiss the wreckage.
>   `interrupt_pending_` already distinguishes the case at the
>   `micropython_embed` seam; only the return shape has to carry it.
> - **#24** D53 root cause. `hw-pending`, `board:pico1`.

> ## What's next — genuinely open
>
> No phase is scoped. The candidates, in the shape the spec tracks them
> (letters = planned work, dots = what turned up outside a phase's goals):
>
> - **6.3 — native compiled `.uf2` apps.** Draft spec exists
>   (`phase6.3-spec.md`), **D88-D91 all still PROPOSED, not accepted**.
>   Dotted, so it never gated Phase 6 and still doesn't. The two things
>   most worth attacking are whether the RP2350 chain-jump and ATRANS
>   survive an SDK image's own boot, and whether delegating the Pico 1 to
>   `uf2loader` is the right boundary or a gap that will be resented.
> - **6.1** home-screen convenience scripts (§9.3), **6.2** PCM sampler
>   audio engine (§9.4). Both candidates, unstarted.
> - **21 open issues**, mostly features and chores. #19 (screenshot
>   capture) is worth pulling forward out of proportion to its label: this
>   session could flash, drive Python, push files to the SD card and read
>   the whole card over serial, and still could not see a single visual
>   fix. It is the cheapest multiplier on how we actually work.
>
> CI still runs neither host tests nor clang-tidy.

> ## Working notes that paid off, worth reusing
>
> - **Serial injection is a full remote.** `PICOCALC_SERIAL_INJECT` is on
>   by default and submits lines to the home screen, so `py …` runs
>   MicroPython with `calc` bound and `print()` comes back over USB. This
>   session used it to push a 10 KB `main.py` to the SD card in chunked
>   `append_file` calls (byte-verified by length + checksum), enumerate
>   the whole card, and read scripts off it — **no card removal, no
>   keypresses**. `scripts/serial-capture.py` needs DTR; plain `cat` reads
>   nothing.
> - **A watch tuned only for failure is silent about everything else.**
>   Three times this session a serial filter dropped the thing that
>   mattered: the body of a traceback, a script's own instrumentation, and
>   then a heartbeat that flooded a per-line notifier. Log to a file with
>   timestamps and report transitions, not lines.
> - **A stubbed `calc` proves layout arithmetic and nothing about the
>   API.** The periodic rewrite ran clean against a host stub and was
>   still checked against `mp_calc_module.c`'s real arities afterwards.

---

**Previous session:** 2026-08-16 — **the periodic table app**
(`examples/apps/periodic/`), §4.6 entry 1, built as the pressure test
for the `calc` module. It found the gap it existed to find, and the
measurement §4.4 has wanted since before 6B.1.

> ## The gap
>
> **A script could not tell which arrow had been pressed.** A key event
> reported `code` — a `platform::Key` enumerator Python has no names for
> — and `ch`, which is `None` for every arrow. `key_held("up")` resolves
> a name but asks a *different* question, and by the time a blocking
> `wait_key()` returns the key may be up again. Events now carry `name`
> ("up", "enter", "f1", …) from the same table `key_held` reads, so the
> two cannot drift (D87).
>
> 6B.9's keyboard bindings had passed their own hardware pass. This is
> the same shape as the `calc.input` ENTER bug the session before: **an
> API can be verified through every path that reports a raw value and
> still be unusable by anything that tries to act on one.**

**The §4.4 number**: a 118-element reference dataset costs **11,504
bytes** of Python heap (~97 bytes an element), held as parallel lists
indexed by atomic number. About 3x the spec's estimate, and it still
leaves ~21 KB of the Pico 1's 40 KB — interpreter overhead measures at
~8.5 KB with the app compiled. **JSON was the wrong call** and the
entry's own reason for it (user-editability) is better served by CSV.

**Worth reusing: the app's logic was verified on the host first**, with
a stubbed `calc` module checking that all 118 cells land in unique
in-range positions, no arrow leaves the table, and every element is
reachable from hydrogen. That caught the real trap for free — straight
up from a lanthanide is column 3, empty in every main row, because it is
the column the f-block was pulled out of. Every SD app can be tested
this way; only drawing needs a panel.

**Phase 6B remains code-complete.** Free SRAM **15 KB** (Pico 1) /
**24 KB** (Pico 2); 21 host suites / **3,277 checks**.

> ## Next: close Phase 6
>
> 1. **A Pico 1 pass** on 6B.15/16 and the periodic app — deferred under
>    the board-swap policy (swaps only at major stage closures, and the
>    close is one). The app's heap headroom on 40 KB is computed, not
>    measured, so it is worth actually running there.
> 2. **Merge `phase-6`** — unmerged for the whole phase by standing
>    instruction.
> 3. **Issue #38** (the Python-free build, D78) unblocks: deferred by
>    its own terms until the final SRAM numbers were known, and they are.
>
> CI still runs neither host tests nor clang-tidy, and has only ever
> seen this branch through one manual dispatch.

---

**Previous session:** 2026-08-16 (6B close) — **6B.15 + 6B.16: SD app manifests.
Phase 6B is code-complete** (D86). A directory under `/picocalc/apps/`
with an `app.txt` in it is its own launcher tile, on the tier-2
`AppRegistry` hook unused since 6A.1. Examples in `examples/apps/`.

> ## The lesson worth carrying
>
> **A binding can be verified and still be unusable through the one
> entry point nobody drove.** 6B.9's hardware pass exercised `wait_key`,
> `key_pressed` and `key_held` — all of which report `code` — and never
> typed a line into `calc.input`. `KeyEvent::ch` is filled for printable
> ASCII only, so `calc.input()` had been waiting since 6B.9 for a `'\r'`
> the driver never produces: **ENTER did nothing**. The key queue now
> normalises ENTER/BACKSPACE/TAB/DEL where `platform::Key` is visible.
>
> The board found a second one the same way: `exec_file` compiled with
> `is_repl = true`, copied from `exec_str`, so every top-level
> expression statement printed its value — five `calc.draw_text` calls
> emitted `176 192 168 168 216` into the output pane. A file is a
> module; `exec_str` keeps REPL semantics on purpose, which is what
> makes `py 1+1` show `2`.

**The 4 KB that was 128 bytes.** 6B.1 deferred `exec_file` to here
because reading a script would cost "a second 4 KB staging buffer".
MicroPython's lexer pulls source through an `mp_reader_t` a byte at a
time, so it is a 128-byte window and an SD app's length is bounded by
the card, not by SRAM. Worth checking the premise before paying for a
deferral.

**An SD app is `ProgramScreen` in a second mode**, not a new screen —
the editor taken out, ESC pointed at the launcher. One singleton in two
modes needs both doors explicit (`queue_app` / `open_editor`), because
`HOME` pops to the root from anywhere and bypasses the ESC path.

**Verified on the Pico 2**: both tiles appear, `Hello` draws and its
canvas survives, `Quadratic` takes `calc.input` values, prints both
roots, plots, shows the graph, and ESC walks back graph → output pane →
launcher. Stack peak 2,004 of 4,096. 21 host suites / **3,275 checks**.
Free SRAM **15 KB** (Pico 1) / **24 KB** (Pico 2).

> ## Next: close Phase 6
>
> 1. **A Pico 1 pass on 6B.15/16** — deferred here under the board-swap
>    policy (swaps only at major stage closures, and the close is one).
>    Sizes are already recorded; what is unverified is behaviour, and
>    the display path's one board-specific hazard is already fixed
>    (`gfx::display_wait_idle()`).
> 2. **Merge `phase-6`** — it has stayed unmerged for the whole phase by
>    standing instruction.
> 3. **Issue #38** (the Python-free build, D78) unblocks: it was
>    deferred *by its own terms* until the final SRAM number was known,
>    and both numbers are now final.
>
> CI still runs neither host tests nor clang-tidy, and has only ever
> seen this branch through one manual dispatch.

---

**Previous session:** 2026-08-16 (Pico 2 bring-up) — **the Pico 2 has now run
Phase 6B**, and it found a two-core SPI race that only it could show
(issue #39, closed).

> ## The rule that came out of it
>
> **Nothing outside the render loop may touch the panel or the scratch
> buffers without calling `gfx::display_wait_idle()` first.**
>
> D85 had asserted that `render_frame` always leaves core 1 idle before
> returning. True on the Pico 1 (strip mode ends with `drain_acks()`);
> **false on the Pico 2**, whose async full-frame push returns while core
> 1 is still transferring and drains at the *start* of the next frame.
> A 6B.8 binding therefore shared the SPI peripheral and the `staging`
> buffer with core 1, and one `calc.draw_rect` cost the panel its colour
> depth globally until reboot. D85 is corrected in place.
>
> **Three hypotheses were built and disproved first.** What broke it was
> a negative result: a full-width push degraded the panel identically to
> a narrow one, so geometry was irrelevant and the difference had to be
> *which core held the bus*. Look for the asymmetry that is not the
> obvious one.

**Two Pico 2 differences, both benign and worth knowing**: the M33's FPU
makes frames shallower, so the stack guard *permits deeper nesting there*
(a 10x10 eigen four frames down peaks at 3,440 of 4,096 and runs, where
the Pico 1 refuses it two frames down) — which is the runtime-bytes check
working as designed. And the 96 KB heap means D77's churn loop simply
completes instead of triggering the fragmentation reset.

**Phase 6B is verified on both boards.** Free SRAM 16 KB (Pico 1) and
26 KB (Pico 2).

---

**Previous session:** 2026-08-16 (later still) — **6B.8-6B.10: the `calc`
module is complete** (D85). A script can draw on its own canvas, read
keys, and read and write SD files. 21 host suites / 3,241 checks; free
SRAM **16 KB** (Pico 1) and 26 KB (Pico 2).

> ## Two traps this chunk walked into
>
> **A symptom with two causes looks like a fix that did not work.** The
> canvas drew and was instantly repainted by the editor. Fix one —
> `ProgramScreen::on_key` skipping `invalidate_all()` when the run took
> the panel — was necessary and *not sufficient*: while dirty tracking
> was off, `take_dirty()` had been resetting the band to the full screen
> every frame, so switching tracking on inherited one last full repaint.
> `set_dirty_tracking(true)` now clears the band. Only hardware showed
> either.
>
> **`--gc-sections` makes an unused buffer free until you touch it.**
> The canvas borrows the render loop's buffers rather than allocating.
> Referencing `strip_buf` from the scratch accessor **resurrected 10 KB
> on the Pico 2**, where full-framebuffer mode had left it unreferenced
> and the linker had dropped it. The accessor is now `#if`-split: Pico 1
> lends `strip_buf`, Pico 2 lends the full framebuffer.

**D85 also records**: the canvas is **span-exact** (no readback, so
`draw_text` takes a background colour and diagonal lines are horizontal
runs) with the vendored `read_buffer_spi()` noted as the additive
upgrade path; and D81's "one poller" is really **one drain, one queue**,
because a blocking `wait_key()` sits where the VM hook never runs.

**Nothing here needed a stack guard** — canvas, key and file bindings
peaked at 1,748-2,156 of 4,096 against 2,848 for a plain `calc.eval`.
They reach gfx and Storage, not the evaluator.

> ## Next: 6B.15 + 6B.16, the last committed Phase 6 work
>
> SD app manifests — scripts under `/picocalc/apps/<name>/` appearing as
> their own launcher tiles, on the tier-2 `AppRegistry` hook that has
> existed unused since 6A.1. ~12 hrs. `exec_file` was deferred to here
> from 6B.1, and now has everything it needs.
>
> After that, Phase 6 closes: the **Pico 2 has still never run a line of
> 6B code**, CI has only ever seen the branch via one manual dispatch,
> and issue #38 (the Python-free build, D78) is deferred *by its own
> terms* until the final SRAM number is known — which it nearly is.

---

**Previous session:** 2026-08-16 (later) — **6B.7 + 6B.17: lists and
matrices** (D84). A script can read and write the six lists and the ten
matrices. 21 host suites / 3,203 checks; free SRAM 17 KB / 26 KB.

> ## The number the whole task existed for
>
> D77 measured a 400-iteration loop accumulating samples in a Python list
> **exhausting the 40 KB heap**. The same 400 iterations through
> `calc.list_append` cost **16 bytes** (32,432 → 32,416 free), because
> `math::Array` moves to PSRAM above ~256 elements. §4.6 entry 1's
> data-logging app is now possible.
>
> **The eigen guard is set by margin, not by what survives.** A
> $10\times10$ Hilbert matrix peaked at 3,192 (904 spare) at top level
> and **3,864 (232 spare)** two Python functions deep. The two-deep call
> *worked* and is still refused: `fault.cpp`'s `kLiveMargin` assumes an
> IRQ frame can exceed 256 bytes, and paint-and-scan only records an ISR
> that actually fired. `kEigenStackNeed = 1900` keeps ~400 spare, making
> eigenvalues top-level-only like `solve()`. **Do this for every
> remaining binding.**

**Also settled** (D84): matrices cross as **nested Python lists**, not
handles — `Array` is non-copyable and PSRAM-backed with ten slots, so a
value-returning `calc.matrix()` would exhaust them. One file-static
scratch, `clear()`ed per call. `calc.eigenvalues` returns a **flat**
list, because `matops::eigenvalues` makes a 1-D Array on purpose so
results flow into l1-l6.

**Persistence for lists and matrices is deferred to end-of-run**
(`calc_api_flush_run`, called from `exec()` before the GC collect) and
verified across a reflash. Variables and graph state still save
immediately — a variable image is 456 bytes, a list can be 10,000
elements. `calc.store` in a tight loop has the same per-write cost and
was knowingly left alone.

---

**Previous session:** 2026-08-16 — **6B.6: a script can plot and analyse
graphs** (D79). `calc.plot`, `window`, `show_graph`, and `graph_zero` /
`min` / `max` / `integral` / `deriv` / `value`, all verified on the
Pico 1. Flash +2.2 KB, **free SRAM unchanged at 17 KB**.

> ## The lesson to carry into 6B.7-6B.10
>
> **Measure a new binding against a hostile input, not a convenient one.**
> `analyze_integral` recurses through `integrate_panel` — 136 bytes a
> frame, depth cap 12. Integrating `x^2-4` peaked at 2,488 of 4,096 and
> looked completely safe. Integrating `sin(1/x)`, which actually
> subdivides, peaked at **3,532 — 564 bytes spare**. The first number
> would have set the guard far too low.
>
> 6B.7's `calc.det` reaches `eigen_core`, whose **1,248-byte frame is the
> largest in the firmware**. Assume it does not fit until measured, with
> `-DPICOCALC_STACK_PROBE=ON`.

**D79** records three things D68 did not cover: `show_graph()` is
**deferred** to after `exec()` (a binding runs inside the VM inside
`on_key`, so pushing a screen there nests screen management inside
itself); there is **no per-plot colour** (slot colour is fixed per index,
and a field for one would bump the persistence magic and reset every
user's graphs — `plot()` returns the slot number instead); and plotting
**forces FUNC mode**.

**Surprising but specified**: D68's latch resets at each top-level
`exec()`, and each `py` line is one — so two `py calc.plot(...)` lines
leave one curve, not two. Within one exec they accumulate. A script from
the editor is a single exec throughout.

**Still unverified**: nobody has *looked* at a script-drawn graph, only
proven the screen switches; and the Pico 2 has never run any 6B code.

> ## §8 stopped being fully answered, and the pattern matters
>
> The spec's §8 said "every question is now answered" and that held
> through 6B.6. Building 6B.3-6B.6 raised **four** it had never asked —
> now **P6-16 to P6-19**. The one that blocked 6B.8 outright is settled:
>
> **P6-16 / D80: a drawing script owns the screen.** `calc.clear_screen()`
> enters graphics mode — a bare screen whose `render()` paints nothing —
> and `calc.draw_*` write straight through `push_rect()` as called. No
> display list (it would grow in the Python heap, D77's binding
> constraint), composition through one 320 px row buffer rather than a
> 200 KB framebuffer, and `ESC` stays live throughout. That is 6B.8's
> shape; it is not built.
>
> **All four are now resolved**, so 6B.7-6B.10 and 6B.17 are
> implementation:
>
> - **D81 (P6-17)**: the VM hook **queues** non-`ESC` key events into a
>   16-entry ring rather than discarding them, and stays the single
>   caller of `poll()`. `wait_key`/`input`/`key_pressed` drain the ring;
>   `key_held` queries directly and never had a conflict. Also fixes an
>   existing defect — type-ahead during a script is dropped today. Note
>   a drainer must loop until `fifo_empty()`, not stop on the first
>   `kNone` (hardware, 2026-07-18).
> - **D82 (P6-18)**: list bindings are in scope as **new task 6B.17**
>   (~3 hrs) — `set_list`, `get_list`, `list_append`, `stat_mean`.
>   `list_append` writes the `Array` only; the run's lists save **once**
>   when `exec()` returns. `Array` goes to PSRAM past ~256 elements, so
>   a log avoids both SRAM and the Python heap — which D77 made urgent.
> - **D83 (P6-19)**: there is **no staging buffer**. The glue allocates
>   the Python string with `vstr_init_len()` and one call fills it via
>   `read_file_range()`. `io_scratch`'s invariant never comes up.
>   **Expect to want chunked or seeking reads eventually** — a whole-file
>   read is capped by the 40 KB heap, and §4.6's JSON dataset needs
>   source *and* parsed objects live at once. That form is additive over
>   the same `read_file_range()` primitive; do not pre-build it, because
>   a chunked API nobody needs invites string concatenation and D77's
>   fragmentation.
>
> Committed 6B is now ~69 hrs, phase total ~103.
>
> **The pattern**: every question §8 anticipated was about *semantics* —
> what should `plot()` do to Y1-Y7? Every question that actually blocked
> work was about **where the code runs**: inside the VM, inside `on_key`,
> on 2,239 bytes of stack, against a pull-model renderer. None of those
> are visible from a feature description, so expect more of them in
> 6B.7-6B.10 and look for them early.

---

**Previous session:** 2026-08-15 (fourth that day) — **measured the Python
heap, found a wedge instead of a sizing answer** (D77, D78).

The question was whether the 40 KB heap could be cut to fund D70 lever
C's ~6.3% render cost. **It cannot**: MicroPython's own baseline is 544
bytes and a realistic live working set is ~8 KB, but the 40 KB buys
*churn headroom*, and a 400-iteration loop building expression strings
already exhausts it. 400 `calc.eval` calls cost 9.4 KB and returned every
byte, so the binding does not leak. D70 lever B, meanwhile, costs nothing
in normal use (peak 12 slabs of 14, `miss 0`) — **lever C is the only one
of the three with a general price**, which makes a Python-free build the
only way to get it back:
[issue #38](https://github.com/moodoki/graphite_picocalc_gc/issues/38),
reasoning in **D78**. Revisit when 6B closes and the final SRAM number is
known.

> ## Two things fixed, one worth remembering
>
> **Heap exhaustion used to wedge the `py` path until a power cycle**
> (**D77**). Not exhaustion at all — **31.5 KB free when a 512-byte
> allocation failed**. The GC does not compact, and 400 surviving floats
> among 1,200 freed strings left no run long enough to compile another
> statement. `gc.collect()` cannot help: it has to be compiled first.
> Now `exec()` collects from C after every run and rebuilds the runtime
> when the largest contiguous run drops below 1 KB, saying so through the
> script's own output.
>
> **The first fix — collect only — was flashed and did not work.** That
> failure is what produced the 31.5 KB measurement and the right
> diagnosis. Do not assume a GC problem is a volume problem.
>
> **D76 overstated the usable call depth.** Measured: `calc.eval` works
> at top level (peak 2,828) and inside ONE function (3,412, 684 B
> spare), refused inside two. `calc.eval("solve(...)")` is top-level
> only.

**CI has now run against `phase-6`** (`workflow_dispatch`, all four jobs
green), so the Linux side of the MicroPython generation works. Note
`build.yml` triggers only on `main`, tags and PRs into `main`, and **runs
neither the host tests nor clang-tidy** — both are local-only gates, so a
green PR means "it compiles", not "it works".

---

**Previous session:** 2026-08-15 (third that day) — **the `calc`
module: Python can reach the calculator.** 6B.3, 6B.4 and 6B.5 are done
and verified on the Pico 1. `import calc` gives a script `eval`,
`store`, `recall`, `simplify`, `expand`, `factor`, `diff`, `integ`,
`solve`, `complex`, `c_abs`, `c_arg`, `c_conj`.

> ## The finding that should change how you build 6B.6-6B.10
>
> **`calc.eval("solve(x^2-4,x,0,10)")` hung the board on the first
> flash** — `sp=0x20040ff0`, sixteen bytes below `__StackBottom`, which
> is D48's failure mode exactly. **D73 was right about MicroPython and
> that was the wrong question.** The deep frames belong to the
> *calculator*, and `MICROPY_STACK_CHECK` cannot see them: it guards the
> VM's own recursion, and a binding has left the VM.
> `solveexpr::substitute` alone was **1,672 bytes**, the largest frame in
> the binary, called from a VM already 1,857 bytes deep.
>
> Fixed twice: 1 KB of that frame moved to bss (1,672 → 696), and every
> binding now checks headroom and raises instead of running (**D76**).
> A binding has **~2,239 bytes** to work with from a script's top level,
> and less from every frame down.
>
> **Measure every new binding with `-DPICOCALC_STACK_PROBE=ON`**, which
> reports free stack at each binding entry. `stack: peak` cannot answer
> this — it is a high-water mark since boot. 6B.6's `calc.plot` reaches
> graph state; 6B.7's `calc.det` reaches `eigen_core`, whose 1,248-byte
> frame is now the largest in the firmware.

**The three-file split is not organization, it is the safety property**
(**D74**). `mp_calc_module.c` converts arguments and builds Python
objects; `calc_api.cpp` calls `math::` and never calls back;
`calc_api.h` is the plain-C boundary. It is not only `mp_raise_*` that
longjmps — `mp_obj_new_*` can trigger a GC, a `__del__` finalizer can
run arbitrary Python during it, and a `MemoryError` leaves from there.
So allocation happens only after the C++ leaf has returned. Follow the
same shape for every 6B.6-6B.10 binding.

That split also bought host testability: `calc_api.cpp` depends on
`math/` and nothing else, so `tests/host/test_calc_api.cpp` covers the
pipeline, the name rules and both guards with **124 checks** and no
board. The one dependency that would have broken it — persisting
variables after `calc.store` — is a function pointer the interpreter
installs.

**`calc.eval` returns a float, a Python complex, or a string** by result
kind (**D75**). A list or matrix comes back as formatted text, because
that is what `unified::evaluate_home` produces; real list data waits for
`calc.get_list`. The imaginary part is not in `HomeResult` — it comes
from **Ans**, which the VM writes for every scalar result.

**Sizes are flat**: free SRAM still 17 KB (Pico 1) and 27 KB (Pico 2);
flash +3.8 KB. The module's tables are const, so its SRAM cost is zero;
the two bss moves (−452 from the `var_store` extraction, +1,024 from
`substitute`) cancelled.

> ## Next: 6B.6-6B.10, and one gap to close first
>
> §4.2's remaining bindings are graphing (6B.6), matrices (6B.7),
> display (6B.8), keyboard (6B.9) and file I/O (6B.10). **6B.6 is the
> one that destroys user state**: per D68 a script's first `plot()`
> clears all seven Y= slots and `save_graph_state()` makes the loss
> survive a power cycle. Give it its own hardware session.
>
> **The spec has no list-bindings task at all.** §4.2 specifies
> `calc.set_list`, `calc.stat_mean` and `calc.list_append` — the last
> load-bearing for §4.6's data-logging walkthrough, since it is what
> keeps a logging run out of the Python heap — but §5's table goes
> straight from complex (6B.5) to matrices (6B.7). Add the task (~3 hrs,
> plus `calc.get_list`) before building §4.6 entry 1.

**Two things this session did not verify**: the Pico 2 has still never
been run, and the `calc` module has only been exercised through `py`
over serial, never through the ProgramScreen's RUN key. The binding path
is identical; the screen path is not covered.

---

**Session before that:** 2026-08-15 (second of four) — **MicroPython
is embedded and running on hardware.** 6B.1, 6B.2, 6B.11, 6B.12, 6B.13
and 6B.14 are done and all six on-device checks passed. You can write a
Python script on the device, run it, save it, power-cycle, and reload
it.

**MicroPython came in as a git submodule** pinned to **v1.28.0**
(**D71**, direct user instruction), not a vendored copy — and that sets
the rule going forward: small single-purpose drivers stay vendored,
large actively-maintained upstream projects become pinned submodules.
`drivers/README.md` carries both halves. **The submodule is never
edited**; `drivers/micropython_port/` is the entire local configuration
(`mpconfigport.h`, `micropython_embed.mk`, `picocalc_mphal.h`).

**A fresh clone now needs `git submodule update --init --recursive`,
and the build needs `make` plus a HOST compiler** — MicroPython's embed
port *generates* the C tree we compile, at CMake configure time.
Generation is deliberately a clean rebuild every time: the incremental
path leaves stale fragments in `genhdr/module/`, so turning a feature
off still emitted its `MP_REGISTER_MODULE` entry and the link failed on
a symbol nothing compiled any more.

**Everything touching a MicroPython header is in C**
(`src/scripting/mp_port.c`). MicroPython raises by `longjmp`, past every
intervening frame, with no diagnostic when that frame was C++ holding
something with a destructor. **This is load-bearing for 6B.3** — every
`calc` binding must be an `extern "C"` leaf.

> ## The two numbers that changed
>
> **Free SRAM: Pico 1 61 → 17 KB, Pico 2 126 → 26 KB.** 41,916 bytes
> went to a 40,960-byte heap, so **MicroPython's own static cost beyond
> the heap is under 1 KB** — the one figure §4.4 had no estimate for at
> all. `json` plus the `io` module it drags in: +4 KB flash, **zero**
> SRAM. Flash 473 → 633 KB of 2 MB.
>
> **17 KB is the real budget for the whole `calc` module**
> (6B.3-6B.10), not the 61 KB the D70 recovery banked. Re-measure at
> every step. If it gets tight, `MICROPY_CONFIG_ROM_LEVEL` (currently
> `CORE_FEATURES`) and `kPythonHeapSize` are each one constant.

**The stack risk is closed, and the contingency was not built** (**D73**).
MicroPython runs inside core 0's existing 4 KB with
`mp_stack_set_limit()` 1 KB below `__StackTop`. Measured: a 15-line
script set no new high-water mark; Python recursion to depth 40 peaked
at **3,224 of 4,096** and raised a catchable `RuntimeError: maximum
recursion depth exceeded`, 808 bytes still unused. D48's failure mode
(SP crosses into core 1's stack, machine hangs) is now a Python
exception. No PSP switch, no custom linker script, no assembly.

**Also**: the heap is a **static bss array**, not an allocation
(**D72**, amending D57) — there is no allocator, so "lazily allocated"
could not be implemented as written; what is lazy is `init`/`deinit`.
And `py <statement>` at the home screen is a one-liner REPL *and* the
harness that makes the interpreter drivable from
`scripts/serial-console.py`, which is how every measurement above was
taken without touching the keyboard.

> ## ~~Next: 6B.3, the `calc` module~~ — DONE, see the top of this file
>
> ~~§4.2's `calc` bindings and §4.5's SD app manifests are what is left of
> 6B. A script today can print, loop and compute in pure Python but
> **cannot reach the calculator at all**. Start at `calc.eval` — §4.7
> already traced its call path through `cas::evaluate_home`, and D68
> settled the `plot()` Y-slot semantics, so §8 has no open questions.~~
> `exec_file` is still deferred to 6B.15/16 (RUN executes the editor
> buffer, which the widget has just written to disk).

**Hardware-verified on the Pico 1, all six checks passed**: launcher
with three apps, write-and-run with auto-indent after `:`, a red
`raised` header with the traceback scrolled into view, **`ESC` breaking
a `while True:` via `KeyboardInterrupt`**, save → power-cycle → reload →
run, and ten enter/leave cycles with no heap drift. A 400,000-iteration
loop confirmed the VM hook can safely reach I2C from deep inside the
interpreter. **More extensive testing is deferred to a soak session**
(user's call).

**Not yet run on the Pico 2** — it builds and fits (26 KB free) but only
the Pico 1 is connected, and board swaps happen at stage closures.

**`phase-6` is not pushed and not merged** — merge to `main` still waits
for Phase 6 to close.

---

**And before that:** 2026-08-15 (first of four) — **Phase 6A + 6C.1
implemented and hardware-verified, and the SRAM tooling turned out to be
wrong.** Two distinct pieces of work, and the second one matters more.

**6A is code-complete**: `platform::AppRegistry` (two tiers per D67),
an app launcher reached from Home by **F6** *and* the `apps`/`app`
command (D58 — F6 took CAS's slot, which stays typed-only),
`FilesScreen` generalized into a `FileBrowserScreen` with directory
navigation, pick mode and file management, a shared
`ui::TextEditorWidget` split over a host-testable `ui::TextBuffer`, a
new `ui::PromptLine`, and **Notepad** as the first real app. Built in
D64's order. New host targets `test_apps` (34 checks) and
`test_text_buffer` (67). Suite 2350 → 2451.

> ## Read this before trusting any SRAM number in this repo
>
> **`size-report.sh` was omitting the entire `.data` section** — every
> headroom figure it ever printed was wrong by ~44 KB (**D69**).
> `.data` on this target has a flash LMA, an **SRAM VMA**, and SDK
> flags of `READONLY, CODE`, so Berkeley `size` bins it under *text*
> and prints `data 0`. The script summed `bss + 0`.
>
> Real free SRAM was **14.3 KB**, not the 58.2 KB D61 acted on, so the
> MicroPython heap **never fit on either board** — a ~32 KB shortfall,
> not a tuning question. The script now sums every ALLOC section whose
> VMA lands in the main SRAM bank, and prints the breakdown.
>
> **Past phases' RAM *deltas* still stand** (the omission is a
> near-constant offset within a phase); their absolute headroom claims
> do not. That includes `pre-phase5-review.md`, whose own baseline line
> reads `data 0`.

**Then 54 KB was recovered** (**D70**), each lever measured and
hardware-tested: **A** one-shot persistence buffers folded into a
shared `platform::io_scratch()` region (+15.3 KB); **B** ArrayStore
slabs 28 → 14 behind a new **PSRAM fallback** so exhaustion is no
longer fatal (+28 KB); **C** render strip height 16 → 8 (+10 KB).
**D declined** — it returns flash, not SRAM, and flash is 22.3% used.

**Pico 1 free SRAM 7.9 → 61 KB; Pico 2 83 → 126 KB**, against the
48 KB and 104 KB the two heap budgets need. Risk 6 is closed by
recovering SRAM rather than shrinking the heap.

> ## ~~The next job is 6B~~, and one number gates it
>
> **Superseded — 6B.1 landed the same day; the real figure is 17 KB, at
> the top of this file.** The ~13 KB estimate below was close.
>
> `phase6-spec.md` §0.1, §4.4 and Risk 6 are rewritten on the corrected
> figures. 6B has **~13 KB of Pico 1 spare** to fit the interpreter
> wrapper, the `calc` module and the program editor — re-run
> `size-report.sh` as **6B.1** lands rather than assuming it holds.
> P6-2 and P6-15 were both settled by **D68** (`calc.plot()` clears the
> Y-slots; `plot()` is a state write, `show_graph()` displays), so §8
> has no open questions left.

**Hardware-verified on the Pico 1**: persistence survives reboot across
all the folded buffers (lists, matrices, variables, graph state);
`ListEditorScreen::delete_row` shifts correctly and persists; the
PSRAM fallback was exercised at a hostile `kSlabCount = 4` (11
fallbacks, 30/30 sensitive checks clean); and a full visual pass at
8-px strips over home, Y=, graph + trace, list editor, stats, Notepad
and the browser found **no tearing, flicker or lockups** — the D47
class of bug did not resurface. **Still unverified**: the launcher's
scrolling path, which needs a third app to exercise.

**Two things noted for issue #24 (D53)**: `config::kOverclockHz`
(200 MHz) is **defined and never applied** — no `set_sys_clock*` call
exists anywhere — so the board has run at 125 MHz for every
measurement, and clock rate cannot explain the faults seen so far. But
the PSRAM PIO uses **clkdiv 1.0**, pinned to `sys_clk` with no
compensation, so anyone enabling that constant must raise the divider
in the same change. Deliberately *varying* `sys_clk` would also be a
sharper probe than the address hypothesis.

**Previous session:** 2026-08-14 — **Phase 6 spec-completion continued,
docs-only except one small hardware-verified diagnostic.** Six new
decisions, **D61-D66**, all in `docs/notes/decisions.md` and
cross-referenced in `docs/phases/phase6-spec.md`. **§0's pre-flight
checklist is now fully clear** — every item open at the start of this
session is resolved, nothing blocks starting 6A. Highlights: the Pico 1
MicroPython heap is **pre-committed to 40 KB** (was 48 KB), ahead of 6A
landing, after a fresh `size-report.sh` measurement found only 2.2 KB
of margin above threshold with zero 6A code written yet (**D61**);
P6-13 (editing vendored `pwm_sound.h`/`.c`) and P6-12 (sensor catalog —
DHT11/DS18B20 get dedicated C++ bindings, LM393 boards need only a new
`calc.adc_read`) both resolved (**D62**, **D63**); build order changed
to **Notepad (6C) before MicroPython (6B)**, proving the shared
`TextEditorWidget` on a real app first, per direct user instruction
(**D64**); **P6-14 hardware-confirmed on the connected Pico 1** — a
real power-cycle reads `watchdog_caused_reboot()=false`, a non-power
reboot reads `=1` — via a small new permanent diagnostic in
`src/main.cpp` (**D65**); and §3.4's compiled-app launcher goes
**self-sufficient, not `uf2loader`-dependent**, per direct user
instruction, which surfaced two real corrections to §3.4 (a dedicated
reset-reason marker is needed, not bare `watchdog_caused_reboot()`;
the bootstrap must be a separate permanent firmware component, not
logic inside `main()`) — **D66**. Full narrative in
[worklog.md](worklog.md).

**This landed on a new `phase-6` branch, not `main`** (explicit user
request this session) — the two previous Phase 6 docs sessions
committed straight to `main`; this one deliberately didn't. Merge or
rebase onto `main` when picking this back up, if it hasn't happened
already.

> ## ~~The next job is 6A.1~~ — **DONE 2026-08-15.** 6A and 6C.1
> are code-complete and hardware-verified; see the top of this
> file. The D64 build order was followed as written. ~~6B is next.~~
> **6B's first half also landed the same day** — see the top.

**Previous session:** 2026-08-13 — **Phase 6 spec-completion brainstorm,
docs-only.** No firmware changed — everything landed in
`docs/phases/phase6-spec.md` and `docs/notes/decisions.md` (**D54-D60**).
The editor (§4.3) generalized into a shared 6A `TextEditorWidget` (§3.5)
with Notepad promoted to a committed first 6C app (§3.6, D54); the
diagnostic `FilesScreen` generalized into a `FileBrowserScreen` with
browse/pick modes plus delete/rename/new-folder management (§3.7, D55) —
**6A/6B/6C's committed total moved 87 → 100 hrs** (6A alone: 14 → 31).
Two new **unscoped** candidate sub-phases were opened: **6.1** (home-screen
convenience-script replay via Phase 5.1's `submit_line()`, §9.3) and
**6.2** (a PCM-sampler audio engine, §9.4, spun off the sound-demo
pressure test after research turned up real prior art). Launcher UX
(P6-3/P6-4, D58) and the compiled-app "return to calculator" mechanism
(P6-6, D59) were both resolved. **Later the same day, [issue #27](https://github.com/moodoki/graphite_picocalc_gc/issues/27)
was re-verified and closed** — source-reading only, no hardware needed.
New **§4.7** records the real `calc.eval()` pipeline
(`math::cas::evaluate_home` → `math::solveexpr::contains_solve`/
`substitute` → `math::unified::evaluate_home`, not raw `compile()`/`run()`)
plus two binding requirements (eager copy of list/matrix results, an
explicit reentrancy guard) as **D60**; 6B.3's task row now points at it.
6B is unblocked — no remaining open issue gates its scoping. Full
narrative in [worklog.md](worklog.md).

**Previous session:** 2026-08-10 (later) — **wiki navigation links fixed.**
Docs-tooling only, no firmware changed. `scripts/gen-wiki.sh` was linking to
`Page-Name.md` instead of `Page-Name` in both in-page cross-links and
`_Sidebar.md`; GitHub wikis (Gollum) serve extension-less URLs, so every wiki
nav link resolved to a raw markdown blob instead of the rendered page — the
reported symptom. Fixed in the generator (`docs-site/README.md`'s flattening-
rule doc corrected to match) and verified locally against a regenerated
`build/wiki/`. Full narrative in [worklog.md](worklog.md).

> ## Open follow-up: confirm the published wiki actually got the fix
>
> This session's change only touched the generator (`scripts/gen-wiki.sh`,
> `docs-site/README.md`) — no file under `docs-site/**` content changed, so
> nothing forces a republish by content diff. The `publish-wiki` CI job has
> **no path filter** on its `push: branches: [main]` trigger and rebuilds
> `build/wiki/` from scratch on every run, so this commit's push to `main`
> should still fire it and overwrite the previously-published (broken-link)
> pages. There is no local way to verify a `WIKI_TOKEN`-gated push —
> **check a live wiki page in the browser** once this merges (e.g. the Home
> page's link to "Build and flash") to confirm the target no longer ends in
> `.md`.

**Previous session:** 2026-08-10 (earlier) — **the repo went public.** No
firmware changed. Privacy/security pass, history rewritten to drop 204 AI
co-author trailers and 116 session URLs, README split into four root docs,
the backlog moved to GitHub Issues, the user guide written, and a landing
page + wiki published and auto-deploying. Full narrative in
[worklog.md](worklog.md).

> ## Read this before touching user-facing docs
>
> **A derived document is not a source.** One commit on 2026-07-18 changed the
> F-key layout and made identifiers case-sensitive. The README was never
> updated; `USAGE.md` was then written *from the README* and inherited both
> errors, which shipped publicly. Four files carried a wrong key map, and
> variables were documented as `A-Z` when they are lowercase `a-z`.
>
> The generated pages under `docs-site/reference/` come from
> `src/apps/help_screen.cpp` and `src/math/catalog.cpp` and are drift-checked in
> CI. **Write user docs against those, or against source — never against other
> prose.** Related: the on-device help lists only a subset of the typed commands
> the home screen accepts.

> ## Session-start habit
>
> `./scripts/gh-issues.py` mirrors the open backlog into
> `docs/notes/issues.local.md` (gitignored). Open work is **GitHub Issues** now;
> `wishlist.md` is an archive. What lives where is settled in
> [issue-tracking.md](issue-tracking.md).

### Open, and now tracked as issues

| | |
|---|---|
| [#24](https://github.com/moodoki/graphite_picocalc_gc/issues/24) | **D53 root cause** — start from the address hypothesis, not concurrency. 40,000 instrumented reads came back clean. |
| [#26](https://github.com/moodoki/graphite_picocalc_gc/issues/26) | matrix nesting's unattributed ~104 B/level |
| [#33](https://github.com/moodoki/graphite_picocalc_gc/issues/33) | host-side renderer for docs screenshots — `font.cpp` already compiles on the host |
| [#15](https://github.com/moodoki/graphite_picocalc_gc/issues/15) / [#16](https://github.com/moodoki/graphite_picocalc_gc/issues/16) | tinyexpr: re-vendor **or** replace — alternatives, decide one and close the other |

**Next phase is 6** (app framework + MicroPython) — fully specced as of
2026-08-14, still not started. **§0's pre-flight checklist is fully
clear** (D61-D66) — the literal next task is **6A.1**, in the D64 build
order (see the blockquote above), not `phase6-spec.md` §0 itself
anymore. Committed total is **~100 hrs** (6A 31 + 6B 66 + 6C 3). Two new
unscoped candidates also exist: **6.1** (home-screen script replay,
§9.3) and **6.2** (PCM sampler audio engine, §9.4) — neither is
committed work, just recorded so it isn't lost. A third open question,
**P6-15** (§4.6 entry 6/§8 — `calc.plot()`'s Y-slot clearing semantics),
needs deciding before 6B.6, not before 6A starts.

**Manual, if the wiki ever stops updating:** the `WIKI_TOKEN` secret carries an
expiry by design (GitHub has no wiki-only permission, so it is a full repo-write
credential). When it lapses the `Publish wiki` job goes red on `main` and the
wiki simply stops updating — nothing is lost. Regenerate and update the secret.

---

**Previous session:** 2026-08-09 (last) — **Phase 5.2 task 5.2.12: on-device
verification, both boards. §9's measurement method did not survive contact with
the hardware, and the Pico 1 found two bugs** (D52, D53). 5.2.12's own work is
**done**; two defects it surfaced are **not**, and one of them should be fixed
before Phase 6 starts.

**The numbers** (evaluation-only firmware probe, median of 15, per-sample spread
0.02-0.18 ms, Pico 2 then Pico 1): **M1 -29%/-32%** — the guardrail row is
*better*, not merely unchanged, because REAL mode no longer evaluates twice;
**M3 -17%/-9%**, **M4 -32%/-14%**, **M5 +0.15/+0.17 ms** dispatch;
**M2 +52%/+36%** and **M6 +54%/+80%**, the two regressions. M2 is §3's promised
number and it is the predicted cost (5.7 us/element = two extra streaming passes
at D10's ~6.8 MB/s). Depth: **paren 16 -> 62+, matrix 3 -> 14+**, worst stack
peak **3,972 -> 2,344** (Pico 2), `kMaxStack = 64` exact with 65 a clean error.
`.bss` **-5,360 B** (Pico 1) — which does **not** match the phase's claimed
-6,888 and is recorded as measured, not reconciled.

> ## The two open defects
>
> **1. `matexpr`'s depth cap does not hold on the Pico 1 — v0.3.2 hard-faults.**
> `det((([A]*[A])+[A])*[A])` reboots the shipped firmware. `DepthGuard` is RAII
> *inside* `parse_unary`, so depth 4 allocates its frame before the guard can
> refuse it, and the Pico 1 has 144 B of margin against a ~600 B frame. **Phase
> 5.2 fixes this by deleting `matexpr`** (returns `8` at a 2,104 peak) — so if
> 5.2 merges, nothing more is owed. **If 5.2 slips, `main` needs a point fix**,
> because every release from v0.2.0 to v0.3.2 carries a reachable hard fault on
> that board. See D48's amendment.
>
> **2. Per-element PSRAM reads are intermittently wrong (D53) — symptom fixed,
> DEFECT STILL OPEN.** A long list displayed one element wrong on ~8 runs in 30,
> Pico 1 only. Pre-existing — identical 8/30 on v0.3.2 and on 5.2 — so **not a
> phase regression**; 5.2 only widens which expressions reach it.
>
> **Fixed in the display path only**: `format_list` now block-reads through
> `read_range` instead of per-element `get()`. Verified **0 in ~144 runs**.
>
> **Partially resolved — do not read it as closed.** `format_matrix_impl` is
> still per-element (`array_format.cpp:58/68/75`), ~17 other `get()` call sites
> are untouched, and the root cause is unknown.
>
> **Investigated further 2026-08-09 and it will not reproduce.** The fix is
> **confirmed causal** — reverting `format_list` to per-element reads brings the
> fault straight back (7/30, 2/30, same corrupt value). Three of four candidate
> locations are eliminated by sensitive folds: the **write path**, the failing
> expression's **stored data**, and **bulk read** are all clean, 0/30. But a
> temporary firmware diagnostic ran **40,000+ per-element reads with zero
> failures** across six variants, including after heavy streaming — against ~2%
> in the real path. **A ~1000x rate discrepancy means the trigger is contextual**,
> and it argues *against* the DMA-contention hypothesis. **Start from the address
> hypothesis**: the only difference left is that the diagnostic used a local
> `math::Array` while the real case formats an evaluator temporary from the pool,
> a different PSRAM region. Log the failing temporary's `psram_addr_` first.
>
> Also: the signature recorded earlier was **wrong** — "partial transfer, low 4
> bytes unwritten" came from comparing the *derived numerator*, not the value
> read. It is a **single-bit flip at mantissa bit ~32**. And 10-significant-figure
> display output cannot recover low mantissa bytes at all.
>
> **Earlier 2026-08-09**: matrix display was probed and **does not show the defect** —
> a PSRAM-backed 200x2 matrix with varied values, plus single-element
> `[G](3,1)` reads, came back 1-distinct-in-30 on all four, at sensitivity
> comparable to the test that caught lists at 8/30. So the exposure may be
> **narrower than "per-element PSRAM reads are unreliable"**: something
> distinguishes a freshly-written list temporary from settled matrix storage.
> That is a sharper and cheaper question than the concurrency hypothesis — try it
> first. It also means the remaining call sites may not be at risk, so this is
> lower-severity than it first looked, on 30 samples of one shape.
>
> The split is by *access pattern*:
> `read_range` (bulk) is clean, `Array::get` (one 8-byte PSRAM transfer) is not,
> and nobody knows why. Note what argues against the obvious DMA-contention
> guess: a 2 KB `read_range` performs ~67 chunked transfers to `get()`'s one, yet
> only `get()` corrupts — transfer count is not the variable. **~20 other
> `get()` call sites** and `format_matrix_impl` still carry the exposure; they
> have just never been run 30 times in a row. D53 names the concurrency test that
> would actually settle it.
>
> **Method note worth more than the bug**: two "sensitive" tests cleared this
> wrongly before the third caught it, both by summing. `sum(l1*1)` and
> `sum(l1/499500)` both hide a single-element fault in the low digits of a large
> total — the second *worse* than the first, because dividing shrank the error to
> 3.6e-12. `sum(l1)-499500` works. **A sum is the wrong instrument for a
> single-element fault.**

**Also corrected: nothing moved to PSRAM.** D48 said the explicit stack would be
"PSRAM-friendly" and that this is "what makes much larger depth reachable at
all". 5.2 put the operand stack in **bss** (1,536 B) and never needed PSRAM.

**The measurements are documented at
[`docs/notes/measurements/phase5.2/`](measurements/phase5.2/README.md)** — method,
results, caveats, and the raw per-sample JSON for both boards and both builds.
**Phase 5.2's closure should cite it** for every performance claim. New tool:
**`scripts/ab-measure.py`**, plus a firmware `eval_us=` probe on the inject echo. Full detail: `decisions.md` **D52** (results + why the method
changed), **D53**, D48's amendment, and `phase5.2-spec.md` §9's amendment.

**Previous session:** 2026-08-09 (later still) — **tinyexpr's unary-minus/`^` bug
fixed at the source (D51), shipped as v0.3.2 on `main`, HW-verified on the
Pico 2, and merged back into this branch.** Not 5.2 work: D50 had split it out,
and it landed on the shipping baseline so it is in the firmware whether or not
5.2 closes. `(-2)^2` was **-4** on every path `evaluate_real()` serves — home
screen in REAL mode, graphing, tables, stats, solver — and patching the vendored
parser turned up a **second** defect nothing had recorded: `2^-3^2` returned
**512**, because the right-associative insertion loop re-based a negated
exponent. Neither was pinned, because `(-2)^3` = -8 and `(0-2)^2` = 4 are right
*by accident*. Verified by flashing the pre-fix and post-fix builds to the same
Pico 2 and replaying one corpus through Phase 5.1's serial injection — 14 rows
flip, the must-not-move half is byte-identical. **`mode real` first, or the pass
is meaningless**: in `a+bi` the answer comes from `complexexpr`, which was
already right. text **-104 B**, `.bss` flat; `test_math` 242 → 272,
`test_graph` 72 → 74.

**For 5.2 this closes soak row #2 and removes a caveat rather than adding one**
— `Y1=(-2)^X` now plots 4 at X=2, so the home-vs-graph inconsistency the phase
was going to ship never existed. Full detail: `decisions.md` **D51** (and D50's
same-day amendment), worklog's **2026-08-09 (later still)** entry.

**Previous session:** 2026-08-09 (later) — **Phase 5.2 tasks 5.2.6-5.2.11: the
unified evaluator now IS the home screen, and the three it replaces are
deleted.** 3,903 lines gone, four parsers down to two, three of four depth caps
retired with the parsers that needed them. `det(([A]*([A]+[A]))+[A])` — the
expression that hard-faulted the Pico 1 last session — evaluates. Net **-6,888 B
of bss, +1,500 B of text**. Suites at 1,930 checks green, both boards build.

**Nothing here is hardware-verified.** That is 5.2.12, and it is the only task
left in the phase.

Full detail: worklog's **2026-08-09 (later)** entry, the byproduct register
[unified-evaluator-changes.md](unified-evaluator-changes.md), and
[phase5.2-spec.md](../phases/phase5.2-spec.md) §9 for the measurement plan.

---

## Decisions left to soak (2026-08-09)

Parked deliberately before hardware verification, so they can be changed while
changing them is still cheap. Each says where it lives and what reversing costs.
**Everything below is behaviour a user could notice** — the structural choices
(shim, rehomed formatters, `Mode` not defaulted) are not listed because
reversing them changes nothing observable.

| # | Decision | Where | Cost to change now |
|---|---|---|---|
| 1 | **`1/0` shows `Inf`, not "Undefined result"** — `matexpr`'s gate was dropped rather than generalised. The 5.2.10 sign-off nearly went the other way. TI raises `ERR:DIVIDE BY 0`. | register W14, §2.5 | Small — one check in `run()`. But it would change the *scalar* path, which every user hits. |
| 2 | ~~**`(-2)^2` will read 4 on the home screen and still plot as -4**~~ — **CLOSED 2026-08-09 (D51), shipped as v0.3.2 and HW-verified.** tinyexpr was patched, so it reads 4 *and* plots 4. The estimate was wrong in a useful direction: "~5 lines" became a `factor()` rewrite, because the source also held `2^-3^2` = 512, which no register row covers. | register F1, D50 amendment, D51 | **Nothing to change** — it was taken, not deferred. |
| 3 | **`mat2list` may not compose or be stored** — `matexpr`'s rule, restored after 5.2.7 briefly dropped it, because it writes lists the operand stack may still hold by reference. | register P5, spec §6.2 | Small — delete `check_statement_forms()`. But then `l1 * mat2list([A], l1)` has an order-dependent answer. |
| 4 | **A bare `sort_asc(l1)` echoes no store target** (matches `listexpr`); an explicit `-> l5` echoes one. | `StoreKind::kListInPlace` | Trivial — one flag. |
| 5 | **`1->a->b` is "Bad store target"**, not a syntax error from inside tinyexpr; the *first* arrow ends the expression, where the old parsers took the rightmost. | register G1/G3 | Small, but the old behaviour was an accident rather than a choice. |
| 6 | **Error-text divergences**: `{1,foo}` → "Syntax error" (was "Bad list element"); `fac(a)` with complex `a` → "Non-real result" (was "Non-real variable"). | register E8, E9 | E9 trivial. E8 needs list-context tracking in the compiler — the only one that is not cheap. |
| 7 | **Replacing tinyexpr on the numeric path is deferred past 5.2 closure**, with the measured costs written down and §9's M1 named as the missing input. | D50, spec P5.2-7 | Nothing to change now — it is a deferral, and the numbers are recorded either way. |

Two more that are decisions but not really reversible: the **differential
harness retired** with the evaluators it compared (recoverable from git if 5.2.12
wants it back), and the **~770 old checks were ported rather than deleted**,
which is what caught the complex-list read gate.

---

~~**Next up: 5.2.12, on-device verification**~~ — **DONE 2026-08-09, both
boards** (D52). All four items below were delivered except the last, which has no
mechanism: the diag screen reports PSRAM, die temp, SD and keys but **no
static-RAM figure**, so the ELF number is the only one available — and it is
definitionally what was flashed. Item 2's method was replaced outright (see
D52). Kept below as the record of what the task was scoped to owe.

**5.2 is now code-complete and hardware-verified. What remains before it closes
is a judgement call, not a task**: M2 (+52%/+36%) and M6 (+54%/+80%) are recorded
regressions, and §9's own criteria call those "a finding to record and cost, not
automatically a blocker". Decide whether to accept them, then do the phase-close
docs pass and open the PR.

**Both regressions were traced to mechanism** (follow-up runs, data in
[`measurements/phase5.2/`](measurements/phase5.2/README.md)), and the summary
sharpened along the way — it is **not** "slower on lists and matrices":

- **M2 is operation count, not lists.** One operation is faster whatever the
  source count (`l1+l2` builds a 999-element list and is **-13%**); the crossover
  is between one operation and two. 5.2's passes are additive where `listexpr`
  fused them. Target, if attacked: **pass fusion**.
- **M6 is per-element interpretation**, flat from 100 to 999 elements: ~4.6
  us/element of fixed `run_body` re-entry, ~1.7x slower per operation than
  tinyexpr's tree walk, and ~1.6 us/element of per-element PSRAM write past 256
  (the only cheap fix — stage `set()` and flush with `write_range`, as
  `format_list` now does).
- **Matrices are not a regression category.** `det`'s overhead is +0.19 ms at
  10x10, 20x20 and 30x30 alike — a fixed `Program` compile cost, constant across
  27x the work. M5's +15-30% is a small-matrix artifact.
- **Flash is the third regression**, and it was missing from the first write-up:
  text **+1,960 B (Pico 1) / +3,320 B (Pico 2)** in shipping configuration,
  against a projected +1,500. `.bss` is **-5,092 B** on both boards, against a
  projected -6,888. Both miss the projection, in opposite directions, and the
  baseline choice explains neither — recorded as measured, not reconciled.
- **One thing documented but NOT attributed**: matrix nesting costs ~104 B/level
  of call stack, where scalar/paren/unary nesting costs nothing. Input length and
  the CAS probe were ruled out; the mechanism is still unknown.

**And M6 answered a question that was not being asked of it.** The baseline it
beats, `listops::seq`, **compiles once and evaluates many** — the graphing shape.
That is the per-sample number D50/P5.2-7 said was missing before deciding whether
to replace tinyexpr on the numeric path, and it **argues against replacing**:
one-shot entry is 22-32% faster under the unified evaluator, repeated evaluation
~1.7x slower per operation. The measurement supports the split that already
exists. See D50's amendment.

The original brief, for reference:

1. **Stack peak** per input — the whole point of moving depth off the call
   stack. The comparison is last session's `matexpr` figures: 4,012 of 4,096 on
   the Pico 1 at depth 3, 3,860 on the Pico 2 after the leaf fix.
2. **The A/B latency pass, §9's M1-M7.** Baseline is now the **v0.3.2 release
   `.uf2`**, not the v0.3.1 the spec names — v0.3.2 is what this branch actually
   forks from, and it is the same three evaluators plus D51's parse-time fix, so
   it isolates the evaluator change more cleanly than v0.3.1 does. Either works
   (the fix cannot move a per-sample number), but prefer the one that differs by
   exactly the thing under test. No rebuild needed, and the injection block is
   still byte-identical, so one script parses both. Timing is host-side
   round-trip (the released binaries predate any firmware elapsed field).
   **M5 is the control**: it should not move, and if it does the other rows are
   not measuring the evaluator.
3. **The register replay** — its `Input` column is the script; the observed diff
   must equal the table.
4. `.bss` delta confirmed on the board, not only from `size`.

**Previous session:** 2026-08-08 (later) — **Post-D47 group-5/6 bench sweep on the
Pico 1, one crash found and capped (D48), and idea F promoted to worth-doing.**
Four of five heavy paths were clean under live stack guards: idle 1,540 of
4,096, graph redraw + zoom 2,360, and **the list/1-Var-stats/inference set never
registered a new mark at all** — first hardware confirmation of D47's
`eval_list_into` rework. The D45 ladder hit 3,588, which is *by design*: D45
predicted 3,728/368 by inspection and the live mark agreed to ~140 B. Rung 4
white in both number modes confirms D46's `c_pow` fix on hardware.
**`matexpr` was the exception** — `det(([a]*([c]+[d]))+[d])` hard-faulted
twice with `sp=0x20040ff8`, 8 bytes below core 0's `__StackBottom` and inside
core 1's stack; `pc`/`lr` were garbage because the overflow corrupted exception
stacking, so `sp` carried the whole diagnosis. It was the last uncapped parser.
**D48** adds `kMaxParseDepth = 3` (RAII `DepthGuard` in `parse_unary`).
**The cap took two tries and the first was instructive**: frame arithmetic said
depth 3 cost 4,300 B and was unreachable, so it was set to 2 — which broke
`det(identity(2))` and matrix literals in function arguments, both depth 3 and
both already pinned by `test_matrix`. Hardware said depth 3 actually fits at
3,940. The arithmetic was 360 B pessimistic; trusting it would have shipped a
level too tight. **Measured before -> after**: `det([[1,2][3,4]])` 3,940 ->
**4,012**, depth-4 crash -> "Too deeply nested". The +72 B is the guard itself
(cycle 808 -> 832 B/level), which the static tooling predicts exactly. **So the
margin at depth 3 is now 84 bytes — containment, not a fix.** `test_matrix`
381 -> **397**, full suite green, both boards build, lint/format clean, `.bss`
unchanged at 211,100. Flashed to the Pico 1 and all three checks verified.
**Decision taken**: live with the caps; the explicit-stack parser that would
actually lift the depth belongs to **idea F** (it retires `matexpr`, so building
it there is throwaway work), and F should be built on an explicit,
PSRAM-capable evaluation stack. PSRAM cannot host a call stack — PIO SPI, not
memory mapped.

**Then the Pico 2 was flashed and the cap turned out not to be enough.**
`det([[1,2][3,4]])` and `det(identity(2))` — both depth 3, both *allowed* —
hard-faulted there, while `det([a]*[c]+[d])` was fine. That board's reporter
gave a **real PC** where the Pico 1's had given garbage: `parse_power` prologue
(`mat_expr.cpp:625`) from `parse_unary`, `sp = __StackBottom + 160`. The
discriminator is a **numeric literal at maximum depth** — `parse_scalar_span`
put a `char span[256]` on the stack and handed it to `eval_field`, i.e. the
whole tinyexpr engine, at the *leaf* of the recursion. **D47's bug verbatim**:
`a0939bf` fixed exactly this in `complexexpr`, but `matexpr` has its own copy
and never got it. Fixed the same way (strtod fast path + static buffer):
**cycle 832 -> 600 B/level (Pico 1), 768 -> 536 (Pico 2), -232 both**, for
+256 B `.bss` (211,100 -> 211,356). Pico 2 re-verified: all five correct, no
fault, worst case **3,860 of 4,096 (236 margin)**. `test_matrix` 397 -> **408**.
**Two things only the Pico 2 could show**: it is *not* simply the roomier board
(it faulted where the Pico 1 survived, despite smaller reported frames and a
304 B lower baseline), and **`size-report.sh` does not count FP register saves**
— zero `vpush` on the Pico 1, 19 on the Pico 2 including in `math::eval_field`.
**Method note**: three attempts to derive a peak from frame sizes were wrong
this session, always optimistic (360 B, then a crash, then 560 B). Static frame
sums bound a single frame, not a peak — measure, and where a board is off the
bench prefer **monotonic arguments** ("this can only remove stack") over
predictions.

Full detail: worklog's 2026-08-08 (later) entry, `decisions.md` **D48** (with
its same-day amendment), `design-departures-matrix-complex.md` §F.

**Previous session:** 2026-08-08 — **Both 2026-08-05 testdrive items fixed and
HW-verified on the Pico 1, plus a separate 4-nested-paren crash the first
fixes did *not* address (D47). Five commits, `ad7ebd6`..`3153868`.** The
crash story is at the end of D47 and is the one worth reading: three wrong
attributions from reasoning about frame sizes, then a crash record in
`.uninitialized_data` that named it in one shot — `preprocess+0x12`, because
complexexpr ran *every numeric literal* through the whole tinyexpr engine at
the leaf of its recursion. Both items came from
`testdrive-2026-08-05-observations.md`. The Y= freeze was a **core-0 stack
overrun into core 1's stack**: `SlotEditorScreen::render()` called
`math::engine().compile()` *inside the renderer*, and that frame measured
**2,232 B** (almost all `te_variable lookup[122]`, rebuilt on the stack every
compile) against core 0's 4 KB. Strip mode renders 16-px bands and the header
is exactly 16 px, so strip 0 pushed, then core 0 rendered strip 1 **while core
1 was mid-DMA on strip 0** and killed it; core 0 blocked forever in
`wait_one_ack()`, taking key polling with it — "first few rows of the header,
then dead, every time." Fixed by caching field validity (`render()` only
draws, the contract `list_editor.hpp` has documented since Phase 3A) and
moving tinyexpr's binding table to bss (`Engine::compile` **2,232 → 280 B**,
firmware-wide). A **new stack-frame listing in `scripts/size-report.sh`** then
found a second, worse instance: `HomeScreen::evaluate_input` →
`listexpr::evaluate` → `eval_list_into` (**2,248 B and recursive**) = 4,312 B
at depth 1, so a plain `{1,2,3}` was already overrunning silently on a path
HW-verified since Phase 3A. Fixed with `noinline` leaf evaluators, buffers to
bss, depth-indexed per-level buffers, and a hard `kMaxRec = 3` cap in
`eval_list_into` itself (`eval_list_into` **2,248 → 32 B**). **`PICO_USE_STACK_GUARDS=1`
+ `PICO_STACK_SIZE=4096` are now on** — the top deferred item below — with a
new `src/platform/fault.cpp` that records the faulting PC and reboots, so the
next boot prints `fault: previous boot hard-faulted at pc=0x…` (without it the
guard just converts silent corruption into an identical-looking lockup).
ZTrig now follows the Angle mode (DEG: $\pm 360$, Xscl 90). Measured: Y= render
path **424 B**, worst list expression **3,152 of 4,096** (944 margin). Host
green, `test_math` 230→**235**, `test_lists` 239→**241**; both boards build
clean; lint/format clean. **Cost: Pico 1 `.bss` 198,836 → 209,120 (+10,284),
headroom 61.8 → 51.8 KB — that is out of the Phase 6 margin (see #0 below).**
**The bench pass then found a third instance, which is the useful part.** The
first flash booted clean but F1/F4/F5 *still* failed — as "black screen, then
back to home" instead of dead keys, i.e. the new fault handler doing its job.
Serial: `fault: previous boot hard-faulted at pc=0x100551da` → `factor+0xa` in
`tinyexpr.c`, the **prologue push** — unambiguous stack overflow. **D45 capped
the CAS parser's depth; tinyexpr's parser never had a cap**, and its recursion
costs **200 B/level**. Y1 was still holding one of the "up to 20 nested trig
calls" stress probes from `testdrive-2026-08-02`; the Y= path allows 16.
Added **`kMaxParseDepth = 8`** (sized to the tightest caller — the list-lift
path leaves ~1,696 B), so over-deep input is a parse error instead of a fault.
`test_math` 235 → **242**. **Re-flashed and verified: Y= opens and renders, Y1
draws red (correctly rejected), the graph works, three `graph recompute:` at
~103.0 ms with no fault.** Full detail: worklog's 2026-08-08 entry,
`decisions.md` **D47**.

## The next job

**Sequencing, settled 2026-08-08.** Phase 5 is **merged to `main`** (merge
commit, PR #2) and tagged **v0.2.0**. The two pieces of work that had been
floating as homeless bullets now have phase numbers and specs, and sit in this
order:

> ~~**Phase 5.1**~~ ([spec](../phases/phase5.1-spec.md)) — serial line
> injection, **DONE 2026-08-09**, HW-verified on the Pico 2
> → **Phase 5.2** ([spec](../phases/phase5.2-spec.md)) — unified evaluator
> → **Phase 6** ([spec](../phases/phase6-spec.md)) — apps

**Phase 5.1 shipped on branch `phase-5.1`** (not merged). Use it: bench checks
that used to need "type this, read me the peak" are now one command —

```bash
python3 scripts/serial-console.py 'sqrt(8)' 'det([[1,2][3,4]])'
python3 scripts/serial-console.py -f my-ladder.txt     # one expression per line
python3 scripts/serial-console.py --watch 300          # tail stack:/fault: only
```

It prints `= <result>  [plain|symbolic|error]`, so **`kind` answers the
amber-vs-white question that previously needed a human at the screen** — which
makes the Pico 2's outstanding rung-4 check scriptable. `picotool load -f -x
build/pico2/picocalc_graphcalc.uf2` reflashes the connected board with no
BOOTSEL button.

Both are *dotted* sub-phases: work that turned up rather than planned phase
goals, per the convention now recorded in `AGENTS.md`. 5.1 comes first because
its tooling is the main practical mitigation for 5.2's regression risk. Version
policy: each phase is a minor bump, so 5.1 → v0.3.0, 5.2 → v0.4.0.

0. **One open observation from 2026-08-08** —
   `testdrive-2026-08-08-observations.md`:
   - **`seq()` needs all five args, `range()` does not.** Not a defect (the
     test plan was wrong), but defaulting `step` to 1 would match `range` and
     TI-84. Small change in `eval_seq` plus a host test.
   - ~~`5!` / `abs(3+4i)` showing white~~ — **closed as not-a-bug.** The
     tester had read the two entries as one expression (`5! / abs(3+4i)`) and
     expected an improper-fraction exact form; they were separate entries,
     the displayed values (`120`, `5`) were correct, and plain white is right
     for real integers. No code change.

1. **Flash the Pico 2 — it is the whole of what's left on the bench pass.**
   Groups 1-4 passed 2026-08-08; **groups 5 and 6 passed on the Pico 1 later
   the same day** (see "Last session"), so the Pico 1 leg is done. Remaining:
   - ~~The Pico 2 has never been flashed on this branch~~ — **flashed and D48
     verified there** (all five det checks correct, no fault, worst case 3,860
     of 4,096). It took a **leaf fix** to get there: the cap alone still
     hard-faulted on this board, see "Last session".
   - **Groups 1-6 on the Pico 2 beyond the D48 checks.** It jumped straight
     from Phase 4D-era firmware, so typeset display, a+bi, guards and the full
     CAS script have never run there. Full framebuffer + hardware FPU, so
     **rung 4 of the D45 ladder (white vs amber) is the highest-value single
     item** — it is precision-sensitive and the one place the FPU could
     genuinely diverge from the Pico 1.
   - **`size-report.sh` misses FP register saves.** Zero `vpush` in the Pico 1
     image, **19 in the Pico 2's**, including inside `math::eval_field` on the
     crash path. Every Pico 2 frame figure is low by an unquantified amount —
     teach the tool to count `vpush` before making another stack decision
     there.
   - **Board swaps are batched to major stage closures** — don't swap to chase
     a number. `picotool load -f -x <uf2>` reflashes the *connected* board over
     USB with no BOOTSEL button, so re-flashing what's attached is cheap.
   - **Pico 1 re-measure, deferred to the next swap.** Its documented 4,012 is
     stale since the leaf fix. Not a safety issue: the fix only ever removes
     stack from that path, and the Pico 1 already passed at 4,012 without
     faulting, so it can only have improved.
   - ~~Guards-are-live sweep~~ — **done on the Pico 1.** Idle 1,540, graph
     redraw + zoom 2,360, list/stats/inference no new mark, D45 ladder 3,588
     (by design), `matexpr` crash found and capped (D48).
   - ~~Phase 5 CAS on-device checklist~~ — **sampled on the Pico 1** and
     correct as far as it went. Not exhaustively walked; the full script is
     still in the "Stage 4" bullet further down and is worth running on the
     Pico 2 in full.
   - **Watch for wrong answers, not just crashes.** Several buffers became
     `static` on a non-reentrancy argument verified by inspection, not
     exhaustively. A wrong result from something that mixes features
     (`solve(...)` or `convert(...)` inside a list or complex expression) is
     the signature of that assumption being wrong.
   - **Serial**: `python3 scripts/serial-capture.py 1800 | grep -E "stack:|fault:"`.
     A `fault:` line now names core, PC, LR and SP; resolve the PC with
     `arm-none-eabi-addr2line -f -C -e build/pico/picocalc_graphcalc.elf <pc>`.
     A PC on a function's `push` prologue means stack overflow.
   - **bss watch:** `.bss` is 211,100 on the Pico 1 (was 198,836 before this
     session). `size`'s total also carries 4,096 B that is **not** real —
     `PICO_STACK_SIZE` 2048->4096 doubles both `.stack_dummy` sections, which
     live in dedicated scratch banks. Compare `.bss` alone. The Phase 6 spare
     above the 48 KB MicroPython heap is thin; the `pre-phase5-review.md`
     levers are now likely rather than optional.

2. ~~`math::matexpr` is the last uncapped parser~~ — **capped 2026-08-08
   (D48).** All four parsers now have depth caps. But `matexpr`'s cap sits at
   **84 bytes of margin** at depth 3, which is containment rather than
   headroom, and the prediction that it "would reject ordinary matrix
   expressions" was half right: depth 3 turned out to fit on hardware, so
   `det(identity(2))` and matrix literals in function arguments survive —
   anything one level deeper does not. Three levers if that bites, cheapest
   first: (a) frame reduction, `parse_power` is 388-416 B holding matrix
   temporaries (cf. D47's `eval_list_into`, 2,248 -> 32 B), worth ~2-3x the
   depth; (b) move core 0's stack out of the 4 KB scratch bank into main SRAM
   via the linker script — raises the ceiling without touching frames,
   comfortable on the Pico 2, tight on the Pico 1; (c) an explicit-stack
   iterative parser. **(c) is deliberately assigned to idea F below, not to
   `matexpr`** — F retires this parser, so doing it here is throwaway work.

**Previous session:** 2026-08-05 — **Phase 5 Stage 5: CAS hardening (4D.22,
D45) plus two Phase 4C bugfixes (D46). PHASE 5 IS CLOSED, HW-verified on
both boards.** Stage 5's brief was "stress testing + edge cases"; the audit
found a live memory-corruption bug first. `simplify_sum` and
`simplify_product` each held four `kMaxOperands = 64` arrays on the stack —
**1,144 B and ~1,140 B frames**, measured on the linked Pico 1 object —
nesting through `simplify_rec` once per level of ADD-inside-POW-inside-ADD.
Core 0 has 2 KB declared and only **4 KB before core 1's stack**
(`__StackOneTop 0x20041000`), which runs the display service on both boards;
no stack guards. `exact_form()` runs parse + two simplify passes on *every*
all-integer home-screen input, so plain arithmetic reached it.
**Reproduced on the Pico 2 — and it did not crash**: the ladder
`(2+1)^2+1` → … out to rung 6 (~6.9 KB) returned the *correct* answer with
46 unbroken serial heartbeats, having overrun past core 1's stack top and
declared bottom into unused space. Silent corruption, not a fault, and
structurally invisible to the host suite (8 MB stack). Fixed by making the
`ExprPool` arena **two-ended** (nodes up, pass scratch down under LIFO
mark/release — scratch can't share the node end because `simplify()` runs up
to 50 fixed-point passes without resetting), adding **stated depth caps**
sized to the measurement (parser 12, simplifier 8; deepest recursing frame is
now `integrate_rec` at 172 B), **implementing Risk 2** (sticky `overflowed()`
+ `near_capacity()`, "Too complex" instead of `simplify()`'s "last good form"
masquerading as converged), and stopping `expand()` simplifying twice.
Largest recursive frame **1,144 B → 172 B**; Pico 1 bss **201,096 →
198,836**; `test_cas` **272 → 368**. The new `test_stress_edge_cases()`
immediately caught a defect in the first cut of the fix itself (`alloc_raw`
bounded against the arena end, not the scratch end). **D46** fixed two
Phase 4C defects found on the bench: DEGREE mode was silently ignored in
non-REAL Number modes (`c_sin` never applied `rad()`), and `c_pow` was
`exp(ln)`-inexact so `10202^2` rendered amber in a+bi mode but white in REAL;
`test_complex_expr` **75 → 113**. **The Pico 1 was flashed with Phase 5 for
the first time and passed**, including the legacy two-field `history.txt`
migration — only testable on that board, which closes the 2026-08-03 fix's
outstanding confirmation. `PICOCALC_PHASE` bumped to `"5"`. Full detail:
worklog's 2026-08-05 entry, `decisions.md` D45/D46.

## The next job

0. **Open the `phase-5` → `main` PR** if it is not already merged. The branch
   carries Stages 0-5 and is HW-verified on both boards; README, ti-parity §8
   and `PICOCALC_PHASE` are all flipped to reflect a closed phase.
1. **Three items deferred from Stage 5, recorded rather than fixed:**
   - **`PICO_USE_STACK_GUARDS=1` + `PICO_STACK_SIZE=4096` — DONE 2026-08-08
     (D47), soak still owed.** Landed alongside the Y=-lockup fix, since that
     bug was this exact class. A `src/platform/fault.cpp` handler was needed
     too: the SDK's default is an infinite loop, which would have made a
     trapped overrun look identical to the lockup being fixed. The soak this
     item asked for is the bench pass in "The next job" #0 at the top of this
     file. Note the warning below came true twice — the Y= editor *and* the
     home-screen list path were both overrunning silently.
   - **Latent MODE clobber, confirmed by code reading, never observed.**
     `main.cpp:432` re-runs `apps::load_graph_state()` when storage arrives
     late (the D14 5-8 s rail settle), and `graph_persist.cpp:56` is
     `*this = g_image.state` — a whole-struct overwrite including `.angle`.
     So a MODE toggle made *before* storage mounts is silently reverted (its
     own `save_graph_state()` having also failed). Repro needs a genuine cold
     power-on plus a toggle inside that window; a bench attempt on 2026-08-05
     could not catch it. Likely fix: have a failed `save_graph_state()` mark
     the in-memory state dirty, and have the late `load_graph_state()` skip
     the apply and re-save instead. Pre-existing (D14-era), not a Phase 5
     regression.
   - **Inverse-trig exact forms** (`asin(1)` → $\pi/2$, `atan(1)` →
     $\pi/4$): a missing feature, not a bug. D44 built a *forward*
     special-angle table only. Needs its own inverse table, angle-mode
     awareness and tests — comparable in size to D44. On
     [wishlist.md](wishlist.md).
2. **Seeded but unfinished: the `docs/site` branch** (2026-08-03, off `main`,
   commit `0f1e8ef`, pushed, no PR). Scaffold + generators + CI only — every
   prose chapter under `docs-site/guide/` is still a TODO stub and
   `docs-site/reference/error-messages.md` is unwritten. Now that Phase 5 has
   closed, the open question from that session resolves: **rebase onto `main`
   after the Phase 5 merge so the CAS chapter can be written.** Phase 5 is now
   merged, so this is unblocked.

   **Topic queued for that branch (D49, 2026-08-09): exact vs approximate
   results.** Users should be told plainly which results are *exact by
   construction* and which are *numerically approximated* — `(1+i)^2` is exactly
   `2i` because integer powers are computed by repeated multiplication, while
   `(1+i)^2.5` goes through `exp(ln)` and can carry a tiny error. The same
   distinction explains why some results render as amber exact forms and others
   as white decimals. D49 has the engineering detail; the user-facing version
   should be the honest short explanation, not the implementation.
3. **Phase 5.2 — F, the unified evaluator** (D37/D40/**D48**) —
   **now has a spec: [`phase5.2-spec.md`](../phases/phase5.2-spec.md)**, and a
   phase number as of 2026-08-08, so it is no longer a homeless bullet. Read the
   spec rather than this summary before starting. Sequenced **after Phase 5.1**,
   whose injection tooling is the main practical mitigation for 5.2's regression
   risk, and **before Phase 6**. Deliberately after CAS so a possible 4th
   symbolic evaluator was known before unification. **Judged worth the effort
   2026-08-08**, on two independent arguments rather than one:
   - *Correctness* (D46): the real and complex evaluators silently disagreed
     about DEGREE-mode trig since Session 18 — the class of bug unification
     removes.
   - *Structural* (D48): **four parsers, four separately-discovered stack
     budgets, three of them found by something crashing.** D45 capped the CAS
     parser, D47 capped tinyexpr and complexexpr, D48 capped `matexpr` after a
     reproducible hard fault. Each cap needed its own measurement pass against
     core 0's 4 KB, and `matexpr`'s landed at 84 bytes of margin.

   **Design constraint taken 2026-08-08: build F on an explicit evaluation
   stack, not the call stack.** Depth then lives in an array that can be sized
   freely and — being accessed sequentially — is genuinely PSRAM-friendly,
   unlike a call stack (`psram.hpp` is PIO-driven SPI and not memory mapped, so
   no stack can live there). This is what makes "much larger depth" reachable
   at all, and it is assigned here rather than to `matexpr` because F retires
   that parser. Note F stays home-screen-only per phase4-spec §5.2 —
   `evaluate_real()` (tinyexpr, graphing/tables/stats) is never touched, so
   this is four parsers -> two, not one. It is also the highest-risk item on
   the list by its own description: a rewrite of three working, tested
   evaluators against ~1,200 host checks that pin their separate behaviours.

   Then revisit idea H (polymorphic variables, D40 — unscheduled, only if real
   usage demands it).
4. **D10 leg B** (no phase home): compute-parallelize
   `GraphScreen::recompute_function` (`src/apps/graph_screen.cpp:313`) —
   needs a second engine/vars context (shared `X` mutation), not just a
   spawned task. The pipeline gives ~0 benefit on compute-bound screens; a
   heavy graph redraw measured 1.17 s on the Pico 1. See
   `testdrive-2026-08-02-observations.md` for a nesting-depth scaling anomaly
   worth another look if this is picked up.
5. **Pre-Phase-6 SRAM levers** (all still deferred, none urgent; Pico 1 bss
   is 198,836 after Stage 5, ~2.3 KB better than before): (a) MicroPython
   heap 48→40 KB if the ~12 KB spare gets eaten by 6A framework growth (spec
   Risk 6); (b) ArrayStore slab cut (~12-16 KB, more with a PSRAM-fallback
   prerequisite); (c) persistence `g_chunk` fold (~6 KB); (d) arena debug
   owner-guard. Full write-up: `docs/notes/pre-phase5-review.md`.

Mind the §8 strip-safety rule (idempotent `render()`) for any new screens
touched during on-device passes.

**Previous session:** 2026-08-03 — **Phase 5 Stage 4: exact-form (surd)
display, source changes, host-verified.** Home-screen results with a clean
closed form now show that form instead of a decimal — `sqrt(2)` → `√2`,
`sqrt(8)` → `2√2`, `1/sqrt(2)` → `√2/2`, `pi*2` → `2π`, `1/3` → `1/3`
(tasks 4D.23/4D.24, **D43**, which also resolves **P5-5 → always-on** and
**P5-6 → yes, `pi` included**). Recognition lives in a new
`src/math/cas/exact.cpp`, deliberately *not* in `simplify()` (which runs
inside integrate/solve/factor/derivative loops — a `POW(NUM,1/2)` rewrite
there is a §13 Risk 1 hazard for zero benefit); it works in
`POW(u,1/2)` space so the existing simplifier does the factor collection
free (`sqrt(2)*sqrt(2)`→2, `sqrt(2)+sqrt(8)`→`3√2`, `1/sqrt(2)` and
`sqrt(1/2)` share one rationalization path). The home-screen probe
mirrors D30: it runs *after* `engine().evaluate()` has committed
Ans/store and can only change the displayed string. **Five gates** bound
it — finite non-store result + no `>dec`; every literal in the parsed
input is an integer; no variables anywhere; a whitelist grammar
(rational coeffs + square-free `sqrt` + `pi`) that must be "interesting";
and agreement with the numeric result to 1e-9. Gate 2 is what makes
always-on safe (`2.5` stays `2.5`, not `5/2`; `0.1+0.2` stays `0.3`, not
`3/10`); gate 3 is not optional (the CAS parser has no `ans`/`e`, so
`ans` would parse as `a*n*s`). Layout builder gained a bare radicand
(`√2` not `√(2)`, except before `^`) and implicit multiplication before a
radical or symbol glyph (`2√2`, `2π`) — `is_call()` was relaxed to accept
the bare shape, with an explicit anti-regression test so `sqrt(2)/2`
still stacks as a fraction. **Behavior changes to judge on device**:
`1/3` now renders as an amber stacked fraction (was `0.3333333333`) and
`pi` renders as `π`; `>frac` results stay white flat text; no exact forms
for expressions naming a variable/`Ans`, or in non-REAL number modes (a
~6-line follow-up). Host suite green — `test_cas` **199 → 238**,
`test_layout` **44 → 54**, 0 failures, and the 199 pre-existing CAS
checks unchanged (the proof that staying out of `simplify.cpp` worked).
Both boards build clean; Pico 1 bss **201,096 bytes, exactly flat**;
lint/format clean. **Not flashed to either board yet** — folds into
Stage 5. Full detail: worklog's 2026-08-03 "Phase 5 Stage 4" entry,
`decisions.md` D43.

**Also 2026-08-03 (parallel, separate worktree):** a **documentation
branch `docs/site`** was seeded off `main` (commit `0f1e8ef`, pushed; no
PR). Plain-markdown source tree under `docs-site/` with `SUMMARY.md` as
the single nav source, driving three outputs: `scripts/gen-wiki.sh`
(flattened GitHub-wiki tree + `_Sidebar.md`), `scripts/gen-offline.sh`
(concatenated markdown always, plus self-contained HTML + PDF when
pandoc is present), and `scripts/gen-doc-reference.py` (generates the
function catalog from `src/math/catalog.cpp` and the key/syntax
references from `src/apps/help_screen.cpp` — firmware stays the source
of truth). CI: `validate-docs` now covers `docs-site/`, and a new
`.github/workflows/docs.yml` validates, fails on stale generated
reference pages, uploads the offline bundle, and has a wiki-publish job
gated on a `WIKI_TOKEN` secret (`GITHUB_TOKEN` cannot push to wikis —
the PAT setup is documented in `docs-site/README.md`). **Prose chapters
are stubs** — this was a scaffold-and-generators seed only.

**Two sessions ago:** 2026-08-03 — **Bugfix, source changes: home-screen
history persistence.** Root-caused and fixed the suspected home-screen I/O
persistence bug flagged at the end of the 2026-08-02 Stage 3 session:
symbolic CAS results were losing their `ResultKind` on reload (always came
back `kPlain` — plain white text instead of the typeset amber fraction),
because `history.txt` only stored `expr<TAB>result` and `load_state`
hardcoded `kPlain` for every reloaded line. Fixed by adding a third
tab-separated kind column (`expr<TAB>result<TAB>S|P\n`, backward
compatible with legacy two-field lines). While auditing the load/save path
also found and fixed two pre-existing latent bugs (predate Phase 5): a
head-vs-tail read bug (`load_state` read from file offset 0 despite its
own comment claiming "tail," so a `history.txt` past 8 KB restored the
*oldest* entries on reboot, not the newest — fixed with a new
`Storage::file_size()` + a seek to the true tail) and unbounded file
growth (no compaction ever existed — fixed with a new
`HomeScreen::compact_history()`, trims to the last 8 KB once the file
exceeds 24576 bytes). Both boards build clean; Pico 1 bss **201,096
bytes**, flat (shared `g_hist_io` buffer replaces the old function-local
static); `lint.sh`/`format.sh` clean; full host suite green (`test_cas`
199 unchanged — firmware-only path); a standalone host logic check of the
round-trip ran 600 checks, 0 failures. **D4 amended in place** (its own
"Revisit when" clause fired) rather than a new decision number. On-device
confirmation of history-survives-reboot is still open — folds into Stage
5's Pico 1/Pico 2 flashing. Full detail: worklog's 2026-08-03 entry,
`decisions.md` D4.

**Three sessions ago:** 2026-08-02 — **Phase 5 (CAS) Stages 0-3: engine +
home-screen UI integration, source changes, HW-verified on the Pico 2.**
On the `phase-5` branch (not yet merged to `main`). Two sessions: the CAS
engine itself — expr tree/pool, parser, serializer, simplify, differentiate,
expand, factor, solve, integrate (`src/math/cas/`, tasks 4D.1-4D.19,
D41: pool overlays the shared scratch `kCompute` arena, SRAM not the
spec's sketched PSRAM) — landed host-tested-only in an earlier session
this same day; this session wired it into the home screen (Stage 3,
4D.4/4D.20/4D.21): an inline-call router (`diff()`/`integ()`/`factor()`/
`expand()`/`simplify()`/`solve()`) dispatches from `HomeScreen::evaluate_input`
(CAS is display-only, no `Ans`/store, per P5-1/P5-2), results typeset via
`serialize` → `render::build_layout` in an accent color (**D42**: reuses
the existing string layout builder instead of a dedicated `expr_to_layout`
tree-walker), plus an F6 CAS menu and typed `cas` command
(`src/apps/cas_menu.{hpp,cpp}`). A round of on-device fixes followed:
exact `p/q` fraction display instead of decimal coefficients, right-aligned
symbolic results, amber accent (was teal — too close to the input-line
gray), descending-degree sum order (TI convention), and a pannable
one-line window for results too long to fit. `test_cas` grew 153 → **199**
checks, 0 failures; both boards build clean; Pico 1 static RAM **201,096
bytes** (~67 KB headroom, essentially flat — the CAS pool overlays the
existing arena); `lint.sh`/`format.sh` clean. Flashed to the Pico 2 and
confirmed working interactively (inline ops, F6 menu, fractions, sum
order, accent, scroll all reported "looks good") — **the Pico 1 leg for
this branch's CAS work is still open**, see "The next job" below.
Decisions **D41**, **D42**. `PICOCALC_PHASE` stays `"4D"` (bumping to `"5"`
is a Stage 5 close-out task, not yet reached). Full detail: worklog's
2026-08-02 "Phase 5 Stages 0-3" entry.

**Four sessions ago:** 2026-08-02 — **CI fix + first release, docs/infra only, no
source changes.** The GitHub Actions "Build" workflow had two red jobs (the
board build jobs themselves always passed): Lint disagreed with local
clang-format because CI installed Ubuntu's apt `clang-format 18` against
local's Homebrew `22` — fixed by pinning **`clang-format==22.1.8`** in
`requirements-dev.txt` and having CI `pip install` that exact version (no
source reformatting needed); Validate-docs failed on 6 loose `×` characters
in `docs/notes/pre-phase5-review.md` — replaced with ASCII `x`. Also bumped
all workflow actions to current majors (clears Node 20 deprecation
warnings) and added a `release` job that publishes both boards' UF2s to a
GitHub Release on `v*` tags. Landed via PR #1 (merge commit `e4b53ab`); CI
is now fully green on every job. **v0.1.0 published** — the project's first
tagged release:
<https://github.com/moodoki/graphite_picocalc_gc/releases/tag/v0.1.0>. No
decision number consumed, no phase/sub-phase status change (Phase 4D stays
closed, Phase 5 CAS is still next — see "The next job" below). Full detail:
worklog's 2026-08-02 "CI fix" entry.

**Five sessions ago:** 2026-08-02 — **Pre-Phase-5 review pass: shared scratch
arena (−21.8 KB SRAM) + near-zero matrix chop, HW-verified on the Pico 2.**
Opened the pre-Phase-5 code-review/size-optimization pass. A per-symbol SRAM
audit (new `scripts/size-report.sh`) found ~40 KB tied up in per-module
256-element PSRAM-streaming scratch buffers that are never simultaneously
live (single-threaded on core 0). Collapsed the verified-mutually-exclusive
ones onto one arena (`src/math/scratch.{hpp,cpp}`), two disjoint regions:
**kCompute** (list_expr | stats | infer | matops — none calls another) and
**kListops** (listops, disjoint because list_expr calls it); rebound by
reference-aliasing so call sites are unchanged (matops RowBufs via
placement-new). **Pico 1 bss 222,528 → 200,704 (−21,824 B; headroom ~46 →
~68 KB)**, Pico 2 same. During the device spot-check, `[A]^-1*[A]` showed FP
roundoff (2.22e-16) as scientific noise; added a relative near-zero chop to
`format_matrix` (cell >~12 orders below the max snaps to 0) — NOT an
arithmetic bug and NOT the arena (all matrix ops compute correctly
on-device). Also measured (no code change): **`-Os`** gives −126 KB flash but
0 SRAM (not a lever for this pass — keep `-O3`); **Phase 6 MicroPython budget
re-verified — the arena is what makes Phase 6 fit on Pico 1** (pre-arena 46.7
KB free < the 56 KB lazy heap; post-arena 68 KB → fits, ~12 KB spare). Host
1627 + test_matrix +6 (381) green; both boards clean; lint/format clean;
device-verified on Pico 2. **No decision number** (measurement/trim, not a
design call). Full detail: `docs/notes/pre-phase5-review.md`, worklog's
2026-08-02 "Pre-Phase-5 review pass" entry. Commits `1073f4f` (doc de-stale),
`5f76851` (arena), `4edba81` (chop).

**Six sessions ago:** 2026-08-02 — **UI-friction polish, source changes,
HW-verified on the Pico 2 (build `0cfbe05-dev`).** Fixed the two
UI-friction feature requests logged in the 2026-07-27 eval, plus two
follow-ups raised during this session's on-device testing. Matrix results
now format cells with the compact number formatter (`format_matrix`, 4 sig
figs; new `format_complex_compact` for complex cells) instead of full
10-digit precision. The constants picker was relaid out into four fixed
non-overlapping columns (symbol | engine id | short value | summary,
truncated with an ellipsis) to fix overprinting on long values like
`hbar`. Follow-up 1: `>Frac` now works on matrix results too (new
`math::matexpr::format_matrix_frac`, real cells become `p/q`). Follow-up
2: the constants-picker relayout needed LEFT/RIGHT description scroll
(`desc_scroll_`) to keep long summaries fully readable after truncation.
Host suite green (`test_math` 230, `test_matrix` 375, 0 failures across 12
suites); both boards build clean; Pico 1 bss **222,528 bytes** (was
222,520, +8 from `desc_scroll_`); `lint.sh`/`format.sh` both clean.
**No decision number consumed** — polish inside the already-closed Phase
4D, not a new design call. Full detail: worklog's 2026-08-02 "UI-friction
polish" entry.

**Seven sessions ago:** 2026-08-02 — **Phase 4D CLOSED, docs-only (D40).** Resolved
the three-item Phase 4D close checklist carried below: the **F-evaluator
follow-on check (D37) fired** — idea B (complex vars, 4D.15), C (complex
lists, 4D.24), D (complex matrices, 4D.25), E (vector ops), and G
(eigenvectors, 4D.23) have all shipped and are HW-verified within 4D — and F's
**sequencing is now decided (D40): after Phase 5 (CAS)**, not immediately
following 4D (order: pre-Phase-5 code-review/size-optimization pass → Phase 5
CAS → F). **Idea H (polymorphic variables) deferred again**, stays
unscheduled — TI's three namespaces (`A`-`Z` scalars, `[A]`-`[J]` matrices,
`l1`-`l6`/named lists) stay as-is; revisit only if real usage demands it,
re-checkpoint after F. `ti-parity.md` and `README.md` flipped to reflect
Phase 4 (4A-4D) as complete and hardware-verified rather than "code-complete,
evals pending" — see "The next job" below for the new forward path. No
source changes this session. Full detail: worklog's 2026-08-02 "Phase 4D
CLOSED" entry, `decisions.md` D40 (cross-refs D37,
`design-departures-matrix-complex.md` §H).

**Eight sessions ago:** 2026-08-02 — **D10 leg A, source change, HW-verified on
the Pico 2/RP2350 (`1a45763-dev`).** The dual-core display
pipeline — core-1-offloaded panel pushes — now covers the Pico 2's
full-framebuffer path, closing the "extend to Pico 2" half of the D10
follow-up item below. `start_display_service()` launches the core-1
service on both boards now (was Pico-1-only); the Pico 2's `render_frame`
hands its band push to core 1 asynchronously via the existing
`submit`/`drain_acks` machinery instead of blocking core 0 with a
synchronous `push_rect` (single `frame_buf`, so each frame's `drain_acks`
waits out the previous push before reusing it; a synchronous fallback
covers the pre-service boot window). This exercised the RP2350 XIP/USB
wedge risk the 2026-07-25 Pico 1 RAM-residency fix had never been tested
against on this chip — flashed clean, sustained boot with USB enumerated
throughout, no wedge/fault/drop; developer interactive pass (rapid nav,
fast typing, graph pan/zoom under key-repeat) came back clean. Both boards
build clean, full host suite green (multicore TU isn't in the host build).
D10 **leg B** (compute-parallelize `recompute_function`) is the one
remaining open D10 item — see "The next job" #2. Full detail: worklog's
2026-08-02 "D10 leg A" entry, `decisions.md` D10.

**Nine sessions ago:** 2026-08-02 — **feature follow-on, source changes,
HW-verified on the Pico 2 (build on top of `e5f2a10-dev`).** `MatAns` now
persists across a power cycle (**D39**): reverses the by-design-transient
stance the bugfix session below landed the same day. Save/load reuses the
`[A]..[J]` PCM2 file format via new path-based `save_matrix_file`/
`load_matrix_file` helpers (`matrices_persist.cpp`/`matrix.hpp`); MatAns
gets its own `/picocalc/matans.dat` written on every matrix-result commit
(`home_screen.cpp`) and restored at boot (`main.cpp`, same D14 late-init
retry contract as the named matrices). Host suite green (`test_matrix`
unchanged at 369 — no new host coverage, this path is firmware-only, same
as `MatrixStore`'s own persistence); both boards link clean, Pico 1 bss
unchanged at 222,520; cold-boot survival confirmed on the Pico 2. Full
detail: `worklog.md`'s 2026-08-02 "MatAns now persists" entry,
`decisions.md` D39.

**Ten sessions ago (same day):** 2026-08-02 — **bugfix session, source
changes, HW-verified on the Pico 2 (`e5f2a10-dev`).** Fixed the two minor bugs found in the
2026-07-27 eval: SEQ-mode trace (F4) now reads exact values straight from
`math::seqexpr::value()` instead of the pixel-quantized point cache (was
showing float noise instead of the table's exact integers) — covers both
TIME and WEB seq plot styles (the first cut only handled TIME; the test
board turned out to be in WEB style, which is sticky across reboots via
GraphState/PCG5 — test both styles on seq work going forward). And the
sequence editor no longer draws every recursive row red: added a stateless
`math::seqexpr::compiles()` (lag-rewrite + compile, no iterator side
effects) plus a `SlotEditorScreen::field_valid()` hook the seq editor
overrides, so `u(n-1)`-style self-references validate correctly instead of
failing the plain-engine compile check every other editor uses. Also
corrected three stale claims left in this file by earlier sessions: the
home-screen `MatAns` token and "fnInt shading follows curve color" were
each listed as open gaps but actually shipped as 4D.14/4D.11; `MatAns` not
surviving a power cycle (plus its Pico 2 discrepancy, see "Two sessions
ago" below) was called by-design at the time — `mat_ans()` was a transient
global (`g_mresult`, `mat_expr.cpp:27`) never written to SD. **That call
was reversed later the same day — see "Last session" above (D39): MatAns
now persists.** 12 new host checks (`test_seq` now 63); both boards
rebuilt clean; `clang-format` clean. Full detail: `worklog.md`'s
2026-08-02 bugfix entry.

The 2026-08-02 Pico 2 hardware session (informal perf spot-check — general
UI felt snappy, `graph recompute:` stress probes up to 33.7 ms stayed well
under the 146 ms push-budget floor; the MatAns power-cycle discrepancy it
found was root-caused and then superseded by D39 above), the 2026-07-27
on-device eval (Batches 2-4 PASS, closing all nine D38 batches on the
Pico 1), and the 2026-07-26 Phase 4D kickoff session are further back than
this rolling summary keeps — see `worklog.md`'s 2026-08-02, 2026-07-27, and
2026-07-26 entries (`testdrive-2026-08-02-observations.md`,
`testdrive-2026-07-27-observations.md`, `decisions.md` D38) for full
detail.

## The next job

0. **Seeded but unfinished: the `docs/site` branch** (2026-08-03, off
   `main`, commit `0f1e8ef`, pushed, no PR). Scaffold + generators + CI
   only — every prose chapter under `docs-site/guide/` is still a TODO
   stub, and `docs-site/reference/error-messages.md` is unwritten. Next
   steps whenever it's picked up: write the getting-started and guide
   prose (README's "Using the calculator" is the seed), create the wiki
   `WIKI_TOKEN` PAT if wiki publishing is actually wanted (see
   `docs-site/README.md`), and decide whether to rebase onto `main` after
   Phase 5 merges so the CAS chapter can be written. Independent of the
   Phase 5 work below — it does not block Stage 5.
1. **Phase 5 (CAS) is in progress on the `phase-5` branch — Stages 0-4
   code-complete; 0-3 HW-verified on the Pico 2 (2026-08-02), Stage 4
   host-verified only (2026-08-03).** The engine
   (tree/pool/parser/serializer/simplify/diff/expand/factor/solve/integrate,
   4D.1-4D.19) and the home-screen UI integration (inline CAS calls, F6
   menu, `cas` command, 4D.4/4D.20/4D.21) are both done and pushed; see
   "Last session" above and worklog's 2026-08-02 "Phase 5 Stages 0-3"
   entry. **One stage remains, per `phase5-spec.md` §11:**
   - **Stage 4 — exact-form display (4D.23/4D.24): DONE 2026-08-03
     (D43) + follow-ups the same day (D44). Flashed to the Pico 2, clean
     boot confirmed; interactive confirmation still pending.** Remaining
     on-device script, to run on both boards during Stage 5:
     - Amber typeset exact forms: `sqrt(2)`, `sqrt(8)`, `sqrt(12)`,
       `1/sqrt(2)`, `sqrt(1/2)`, `sqrt(2)+sqrt(8)`, `1/3`, `2/6`,
       `1/3+1/7`, `pi`, `pi/2`, `pi*2`, `1/pi`, `1+sqrt(2)`;
       trig `sin(pi/6)`, `sin(pi/3)`, `cos(pi/3)`, `tan(pi/6)`,
       `tan(pi/3)`, and `sin(pi)`/`cos(pi/2)` → a clean `0`.
     - DEGREE mode: `sin(30)`, `sin(45)`, `sin(60)`, `cos(30)`, `tan(60)`
       fold; `sin(37)` does not.
     - RECT/POLAR number mode: real-valued results still get exact forms;
       genuinely complex ones stay decimal.
     - Unchanged white decimals: `2.5`, `0.1+0.2`, `2+2`, `4/2`,
       `sqrt(4)`, `sin(1)`, `sin(pi/5)`, `tan(pi/2)`, `1/3>dec`,
       `5->a` then `a/3`. `1/3>frac` still works the old way.
     - **Alt+Enter**: on a typed expression → decimal; on an empty line
       after an amber result → re-runs it as a decimal. (Shift+Enter was
       the first binding and does *not* work — it arrives as `kInsert`;
       see "Last session" above.)
     - Reboot and confirm amber forms reload amber (also covers the
       2026-08-03 history fix).
     - **Judgement calls while it's in hand**: whether `1/3` as a stacked
       fraction and `pi` as `π` are welcome or intrusive. D43's "Revisit
       when" names the escape hatch (require a `sqrt`/`pi` flag rather
       than any flag, dropping bare rationals back to decimal).
   - **Stage 5 — hardening + on-device verification (4D.22), not started.**
     Stress/edge-case tests, a pool-capacity guard (abort above ~80%
     capacity, spec Risk 2), the Risk-1 termination cycle set exercised at
     scale (not just unit-test scale); then flash the Pico 2 again and
     **flash the Pico 1 for the first time on this branch** (watch bss
     headroom there specifically — the Pico 1 is the tighter budget).
     Once Stage 5 closes: bump `PICOCALC_PHASE` `"4D"` → `"5"` in
     `CMakeLists.txt`, do the phase-close docs pass (ti-parity.md gets its
     CAS-section sweep at this point, README status flip), and open the
     `phase-5` → `main` PR. **Fold in on-device confirmation of the
     2026-08-03 history-persistence fix** (below) while the Pico 1/Pico 2
     are on the bench for this stage anyway — it's a firmware-only path,
     not covered by the host suite.
   - **BUG flagged 2026-08-02, RESOLVED 2026-08-03**: the suspected
     home-screen history I/O persistence bug was root-caused (symbolic CAS
     results lost their `ResultKind` on reload) and fixed, along with two
     related latent bugs (head-vs-tail read, unbounded file growth) found
     during the investigation. See "Last session" above, worklog's
     2026-08-03 entry, and `decisions.md` D4 (amended in place). Still
     open: on-device confirmation that history now survives a reboot
     correctly (see the Stage 5 bullet above) — the fix is host-logic
     verified (600 checks) but this path itself isn't in host coverage.
   - The pre-Phase-5 SRAM levers noted before CAS started remain relevant
     background (all still deferred, none urgent — watch Pico 1 bss as
     Stage 4/5 lands): (a) MicroPython heap 48→40 KB if the ~12 KB spare
     gets eaten by CAS + 6A framework growth (spec Risk 6); (b) ArrayStore
     slab cut (~12-16 KB, more with a PSRAM-fallback prerequisite); (c)
     persistence `g_chunk` fold (~6 KB); (d) arena debug owner-guard. Full
     write-up: `docs/notes/pre-phase5-review.md`.
   - **After Phase 5 closes**: F (the unified evaluator, D37/D40 —
     deliberately sequenced after CAS so a possible 4th symbolic evaluator
     is known before unification), then revisit idea H (polymorphic
     variables, D40 — unscheduled, only if real usage demands it).
   - Phase 4D itself has been closed since 2026-08-02 (D40, all 9 D38
     batches HW-verified) — see worklog's 2026-08-02 "Phase 4D CLOSED"
     entry if the pre-CAS history is needed; `phase4-spec.md` §8 and
     `decisions.md` D37/D38/D40 have the full task/decision map.
2. **D10 follow-ups** (originally from 2026-07-25, no phase home):
   - **Extend the display pipeline to Pico 2 — DONE + HW-VERIFIED
     2026-08-02 (leg A).** `start_display_service()` now launches the
     core-1 service on both boards; the Pico 2 full-framebuffer push
     routes through core 1 asynchronously (`submit`/`drain_acks`) instead
     of blocking core 0. The RAM-residency fix's XIP/USB wedge risk was
     re-verified on the RP2350 (sustained boot, USB enumerated, no
     wedge/fault/drop) and a developer interactive pass (rapid nav, fast
     typing, graph pan/zoom under key-repeat) came back clean — no
     tearing, no corruption, no freeze. See `decisions.md` D10, worklog's
     2026-08-02 "D10 leg A" entry.
   - **Compute parallelization candidate (leg B), still open**: the pipeline gives ~0 benefit
     on compute-bound screens (render > ~146 ms push budget; a heavy graph
     redraw measured 1.17 s on the Pico 1). Those want
     `GraphScreen::recompute_function` (`src/apps/graph_screen.cpp:313`)
     parallelized — needs a second engine/vars context (shared `X`
     mutation), not just a spawned task. 2026-08-02 Pico 2 stress probes
     (up to 20 nested trig calls, 33.7 ms) didn't reach this regime either
     — see `testdrive-2026-08-02-observations.md` for a nesting-depth
     scaling anomaly worth another look if this is picked up.
3. **Pico 2 perf spot-check: done informally, 2026-08-02.** Reflashed to
   current HEAD; general UI felt snappy, and `graph recompute:` stress
   probes (up to 33.7 ms) stayed well under the 146 ms push-budget floor —
   no compute-bound stall observed. This was an interview-driven spot
   check, not a rigorous side-by-side comparison against the pre-Phase-3
   2.25 baseline — a systematic re-measurement remains optional/
   low-priority if ever wanted. Full detail:
   `testdrive-2026-08-02-observations.md`.

Mind the §8 strip-safety rule (idempotent `render()`) for any new
screens touched during the on-device passes.

## D14 rail settle — NEXT BENCH SESSION

Moved to **[next-bench-session.md](next-bench-session.md)** to keep this page
short. Non-blocking (last deferred HW item); schematic findings, probe points,
and the full bench plan live there. Pull it back here only when a bench session
is actually scheduled.

## Key things to note — Pico 2 specific

- **Both boards are flashed to `5ef025f`** (2026-08-05) — all of Phase 5
  (Stages 0-5) plus the D46 complex-evaluator fixes. **The Pico 1's flash is
  the first time any Phase 5 code has run on that board**, and it passed:
  the D45 nesting ladder computes clean, CAS ops are perceptibly slower but
  well inside budget (no FPU), and the D46 fixes hold. Pico 1 bss **198,836**
  (was 201,096); serial healthy on both (Pico 1 psram-bulk 232/200 us vs the
  Pico 2's 150/155, die temp ~25 C).
- **The 2026-08-03 history-persistence fix is now confirmed on hardware.**
  The Pico 1 was still on Phase 4D, so its `history.txt` was the legacy
  two-field format — that board was the only place the migration path could
  be exercised, and it migrated correctly (old lines reload plain, new
  symbolic results reload amber). This closes the last outstanding item from
  that session.
- **The history-persistence fix reached hardware with the build above**,
  but its reboot behavior has not been exercised yet — the host suite has
  no coverage for this path (it is firmware-only, with no host UI to run
  against real SD I/O). It is a persistence *format* change (`history.txt`
  gained a third tab-separated kind column) but backward compatible — no
  one-time reset, old two-field lines still parse as `kPlain`. Confirm on
  both boards alongside the Stage 5 flash, see "The next job" #1.
- **Firmware on the Pico 2 was reflashed on `phase-5` (2026-08-02) with
  Phase 5 Stages 0-3** — the CAS engine + home-screen UI integration
  (inline `diff()`/`integ()`/`factor()`/`expand()`/`simplify()`/`solve()`,
  F6 CAS menu, `cas` command). This is the first CAS build to reach either
  board. HW-verified: clean boot, all six inline ops, the F6 menu, exact
  fraction display, right-aligned/descending-order/amber-accent results,
  and the pannable long-result window all confirmed working interactively.
  No persistence format change, no one-time reset (CAS results are
  display-only, never written to SD). **The Pico 1 has NOT been flashed
  with any Phase 5 code yet** — that's part of Stage 5, see "The next job"
  #1. See "Last session" above for the full commit list.
- **Firmware on the Pico 2 was reflashed again same-day (2026-08-02) with
  this session's UI-friction polish** (`0cfbe05-dev`) — compact matrix
  cell formatting, matrix `>Frac`, and the constants-picker relayout +
  description scroll. HW-verified: clean sustained boots at every step,
  no faults; `>Frac` on matrices and the constants-picker scroll both
  confirmed working as intended. No persistence format change, no
  one-time reset. See "Last session" above.
- **Firmware on the Pico 2 was reflashed again same-day (2026-08-02) with
  the D10 leg A change** (`1a45763-dev`) — the display pipeline now
  offloads the Pico 2's full-frame push to core 1 (previously
  synchronous on core 0, see the bullet below which now describes a
  superseded state for that one item — "The next job" #2 and
  `decisions.md` D10 have the current picture). HW-verified: sustained
  boot with USB enumerated, no wedge/fault/drop, and a clean interactive
  pass (rapid nav, fast typing, graph pan/zoom under key-repeat).
- **Firmware on the Pico 2 was reflashed to current HEAD on 2026-08-02**
  (`dadc7cf`; was 9 builds behind, still Session 19's font/glyph build)
  — it now carries the same code as the Pico 1: the D35 perf fixes, the
  2026-07-25 work (`!` factorial fix, D10 display pipeline — core-0-sync
  path only, see "The next job" #2, die temp/build id), and all of Phase
  4D (Batches 1-9). First boot showed the expected one-time reset under
  the PCV1/PCL2/PCM2 format bumps. Its build still layers on top of
  Sessions 11/12/15/16/17/18 (3A lists, 3B stats, 3D inference/plots, 4A
  matrices/solver, 4B CALC menu, 4C complex numbers). **All of their
  hands-on on-device evals remain closed as a formality (2026-07-22)** —
  board-independent logic, and the harder rendering case (Pico 1) passed
  the identical checklists the same day (3D.14 for 11/12/15, the Phase
  4A-4C pass for 16/17/18); see `worklog.md`'s 2026-07-22 entries. The
  Pico 2 perf re-baseline ("The next job" #3) is now done informally as
  of 2026-08-02 — see the top of this file and
  `testdrive-2026-08-02-observations.md`. **Item from that session, now
  resolved**: `MatAns` persisted across a power cycle on the Pico 2,
  contradicting the Pico 1 finding (2026-07-27) on identical code — first
  root-caused as warm-reset RAM retention of a transient global (not a
  source bug), then later the same day the underlying by-design-transient
  stance itself was reversed: **MatAns now persists on both boards by
  design (D39)**, so the discrepancy question is moot going forward — see
  "The next job" #1. Session 15's storage-health row
  (hot-plug/retry-forever, Y=-editor truncation) is fully closed —
  confirmed on both boards. **Session 10 round 2 is also closed
  (2026-07-22)**: `L` toggle surviving a reboot, `rand()` showing a
  sensible varying value, ZTrig short tick labels (`1.571`-style), and `F`
  ZoomFit auto-fit all confirmed on the Pico 1. The HW-PENDING table is
  now clear except the still-informal Session 19 font sweep.
- **The Pico 1 now carries ALL of Phase 4D (Batches 1-9, 2026-07-26)**
  on top of the 2026-07-25 work and the D35 state. Flashed and
  boot-verified over serial after every batch (temp + psram-bulk
  heartbeats healthy throughout); final bss **222,520 bytes**, ~48 KB
  headroom — keep watching per the D28 watch item. One-time resets
  already absorbed on this board: PCG6 (Batch 3 flash). New SD files
  since: `listdir.dat`/`nlist<idx>.dat` (named lists, Batch 6) and
  `settings.dat` PCS1 (Batch 9 — created on first `settings` change).
  **Batch 9's APD defaults to 5 min**: an idle unit now dims its screen;
  any key wakes it (the wake key is swallowed). Phases 2-3 and 4A-4C
  are HW-verified on this board; **all nine 4D Batch 1-9 checklists are
  now cleared** (Batch 1 + 5-9 on 2026-07-26, Batches 2-4 on 2026-07-27
  — see worklog table). Three non-blocking findings from the 2026-07-27
  pass (SEQ trace snap, SEQ color swatch, `MatAns` not persisting) were
  all fixed and HW-verified 2026-08-02 — see "The next job" #1. All five font headers were regenerated with glyph slot 141 in
  Batch 7 — the non-default font builds (`build/pico2-jm|io|uni|term`)
  remain stale as before (the default `build/pico2` Terminus build is
  current as of the 2026-08-02 reflash, see above).
- **Persistence change 2026-07-26: `variables.dat` bumped to magic PCV1**
  (header + vars + imag parts). The old raw 224-byte file is ignored →
  **expected one-time variables reset on first boot** under this firmware
  (same precedent as PCL2/PCM2/PCG bumps), then persistence resumes.
  GraphState still **PCG5**; list/matrix formats still PCL2/PCM2 — but
  complex lists now write 16 B/elem payloads under the unchanged PCL2
  header (the dtype byte was always there); older firmware treats a
  complex list file as corrupt and skips it gracefully.
- **Flash-path notes 2026-07-26**: the BOOTSEL-volume `cp` failed with
  "Permission denied" this time (a new failure mode vs. the old xattr
  complaint) — `picotool load -f` remains the reliable path. Also
  **`picotool info` segfaults in picotool v2.3.0**; `load`/`reboot` work
  fine, just don't use `info` to check state.
- **List/matrix persistence changed shape this session (D35)**: the old
  single `lists.dat` / `matrices.dat` (magics PCL1 / PCM1) are replaced by
  one file per store — `/picocalc/list1.dat`..`list6.dat` (magic PCL2) and
  `/picocalc/matrix1.dat`..`matrix10.dat` (magic PCM2). Old images simply
  aren't read under the new paths (same "old files ignored" precedent as
  prior format bumps) — expect a one-time reset to empty lists/matrices on
  first boot under this firmware, already confirmed as expected. If a load
  ever misbehaves, deleting the relevant `listN.dat`/`matrixN.dat` resets
  just that one store.
- **Non-default font builds** (`build/pico2-jm|io|uni|term`) are stale
  relative to this session's non-font changes (eig alias, list scroll)
  — rebuild before re-comparing fonts. `build/pico2` (Terminus) is the
  canonical default and what's currently flashed.
- **D14 cold boot (~5-8 s rail settle):** PSRAM/SD may fail early init on
  a cold power-on; self-tests retry inside the 30 s late-init window and
  serial prints `late-init: ...` lines (including `lists loaded`).
  Large lists are simply absent until then; the editor shows "List
  memory unavailable" if a >256-element append beats PSRAM bring-up.
  Stats on a not-yet-loaded list just sees fewer/empty elements.
- **Flash path (revised Session 9, reconfirmed Session 12):** `stty -f
  /dev/cu.usbmodem* 1200` reboots to BOOTSEL, the **RP2350 volume
  mounted in ~5 s this time**, then
  `cp build/pico2/picocalc_graphcalc.uf2 /Volumes/RP2350/`
  (auto-reboots; cp exited 0 this session — the Session 11 xattr
  complaint didn't recur). Keep `picotool load` + `picotool reboot` as
  the fallback for when the volume doesn't mount at all.
- **Battery/charging: fully verified 2026-07-18.** Refresh cadence is 5 s
  by design — stability over snappiness; don't "optimize" it back down.
- **Boot printfs still race USB enumeration** — only prints after ~2 s
  (late-init, battery, recompute) are capturable. Don't chase "missing"
  early boot output.
- **STM32 caution unchanged (both boards):** never poll STM32 registers
  back-to-back; a wedge needs a physical power cycle. Fw is v1.6.

## Pico 1 pass: DONE (3D.14 + Phase 4A-4C, D18 resolved 2026-07-22)

The combined pass decided 2026-07-18 (D18) ran 2026-07-22 as task 3D.14: the
Pico 1 was reflashed to current HEAD (Session 19) and put through the full
Phase 2 sweep — headline **split-pane clipping on the strip renderer**, no
bleed — plus the Session 8+9 fix list and the Phase 3 acceptance checklist.
All passed; two non-blocking findings (factorial `!`, list-editor/scatter-plot
perf) were logged at the time. The perf finding was fixed later the same day
(D35 — see "Open design threads" and the fourth 2026-07-22 worklog entry); the
factorial bug remains open in "Backlog" below. Full detail: worklog 2026-07-22 entry,
`phase3-retro.md`, `session3D14-pico1-observations-verbatim.md`. **This
closes Phase 3.** Guardrail carried forward: Phase 3+ render code must stay
strip-safe (idempotent, may run ~20x/frame) — rule recorded in
`phase3-spec.md` §8; it held up cleanly this pass. Pico 1 bss was ~188.8 KB of
264 KB as of Session 19 (D28/D29/D30/D31 combined, essentially flat) — no
headroom pinch observed during 3D.14.

**Same session, second block**: Phase 4A-4C (matrices/solver, CALC menu,
complex numbers) also got their first-ever hands-on pass, on this same
Pico 1/build — all passed (details in "Last session" above and the worklog
Phase 4A-4C entry). This also **closed the Session 16/17/18 Pico 2
HW-PENDING rows as a formality** (board-independent logic, harder rendering
case already passed) — Pico 2 has no genuinely open board-specific gap left
except its own perf re-baseline (low priority, "The next job" #3). Map-file
re-check (the Pico 1 bss watch item, knob `ArrayStore::kSlabCount`) can now
be considered done for this generation of features — no headroom pinch
observed across either pass.

## Open design threads

> **Historical, as of 2026-08-10.** Most entries below are marked resolved or
> DONE inline; what remains is "judge on device" watch-items rather than
> actionable work, so they were deliberately *not* bulk-migrated to GitHub
> Issues — filing ten resolved threads would have made the backlog worse, not
> better. Two genuinely open items were filed:
> [#26](https://github.com/moodoki/graphite_picocalc_gc/issues/26) (matrix
> nesting's unattributed ~104 B/level) and
> [#27](https://github.com/moodoki/graphite_picocalc_gc/issues/27) (Phase 6B's
> `calc` bindings). **New open threads go to Issues, not here.**


- **List UX watch-items (Session 11, judge on device)**: F8 clear-list is
  immediate (no confirm); list history results truncate at ~40 chars
  (`,...`); `lists`/`stats` are typed-command-only entries (now with
  `list`/`stat` aliases, D24) — decide whether stats deserves an
  F-key/menu slot now that the screen exists. (Resolved by D24:
  reductions bare-arg limitation; mean/median/stdev promotion.)
- **Stats watch-items (Session 12, judge on device)**: results are
  plain text lines (no two-column layout for 2-Var's 17 lines).
  (Resolved by D24: "Computing..." indicator — verify its visibility
  on a 10000-element 1-Var.)
- **Session 13 caps to watch**: 4 lift operands per expression, 64
  elements per brace literal — revisit if real use pinches (D24).
- F3 MODE vs ZOOM (TI's F3 slot) — judge after real use (D20 KIV).
- D16 trace-sync option b (trace steps by table-step) — after more split
  use.
- **4B CALC watch-items (Session 17, judge on device)**: min/max "Guess?"
  step is UI-only, doesn't feed Brent's bracket — decide if that's fine or
  needs wiring through (D29). (Resolved by D29: P4-6 intersect = cursor-
  cycle; P4-8 polar fnInt = area only, no arc length — both from
  `phase4-spec.md` §11, tracked here and in `decisions.md` rather than
  editing the spec's open-questions table.)
- **4C watch-items (Session 18, judge on device)**: whether "Non-real
  result" is clear phrasing for REAL-mode domain errors; no
  complex-valued variable storage (`2i->a` errors) — decide if that's
  ever actually wanted (D30). (Resolved by D31: the ASCII `<` polar
  stand-in is now a real ∠ glyph, Terminus default.)
- **Font/glyph watch-items (Session 19, judge on device)**: informally
  spot-checked during the 2026-07-22 Phase 4A-4C pass and reported looking
  correct — not a dedicated sweep, so still worth a proper pass if time
  allows, but no longer a blind spot. Open sub-items: whether `√` read as
  inline-only (`√(x)`, no vinculum) is acceptable; whether the shared
  Unifont-derived `i`/⇒ glyphs look consistent against Terminus's own glyph
  shapes; big-radical display and true subscripts (`Sₓ`, `σₓ`) remain
  KIV/wishlist items (D31).
- **Pico 1 watch-items (task 3D.14 + Phase 4A-4C, 2026-07-22)**: `!`
  (factorial) syntax error in non-REAL Number mode — **FIXED 2026-07-25**
  (`5852c35`, `complexexpr` now shares engine's postfix-`!` rewrite;
  `5!`/`4!` HW-verified). The list editor and 5000-point scatter plot sluggishness
  are **fixed as of the same day (D35)**: bucketed stat-plot point cache,
  list-editor dirty-band narrowing, and (the real bottleneck behind "large
  lists feel sluggish to enter") one-file-per-list/matrix SD persistence —
  all flashed and developer-confirmed on the Pico 1, see `worklog.md`'s
  fourth 2026-07-22 entry and `decisions.md` D35. Two more from the
  Phase 4A-4C pass, **both since shipped in Phase 4D** (this list was
  written 2026-07-22, before 4D landed them): the home-screen `MatAns`
  token arrived as **4D.14** (`matans` is a real expression token now —
  `mat_expr.cpp:591`, `decisions.md` D-line 60), and **fnInt shading now
  follows the curve color** — darkened palette per slot, `4D.11`
  (`graph_screen.cpp:1146`, "was a fixed blue"). All originally logged in
  `session3D14-pico1-observations-verbatim.md`,
  `phase4abc-pico1-observations-verbatim.md`, and `phase3-retro.md`.
- Backlog: D14 rail settle ([next-bench-session.md](next-bench-session.md) —
  the last deferred HW item); 340-point curve cache cap; audio HAL; licensing (D17 —
  display/keyboard rewrites remain); dual-core display service (D10
  addendum — **DONE 2026-07-25: root-caused + fixed + pipeline shipped,
  see "The next job" #1b; two non-blocking follow-ups there — Pico 2
  full-frame pipeline, and compute-parallelizing `recompute_function`**);
  **stale diag-screen label — DONE 2026-07-25**: `main.cpp` header comment
  de-staled; diag title line now shows `Phase 4C [<hash>-dev]` right-aligned
  (build id via CMake `PICOCALC_BUILD_ID` = `git rev-parse --short HEAD` +
  dirty check); leftover per-strip `Frame:` counter removed. Also added a
  **die-temperature read** (`platform::die_temp_c()`, on-chip ADC ch 4) on
  the diag screen + a `temp:` 30 s serial heartbeat (idle ~28-31 C).

## Feature wishlist

Desired-but-unplanned features live in **[wishlist.md](wishlist.md)**. Complex
numbers and TI-84 CALC-menu graph analysis graduated into Phase 4 (sub-phases
4C and 4B — both code-complete). The 2026-07-21 stocktaking session (D32/D33)
graduated most of the rest: eight items into Phase 4D, one into Phase 6 §9,
and the old "symbolic display" item split in two — pi-ticks/`▶Frac` into 4D,
surd/exact-value display into Phase 5 §10.1. What's left unscheduled:
antialiased font rendering (revisit once the Phase 6 desktop-emulator
candidate exists) and the SD list-data-file/CBL-CBR half of the old
"beyond 6 lists" item. See [wishlist.md](wishlist.md) for current detail.

## Hardware debugging kit (reminder)

- Serial: **plain `cat` reads nothing** — pico stdio_usb only transmits
  with DTR asserted. Interactive: `./scripts/monitor.sh` (screen).
  Non-interactive/agent: `./scripts/serial-capture.py [seconds]
  [match-substring]`. Lines: `late-init:` (incl. `lists loaded`),
  `battery:` (change + 30 s heartbeat), `psram-bulk:` (30 s heartbeat),
  `graph recompute: N us`.
- Flash: see the Pico 2 notes above; Pico 1 BOOTSEL volume is `RPI-RP2`.
- `picocalc_diag` target = vendored-only display test for bisecting.
- Build env: `PICO_SDK_PATH=$PWD/pico-sdk`,
  `PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.
- Session protocol: read this file first when starting fresh; update it
  before ending a session.
