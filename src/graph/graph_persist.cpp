#include <cstdint>
#include <cstring>
#include <type_traits>

#include "platform/storage.hpp"
#include "graph/graph_state.hpp"

namespace graph {

namespace {

constexpr const char* kPath = "/picocalc/graphstate.dat";

// Bump when the GraphState layout changes ("PCG3", ...): old images
// then fail to load and callers fall back to defaults/migration.
constexpr char kMagic[4] = {'P', 'C', 'G', '2'};

struct Image {
    char magic[4] = {};
    uint32_t size = 0;
    GraphState state;
};

static_assert(std::is_trivially_copyable_v<GraphState>, "raw image dump requires POD state");

// ~6.5 KB — far too big for the stack; reused by save and load.
Image g_image;

}  // namespace

bool GraphState::save(platform::Storage& storage) const {
    if (!storage.mounted()) {
        return false;
    }
    std::memcpy(g_image.magic, kMagic, sizeof(kMagic));
    g_image.size = sizeof(GraphState);
    g_image.state = *this;
    return storage.write_file(kPath, reinterpret_cast<const uint8_t*>(&g_image), sizeof(g_image));
}

bool GraphState::load(platform::Storage& storage) {
    if (!storage.mounted()) {
        return false;
    }
    const int n = storage.read_file(kPath, reinterpret_cast<uint8_t*>(&g_image), sizeof(g_image));
    if (n != static_cast<int>(sizeof(g_image)) ||
        std::memcmp(g_image.magic, kMagic, sizeof(kMagic)) != 0 ||
        g_image.size != sizeof(GraphState)) {
        return false;
    }
    *this = g_image.state;
    return true;
}

}  // namespace graph
