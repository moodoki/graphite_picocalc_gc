#include "render/pool.hpp"

#include <cstdint>

namespace render {

namespace {
alignas(std::max_align_t) uint8_t g_pool[kLayoutPoolSize];
std::size_t g_offset = 0;
}  // namespace

void pool_reset() {
    g_offset = 0;
}

void* pool_alloc(std::size_t bytes, std::size_t align) {
    const std::size_t aligned = (g_offset + (align - 1)) & ~(align - 1);
    if (aligned + bytes > kLayoutPoolSize) {
        return nullptr;  // Out of pool; caller falls back to plain text
    }
    g_offset = aligned + bytes;
    return &g_pool[aligned];
}

std::size_t pool_used() {
    return g_offset;
}

}  // namespace render
