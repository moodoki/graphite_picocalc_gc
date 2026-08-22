# Phase 6.3 Spec: Native compiled apps (`.uf2` launcher entries)

**Prerequisite phases**: [Phase 6A](phase6-spec.md) (the `AppRegistry` and its
tier-2 hook, the launcher screen), [Phase 6B](phase6-spec.md) §4.5 (the SD app
manifest scan and its `type=native` refusal, which is this phase's entry point).
Phase 6 must be **closed and merged** before this starts.

**Scope**: A compiled `.uf2` on the SD card becomes its own launcher tile, on
the same list as a Python app. Selecting it validates the image, writes it into
a reserved flash slot that never overlaps the calculator, and reboots into it.
Returning is a marker clear and a reboot, writing nothing.

**Status**: **PROPOSED — this is a draft contract, not a committed one.**
Drafted 2026-08-16 from `phase6-spec.md` §3.4's stretch goal; **D88-D91 are all
open**, and the developer is researching further before deciding. Nothing here
is agreed, nothing is built, and §3.4's stretch goal remains what it was until
that decision is made. Read it as the strongest case that could be made for
this shape, with its own weak points marked — not as settled work.

Two things gate it even after that: task **6.3.0 is a feasibility spike that
gates every other task**, and task 6.3.10 is gated separately on §3.5's ATRANS
question.

**Sub-phase numbering** (proposed): dotted, so it would **not** gate Phase 6's
completion —
§3.4 was explicitly outside Phase 6's committed goals. 6.1 (home-screen
convenience scripts) and 6.2 (PCM sampler audio engine) are already reserved as
candidates; this is the first dotted Phase 6 sub-phase to be committed.

---

## 1. Why this is being built now

§3.4 sat as a stretch goal from 2026-07-21 to 2026-08-16 with no forcing
reason. It has one now, and it is not "compiled apps would be nice".

