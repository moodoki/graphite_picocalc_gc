#include "math/var_store.hpp"

#include <cstdint>
#include <cstring>

#include "platform/storage.hpp"
#include "math/engine.hpp"
#include "math/types.hpp"

namespace math {

namespace {

constexpr const char* kVarsPath = "/picocalc/variables.dat";

// variables.dat image (4D.15): versioned since complex storage widened
// Variables — the pre-PCV1 file was a raw 224-byte vars[] dump with no
// header, so it simply fails the magic check and is ignored (one-time
// variable reset on first boot, same precedent as PCL/PCM/PCG bumps).
struct VarsImage {
    char magic[4];
    calc_t vars[Variables::kCount];
    calc_t imag[Variables::kCount];
};
constexpr char kVarsMagic[4] = {'P', 'C', 'V', '1'};

// static, not a local: 456 bytes is more than core 0's 4 KB stack wants to
// carry, and this is called from a script binding as well as from the screen.
VarsImage g_img;

}  // namespace

void save_variables(platform::Storage& fs) {
    if (!fs.mounted()) {
        return;
    }
    const Variables& v = engine().vars();
    std::memcpy(g_img.magic, kVarsMagic, sizeof(g_img.magic));
    std::memcpy(g_img.vars, v.vars, sizeof(g_img.vars));
    std::memcpy(g_img.imag, v.imag, sizeof(g_img.imag));
    fs.write_file(kVarsPath, reinterpret_cast<const uint8_t*>(&g_img), sizeof(g_img));
}

void load_variables(platform::Storage& fs) {
    Variables& v = engine().vars();
    const int n = fs.read_file(kVarsPath, reinterpret_cast<uint8_t*>(&g_img), sizeof(g_img));
    if (n != static_cast<int>(sizeof(g_img)) ||
        std::memcmp(g_img.magic, kVarsMagic, sizeof(g_img.magic)) != 0) {
        return;  // Missing, old-format, or truncated: keep defaults
    }
    std::memcpy(v.vars, g_img.vars, sizeof(v.vars));
    std::memcpy(v.imag, g_img.imag, sizeof(v.imag));
}

}  // namespace math
