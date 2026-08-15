#pragma once

#include <cstddef>
#include <cstdint>

// Shared one-shot I/O staging buffer (Phase 6 SRAM recovery, D70 lever A).
//
// Persistence and file-staging paths each used to own a private
// worst-case buffer in bss, even though only one of them is ever live:
// they are all driven by a single user action or a single boot step, on
// core 0, with no nesting. That duplicated ~15 KB of never-simultaneously
// -live SRAM. This is the same argument, and the same discipline, as
// math::scratch's compute arena (which reclaimed ~21.8 KB the same way).
//
// Current owners, largest first:
//   * apps::HomeScreen history file I/O          8,192 B
//   * graph::save/load_graph_state's Image        7,496 B
//   * math list/named-list/matrix persistence     2,048 B each
//   * apps::ListEditorScreen::delete_row's buf    2,048 B
//
// INVARIANT (must hold for correctness): only one owner touches this
// region at a time, and no owner holds a pointer into it across a call
// that could reach another owner. Two specific edges were checked when
// this landed and must be rechecked if those paths change:
//
//   * ListEditorScreen::delete_row() finishes its read_range/write_range
//     loop BEFORE calling save_lists(), so its buffer is dead by the
//     time list persistence takes the region. If a future edit moves
//     the save inside the loop, they collide.
//
//   * The home screen's `mode` command saves graph state and appends
//     history on separate branches of submit_input(), never nested.
//
// DELIBERATELY NOT A MEMBER: math::Array's own g_chunk (array.cpp).
// Array tier migration can fire from inside resize(), which the
// persistence *load* paths call while holding this region — so that
// buffer stays private. It is the one that genuinely can overlap.

namespace platform {

// Sized to the largest single owner (the 8 KB history tail). Owners
// static_assert their own need against it.
constexpr std::size_t kIoScratchBytes = 8192;

// 16-byte aligned so a Complex (two doubles) view over it is valid.
std::uint8_t* io_scratch();

}  // namespace platform