**Issue [#38](https://github.com/moodoki/graphite_picocalc_gc/issues/38), the
Python-free build.** D78 opened it as the only way to recover D70 lever C's
~6.3% render cost on the Pico 1, and deferred it by its own terms until Phase
6's final SRAM numbers were known. They now are — 15 KB free (Pico 1), 24 KB
(Pico 2) — and #38 as framed is a straight loss: **give up scripting entirely
to get the render time back.**

Native apps dissolve that trade (**D90**). Ship a Python-free calculator in
flash, and a `Python` tile that chain-loads a MicroPython-enabled build of this
same repo. The render time is recovered on the default path; scripting costs
one reboot.

**It works because no user state lives in flash.** Variables, lists, matrices
and graph state persist to SD through `platform::Storage`, so the hop between
two firmware images is invisible to the data. That falls out of every
persistence decision this project has already made; it was not designed for
this, and it is what makes two resident images behave like one calculator.

The consequence for this spec: **the app template (6.3.7) must be able to build
*this project* as an app**, not only third-party code. That is one extra CMake
target, but it has to be true from the start rather than retrofitted.

## 2. The layout — the calculator chain-loads a separate slot

Full reasoning in **D88**. §3.4 as written required a standalone permanent
bootstrap binary at the reset vector (D66), because the calculator cannot make
the boot decision when an app has overwritten it. **That premise holds only for
the one-payload-region layout §3.4 assumed.** Give the app its own
non-overlapping slot and the calculator is always what the boot ROM starts.

```
Pico 1 (2 MB flash)                 Pico 2 (4 MB flash)
0x10000000  calculator  (640 KB)    0x10000000  calculator  (~630 KB)
0x10100000  APP SLOT    1 MB        0x10200000  APP SLOT    2 MB
0x101FFFFF  end                     0x103FFFFF  end
```

| | |
|---|---|
| **launch** | validate (§3), erase + program the slot, marker to `watchdog_hw->scratch[2]`, `watchdog_reboot()` |
| **boot** | the calculator's `main()` reads the marker before any init and chain-jumps to the slot |
| **return** | clear the marker, `watchdog_reboot()`. **No flash write.** |
| **hung app** | power cycle → POR clears the scratch registers → calculator |
| **corrupt slot write** | the calculator was never touched → still boots |

**The safety net is structural, not procedural.** §3.4 calls "worst case is a
corrupted app slot, never a device that won't boot" non-negotiable. Under D66's
layout that was a property of the re-flash code being correct. Here it is a
property of the address map: the only image the boot ROM can start is the one
the loader never writes to.

**What this retires** — D66's standalone bootstrap, its linker script and its
fresh-device install step; and D59/P6-6 entirely (`/picocalc/firmware.uf2`, the
lazy self-snapshot, the build-size symbol, the `pico_set_program_version` gate).
**What survives D66 unchanged, and is load-bearing**: the dedicated scratch
marker. Bare `watchdog_caused_reboot()` is ambiguous with D47's fault recovery,
the bulk-PSRAM self-test and an ordinary `picotool load -f -x`, so `scratch[2]`
is what the boot path checks. (`scratch[0]`/`[1]` are the bulk test's,
`[4]`–`[7]` are boot-ROM-reserved.)

**Apps are linked at the slot base, from this project's template.** A stock
`.uf2` from anywhere else is linked at 0x10000000, where the calculator lives —
so on the **Pico 1** it cannot run from the slot at all, and on the **Pico 2**
it runs only through the address-translation path in §3.5. Neither is a cost of
this layout: §3.4's original bootstrap-at-flash-start layout collides with a
stock image's link address exactly as the calculator does, so **the layout
choice and third-party support are independent** (D91).

**The calculator acquires a hard ceiling**: its image may never grow into the
slot base. 6.3.8 makes that a build-time assertion rather than something a
flash write discovers. Today's 640 KB against a 1 MB reservation leaves 384 KB
of growth on the tighter board.

## 3. Board discrimination — three gates, no guessing

**This is the phase's one upfront requirement**, and it comes from how the
hardware is actually used: one SD card moves between the Pico 1 and the Pico 2,
so **a wrong-board binary sitting on the card is the normal case**. Full
reasoning in **D89**.

Nothing is erased until all three gates pass. Each catches a case the others do
not.

### Gate 1 — the UF2 family ID

A block's `file_size` field is the family ID iff `UF2_FLAG_FAMILY_ID_PRESENT`
is set in `flags` — see `pico-sdk/src/common/boot_uf2_headers/include/boot/uf2.h`,
which is already in the tree and is the authority for every constant below.
Ours becomes `config::kUf2FamilyId`: `RP2040_FAMILY_ID` (0xe48bff56) on the
Pico 1, `RP2350_ARM_S_FAMILY_ID` (0xe48bff59) on the Pico 2. It lives in
`config.hpp` because that is the one place board `#ifdef`s are tolerated — the
loader reads a constant and never branches on board identity itself.

| condition | verdict |
|---|---|
| bad `magic_start0`/`magic_start1`/`magic_end`, or file length not a multiple of 512 | `kNotUf2` |
| `UF2_FLAG_FAMILY_ID_PRESENT` absent | `kNoFamilyId` — **refuse**, never guess a board |
| `UF2_FLAG_NOT_MAIN_FLASH` set | skip the block, do not count it |
| family is not ours | skip the block, **but count it** |
| zero blocks for us, at least one for another *known* family | `kWrongBoard`, naming it — "needs Pico 2" |
| `RP2350_RISCV`, `RP2350_ARM_NS`, `ABSOLUTE`, `DATA`, `CYW43` | `kUnsupportedFamily`, named |
| `payload_size` > 476 | `kNotUf2` |

Counting foreign blocks rather than discarding them does two things: it lets a
**multi-family ("universal") `.uf2` program only our blocks**, so one file can
serve both boards from the shared card; and it is what makes `kWrongBoard`
distinguishable from `kNotUf2`. Both are refusals, but only one of them is the
user's card working exactly as intended.

### Gate 2 — every target address inside our slot, 256-byte aligned

Independent of gate 1 **by construction**: the slot base differs per board
(0x10100000 vs 0x10200000), so a Pico 2 app's target addresses fall outside the
Pico 1's slot even if gate 1 were somehow bypassed. Two mechanisms, one answer.

### Gate 3 — our own app header at the slot base

A 16-byte header the template places at the slot base: magic, family,
vector-table offset, build stamp. It answers the entry-point question the
loader needs anyway — the SDK's image layout differs between RP2040 (boot2,
then vectors at +0x100) and RP2350 — and it separates "a valid RP2040 `.uf2`"
from "one of our apps". The build stamp is what lets the launcher warn that a
Python app image and the calculator have drifted (D90).

