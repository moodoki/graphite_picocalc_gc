#pragma once

#include <cstddef>
#include <new>
#include <utility>

// Bump allocator for short-lived layout trees (task 3.7). Reset before
// each layout build; no per-node free. Nodes are trivially destructible
// and only point within the pool, so reset() reclaims everything.
namespace render {

constexpr std::size_t kLayoutPoolSize = 8192;

void pool_reset();
void* pool_alloc(std::size_t bytes, std::size_t align);
std::size_t pool_used();

// ---- Scratch end (D47) ----
//
// The same two-ended trick D45 gave the CAS ExprPool: nodes grow up from
// the bottom, short-lived parser scratch grows down from the top, and the
// two ends must not meet.
//
// It exists because the layout parser's per-level staging arrays
// (LayoutNode* items[kMaxChildren] in parse_expr and parse_term) were 128 B
// of *stack* each, per recursion level. build_layout runs inside
// HomeScreen::render(), i.e. at the deepest point of the call stack, and
// core 0 has 4 KB before core 1's — measured on hardware, three nested
// parens reached 3,716 B and four faulted. Moving the staging here takes
// ~256 B per level off the stack at no cost in bss.
//
// Scratch cannot share the node end: the arrays stay live across nested
// parse calls that allocate nodes, so the two interleave.
std::size_t pool_scratch_mark();
void* pool_scratch_alloc(std::size_t bytes, std::size_t align);
void pool_scratch_release(std::size_t mark);

template <typename T, typename... Args>
T* pool_new(Args&&... args) {
    void* mem = pool_alloc(sizeof(T), alignof(T));
    if (mem == nullptr) {
        return nullptr;
    }
    return new (mem) T(std::forward<Args>(args)...);
}

}  // namespace render
