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

template <typename T, typename... Args>
T* pool_new(Args&&... args) {
    void* mem = pool_alloc(sizeof(T), alignof(T));
    if (mem == nullptr) {
        return nullptr;
    }
    return new (mem) T(std::forward<Args>(args)...);
}

}  // namespace render