**This gate classifies; it does not refuse** (D91). An image of our own family
with no header is **`kForeign`**, not `kNotUf2`, and §3.5 decides what happens
to it. Refusing here would bake in "third-party images never run", which is
false on the Pico 2.

### Where the checks run

**Boot scan**: block 0 only, 512 bytes per native app. That is a definite
verdict for every single-family file, which is every file the template
produces, and it keeps the boot scan cheap. **Launch**: an authoritative pass
over every block, before a single sector is erased.

### The parser is pure and host-tested

Same shape 6B.15 established for `parse_app_manifest`, and for the same reason:
its 29 rules are host checks rather than something a card with a bad file on it
teaches you one boot at a time. Blocks can be synthesised in a buffer, so
**the requirement this phase exists to satisfy is provable with no hardware at
all** — including checking a genuine RP2350 header against a Pico 1 build.

### What the user sees

A wrong-board app is **listed, greyed, with the reason** ("needs Pico 2");
selecting it explains rather than launching, and erases nothing. On a card
deliberately shared between two boards, a silently missing tile is
indistinguishable from a broken card, a bad manifest, or a failed scan.

## 3.5 Third-party images — Pico 2 runs them, Pico 1 delegates

Full reasoning in **D91**. A "third-party" image here means a stock `.uf2`
built by anyone else for this hardware — Coyote OS, PicoMite, a port — linked
at 0x10000000 and carrying none of our header. The PicoCalc has a real
ecosystem of these, which is why this is worth answering rather than assuming
away.

**Pico 2: run it from the slot, untouched, via QMI address translation.**
The RP2350 has eight XIP address-translation apertures (`QMI_ATRANS0..7`),
documented for exactly this: *"Address translation allows a program image to be
executed in place at multiple physical flash addresses … without the overhead
of position-independent code."* Set `ATRANS0.BASE` to the slot's 4 KiB index,
flush the XIP cache, jump. The image runs believing it is at flash start; the
calculator is never overwritten, and a power cycle still returns to it because
reset restores the identity mapping.

`ATRANS0.SIZE` is set to the slot size, which **bounds the app in hardware** —
*"offsets greater than SIZE return a bus error, and do not cause a QSPI
access"* — so an over-reading image faults instead of quietly reading the
calculator. That is a stronger containment property than the template path has.

**Pico 1: refuse, and name `uf2loader`.** The RP2040 has no address
translation, so a stock image must physically occupy 0x10000000. Rebuilding
`uf2loader`'s architecture to allow that (a boot-start shim, a top-of-flash
chooser, the app overwriting the calculator while it runs) duplicates a GPLv3
project that already works on this hardware, reintroduces a bootstrap
component, and gives up §2's safety net on the board with the least flash. The
boundary instead: **we launch apps; `uf2loader` switches firmwares.** That is
also the first real job for the place D66 put `uf2loader` — optional,
user-installed, never in our automatic path.

**Two warnings this cannot design away**, both to surface at launch time:

1. **ATRANS translates reads, not flash programming.** A stock app that writes
   flash uses physical offsets and can corrupt the calculator. `uf2loader`
   documents the same hazard ("if the app does not itself write to flash").
   Recovery is a USB reflash.
2. **A third-party image has no return path.** It knows nothing about our
   marker, so the way back is a power cycle — which works, and needs no
   cooperation from the app.

