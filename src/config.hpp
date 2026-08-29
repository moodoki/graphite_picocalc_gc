// Compile-time configuration and board-specific constants.
//
// This is the ONE place where PICOCALC_PICO1 / PICOCALC_PICO2 #ifdefs
// are tolerated. Application code reads the constants below;
// it does not branch on board identity.

#pragma once

#include <cstddef>

namespace config {

// ---- Display ----
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 320;

// ---- Memory layout ----
// The host build (Phase 6.4, D92) folds in here rather than getting a branch
// of its own: a desktop has an address space and an FPU, so the Pico 2's
// answers are the right ones. It is NOT told it is a Pico 2 — defining that
// macro would make every future board branch silently apply to the host too.
// gfx/framebuffer.cpp reads the same distinction at three preprocessor sites
// and has to name PICOCALC_HOST at each of them; see the D94 amendment.
#if defined(PICOCALC_PICO2) || defined(PICOCALC_HOST)
// Pico 2 has 520 KB SRAM and a hardware FPU.
// Comfortable headroom — full SRAM framebuffer is fine.
constexpr bool kUseFullFramebuffer = true;
// (CAS ExprPool overlays the shared math scratch kCompute region — see
// src/math/cas/expr.cpp and math::scratch — so no dedicated pool constant.)
constexpr size_t kPythonHeapSize = 96 * 1024;
constexpr int kOverclockHz = 0;  // No overclock
constexpr bool kHasHardwareFpu = true;
#else
// Pico 1: tighter constraints, no FPU. Line-buffer rendering,
// smaller pools, framebuffer goes to PSRAM if needed.
constexpr bool kUseFullFramebuffer = false;
// (CAS ExprPool overlays the shared math scratch kCompute region — see
// src/math/cas/expr.cpp and math::scratch — so no dedicated pool constant.)
// 40 KB, not 48: D61 (2026-08-14) pre-committed the cut ahead of 6A
// landing, after a size-report found only 2.2 KB of margin above the
// 56 KB threshold with no 6A code written. Phase 6 spec §4.4/Risk 6
// state this as the shipped number, not a conditional lever.
constexpr size_t kPythonHeapSize = 40 * 1024;
constexpr int kOverclockHz = 200'000'000;  // 200 MHz
constexpr bool kHasHardwareFpu = false;
#endif

// ---- Render strip size (line-buffer mode only) ----
// 16 -> 8 (D70 lever C, 2026-08-15): halves strip_buf to 10,240 B.
// CHOSEN BY MEASUREMENT, not argument. Three configs were flashed and
// timed over an identical 32-evaluation workload on the Pico 1:
//   16px double-buffered (was)  avg 129.4 ms  max 147.2 ms  51 KB free
//   8px  double-buffered (now)  avg 137.5 ms  max 152.5 ms  60 KB free
//   16px SINGLE-buffered        avg 140.5 ms  max 160.8 ms  61 KB free
// Single-buffering serialises core-0 render against core-1 DMA and is
// both the slowest and barely roomier, so 8px keeps D10s pipeline
// overlap and pays ~6.3% instead of ~8.6%. One run each; the ordering
// is consistent across avg and max.
//
// STAYS AT 8, re-measured 2026-08-23 (issue #38). 6B closed leaving
// 15.2 KB free on the Pico 1, above the ~12 KB the issue set as the
// point where a Python-free build would be the only way to offer the
// faster render — so restoring 16 was affordable, was built, was
// flashed, and was then reverted on the numbers:
//
//   8px  (this)  render+overhead 140.9 / 140.5 ms   15.2 KB free
//   16px         render+overhead 135.9 / 135.9 ms    5.4 KB free
//
// Two runs each, same 32-expression workload, host round trip minus the
// reported evaluation time (main.cpp's `us=` field, D70's own A/B
// method). The gain is 4.8 ms — 3.4%, not the ~6.3% recorded above,
// which was measured before 6B existed and no longer holds. Paying
// 10,008 bytes, two thirds of the board's remaining headroom, for a
// 3.4% frame time nobody can see is the wrong trade.
//
// If this is ever raised again it must be Pico-1-only: strip_buf is
// allocated on both boards, but only the Pico 1 renders in strips — the
// Pico 2 pushes a whole framebuffer and keeps it solely as the script
// canvas's scratch, where a bigger strip costs 10 KB and buys nothing.
constexpr int kStripHeight = 8;
constexpr size_t kStripBytes =
    static_cast<size_t>(kScreenWidth) * kStripHeight * 2;  // RGB565 = 2 bytes/px

// ---- Limits ----
constexpr int kMaxYFunctions = 7;  // Y1..Y7
constexpr int kMaxScreenStack = 8;
constexpr int kHistorySize = 50;  // expression history entries
constexpr int kMaxExprLen = 256;  // characters per expression

}  // namespace config
