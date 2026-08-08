#include "render/pool.hpp"

#include <cstdint>

namespace render {

namespace {
alignas(std::max_align_t) uint8_t g_pool[kLayoutPoolSize];
std::size_t g_offset = 0;                 // node end, grows up
std::size_t g_scratch = kLayoutPoolSize;  // scratch end, grows down
}  // namespace

void pool_reset() {
    g_offset = 0;
    g_scratch = kLayoutPoolSize;
}

void* pool_alloc(std::size_t bytes, std::size_t align) {
    const std::size_t aligned = (g_offset + (align - 1)) & ~(align - 1);
    // Bounded by the scratch end, not the buffer end — the two ends share
    // one array and must not cross.
    if (aligned + bytes > g_scratch) {
        return nullptr;  // Out of pool; caller falls back to plain text
    }
    g_offset = aligned + bytes;
    return &g_pool[aligned];
}

std::size_t pool_scratch_mark() {
    return g_scratch;
}

void* pool_scratch_alloc(std::size_t bytes, std::size_t align) {
    if (g_scratch < bytes) {
        return nullptr;
    }
    const std::size_t at = (g_scratch - bytes) & ~(align - 1);
    if (at < g_offset) {
        return nullptr;  // Would collide with the nodes
    }
    g_scratch = at;
    return &g_pool[at];
}

void pool_scratch_release(std::size_t mark) {
    g_scratch = mark;
}

std::size_t pool_used() {
    return g_offset;
}

}  // namespace render