**Gated on the spike** (6.3.0, extended by 6.3.10): whether ATRANS survives the
app's own early boot. An SDK image configures QMI timings at startup, and if
anything there resets the aperture to identity the app is suddenly executing
from the calculator's flash. The switch routine must also be RAM-resident and
must never return to flash — the moment ATRANS changes, the code that changed
it is no longer mapped.

## 4. Task breakdown

| id | task | hrs | done when |
|---|---|---|---|
| **6.3.0** | **Feasibility spike — gates everything else.** Erase + program a pattern into the slot from a RAM-resident function (core 1 parked via `multicore_reset_core1()`, IRQs off, PSRAM PIO quiesced) and read it back; marker → `watchdog_reboot()` → chain-jump into a trivial second image that prints over USB; confirm POR zeroes `scratch[2]`; confirm a deliberately corrupted slot still leaves the calculator bootable | 8 | All four pass **on both boards**. If the RP2350 chain-jump is not clean, **stop and re-plan** — do not build on top of it |
| 6.3.1 | `platform::AppSlot` flash HAL — `erase()`, `program()`, `verify()`, slot geometry in `config.hpp` | 5 | Write, reboot, read back intact; the calculator still boots afterwards |
| 6.3.2 | **`platform::uf2` — pure parser and the three gates.** Verdict enum, per-block check, whole-file scan summary, verdict→text. New `tests/host/test_uf2.cpp`, wired into `scripts/host-tests.sh` beside `test_apps` | 6 | Every §3 rule is a host check, including a real RP2350 header refused by a Pico 1 build |
| 6.3.3 | `type=native` in the manifest — `src/platform/sd_apps.cpp` already parses and *refuses* it, which is the hook. `AppKind kind` + native path in `SdAppManifest`, default `app.uf2`, optional `native_pico1=`/`native_pico2=` overrides | 3 | A native manifest registers as `kNative`; the existing 29 checks extended |
| 6.3.4 | Boot scan verdict + launcher. `sd_app_scan.cpp` reads block 0 per native app and stores the verdict; `AppEntry` gains enabled + reason; `launcher_screen.cpp` greys disabled rows | 5 | A wrong-board app appears greyed with its reason and cannot be launched |
| 6.3.5 | Launch path — full validation pass, then erase/program/verify, marker, reboot. Progress UI (~1.3 MB read, ~640 KB write is seconds) | 5 | A refusal at any gate leaves flash untouched, provably |
| 6.3.6 | Chain-load in `main.cpp` (marker check before any init) + return-to-calculator: an `app_return()` helper in the template, exposed to Python apps as a `calc` binding | 4 | Launch an app, return to the calculator, launch it again |
| 6.3.7 | **App template** — `examples/native/`, linked at the slot base for both boards, emitting `app-rp2040.uf2` / `app-rp2350.uf2` plus the gate-3 header and the return helper. **Must be able to build this repo as an app** (D90) | 5 | The example runs from the slot; a slot-linked calculator build does too |
| 6.3.8 | Build-time guard: CMake/linker assertion that the calculator image cannot grow into the slot base | 1 | An oversized build fails at link, not at launch |
| 6.3.9 | Hardware pass on both boards, the wrong-board card test, the recovery tests; docs (`USAGE.md`, `FEATURES.md`, this spec) | 4 | §6's checklist complete |
| **6.3.10** | **Third-party images on the Pico 2 (§3.5, D91)** — `ATRANS0.BASE`/`SIZE` set from a RAM-resident switch routine, XIP cache flush, jump; `kForeign` classification surfaced in the launcher with both warnings; refusal on the Pico 1 naming `uf2loader`. **Extends 6.3.0's spike first**: prove ATRANS survives a stock image's own QMI setup before building this | 7 | A stock PicoCalc firmware boots from the slot on the Pico 2, a power cycle returns to the calculator, and the same file is refused with an accurate reason on the Pico 1 |

**Total ~53 hrs** (~46 without 6.3.10, which is separable and can be dropped
without affecting anything above it). Higher than §3.4's recorded ~25–35, which D66 had already
flagged as understated — but §2's layout removes roughly 10–15 hrs of bootstrap
and self-snapshot work that would otherwise have pushed it past 55.

