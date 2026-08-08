#pragma once

#include <cstddef>
#include <cstdint>

// Shared math scratch arena (pre-Phase-5 size pass, 2026-08-02).
//
// The math engine streams PSRAM-backed arrays through fixed 256-element
// (`kChunk`) SRAM buffers. Historically each subsystem owned a *private*
// set of these worst-case buffers in bss, even though only one math
// operation ever runs at a time (single-threaded on core 0 — core 1 does
// display only). That duplicated ~40 KB of never-simultaneously-live SRAM.
//
// This arena collapses the mutually-exclusive buffers onto one allocation.
// It is split into two disjoint regions by an actual concurrency edge:
//
//   * kCompute — list_expr | stats | infer | matops | cas. These are
//     mutually exclusive: none calls another (verified in the pre-Phase-5
//     review — e.g. infer deliberately does not call stats, stats solves its
//     3x3 inline rather than via matops, and list_expr's per-element eval
//     runs over already-lifted scalars). The CAS ExprPool (Phase 5,
//     math::cas, home-screen-only) also overlays this region — a top-level
//     CAS op holds it exclusively and never re-enters list_expr/stats/infer/
//     matops while doing so. They therefore safely overlay the same bytes.
//
//   * kListops — listops (sum/prod/seq/sort/cumsum/copy...). Kept DISJOINT
//     from kCompute because list_expr *calls* listops (see list_expr.cpp),
//     so a listops buffer can be live while list_expr's own buffers are.
//
// Invariant (must hold for correctness): within kCompute, only one of the
// four owners touches the region at a time. If a future change makes one of
// them call another (e.g. list_expr calling a stats reduction), that call
// must either not hold a kCompute buffer live across it, or the callee must
// move to its own region. A debug owner-guard enforcing this is a
// recommended follow-up (see docs/notes/pre-phase5-review.md).
//
// Sizes below are the worst-case footprint of each region's largest owner;
// each owning TU static_asserts its own layout fits.

namespace math::scratch {

// list_expr worst case: g_lift[6][256] + g_op_lift[4][256] + g_outbuf[256]
// = (6 + 4 + 1) * 256 * sizeof(double) = 22528 bytes.
constexpr std::size_t kComputeBytes = 22528;

// listops: g_buf[256] + g_in_a[128] + g_in_b[128] + g_out[128]
// = (256 + 3*128) * sizeof(double) = 5120 bytes.
constexpr std::size_t kListopsBytes = 5120;

constexpr std::size_t kArenaBytes = kComputeBytes + kListopsBytes;

// Region base pointers. Owners overlay typed views onto these (via a
// reference bound with reinterpret_cast) so existing call sites are
// unchanged. 16-byte aligned so a Complex (two doubles) view is valid.
std::uint8_t* compute_region();
std::uint8_t* listops_region();

}  // namespace math::scratch
