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
#ifdef PICOCALC_PICO2
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
constexpr int kStripHeight = 8;
constexpr size_t kStripBytes =
    static_cast<size_t>(kScreenWidth) * kStripHeight * 2;  // RGB565 = 2 bytes/px

// ---- Limits ----
constexpr int kMaxYFunctions = 7;  // Y1..Y7
constexpr int kMaxScreenStack = 8;
constexpr int kHistorySize = 50;  // expression history entries
constexpr int kMaxExprLen = 256;  // characters per expression

}  // namespace config