## 5. Risks

**R1 — the flash-write step is genuinely new, on a dual-core board.** There is
no `flash_range_program`/`flash_range_erase` anywhere in this tree; all
persistence to date is SD I/O through `platform::Storage`. Core 1 is driving
the panel and a PIO is driving PSRAM, both of which must be quiesced. *Mitigation*:
6.3.0 proves it in isolation before anything depends on it. The RAM-residency
pattern already exists — `src/platform/display.cpp`'s whole DMA path is
`__not_in_flash_func` for a related reason (XIP contention hard-faulted it,
D10 addendum).

**R2 — the RP2350 differs where it matters.** Bootrom scratch-register use,
IMAGE_DEF, and the M33's VTOR and security state all bear on the chain-jump.
D65's hardware result covers the reset-reason half of the marker but **not**
whether POR actually zeroes `scratch[2]`. *Mitigation*: 6.3.0 runs on both
boards and assumes nothing a board has not shown. This is the same discipline
D85 needed after the Pico 2 found a race the Pico 1 structurally could not
(issue #39).

**R3 — two firmware images can drift.** A user who updates the calculator and
not the Python app gets two `calc` versions in one session. *Mitigation*: the
gate-3 build stamp lets the launcher say so. Nothing forces them into step, and
this spec does not try to.

**R5 — ATRANS may not survive a stock image's own boot (6.3.10 only).** An SDK
image configures QMI timings at startup; if that resets the aperture to
identity, the app begins executing from the calculator's flash. *Mitigation*:
6.3.0's spike answers it before 6.3.10 is built, and 6.3.10 is separable — the
rest of the phase does not depend on it. A stock app that *writes* flash can
corrupt the calculator regardless, since programming uses physical offsets;
that is a launch-time warning, not something the design can prevent (D91).

**R4 — the Pico 1 flash budget stops being academic.** A MicroPython app image
is ~640 KB against a 1 MB slot. Comfortable, but 6.3.8's assertion is what keeps
it honest as both images grow.

## 6. Verification

- **§3's requirement is provable on the host** — `./scripts/host-tests.sh`
  covers all three gates against synthesised blocks. Do this before a board
  sees any of it.
- **6.3.0 is the hardware gate.** Nothing past it is built until the spike
  passes on both boards.
- **The wrong-board test is physical and explicit**: build the same example app
  for both boards onto one card, boot each board, confirm the foreign tile is
  greyed with the right reason and that selecting it erases nothing.
- **The recovery tests are explicit**: corrupt the slot deliberately, power
  cycle, confirm the calculator boots. Hang an app, power cycle, same.
- Both boards build clean (`./scripts/build-all.sh`), `./scripts/lint.sh`,
  `python3 scripts/validate_md.py` on every touched doc.

## References

1. [Phase 6 spec](phase6-spec.md) §3.4 (the stretch goal this promotes), §3.1
   (`AppRegistry` tiers), §4.5 (the manifest scan)
2. [decisions.md](../notes/decisions.md) — **D88** (layout), **D89** (the three
   gates), **D90** (issue #38), **D91** (third-party images), and the amended
   **D66** / superseded **D59**
3. `pico-sdk/src/common/boot_uf2_headers/include/boot/uf2.h` — the block format
   and every family ID, in-tree
4. `pico-sdk/src/rp2350/hardware_regs/include/hardware/regs/qmi.h` —
   `QMI_ATRANS0..7`, the RP2350 address-translation apertures §3.5 uses, and
   the register documentation stating the intent
5. [uf2loader](https://github.com/pelrun/uf2loader) — the prior art on this
   hardware: top-of-flash loader, stock UF2s at flash start, key-at-power-on to
   return. **Not a dependency** (D66); the Pico 1 refusal points users at it
6. [issue #38](https://github.com/moodoki/graphite_picocalc_gc/issues/38) — the
   Python-free build this enables
