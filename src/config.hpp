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
constexpr int kStripHeight = 16;  // 16 scanlines per buffer
constexpr size_t kStripBytes =
    static_cast<size_t>(kScreenWidth) * kStripHeight * 2;  // RGB565 = 2 bytes/px

// ---- Limits ----
constexpr int kMaxYFunctions = 7;  // Y1..Y7
constexpr int kMaxScreenStack = 8;
constexpr int kHistorySize = 50;  // expression history entries
constexpr int kMaxExprLen = 256;  // characters per expression

}  // namespace config
