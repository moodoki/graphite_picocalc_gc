#pragma once

#include <cstdint>

namespace render {

// Font metrics needed to size a layout tree, decoupled from gfx so the
// builder is host-testable without the framebuffer.
struct Metrics {
    int char_w;
    int char_h;
};

enum class NodeType : uint8_t {
    kText,
    kHBox,
    kFraction,
    kSuperscript,
    kParen,
};

// Single fixed-size node (pool-friendly). Fields are interpreted by
// type; a union keeps the footprint reasonable (~140 bytes).
struct LayoutNode {
    static constexpr int kMaxText = 28;
    static constexpr int kMaxChildren = 32;

    NodeType type;
    int width = 0;
    int height = 0;
    int baseline = 0;  // Pixels from node top down to the math baseline

    union {
        struct {
            char text[kMaxText];
        } t;
        struct {
            LayoutNode* items[kMaxChildren];
            int count;
        } h;
        struct {
            LayoutNode* a;  // Fraction numerator / superscript base
            LayoutNode* b;  // Fraction denominator / superscript exponent
        } bin;
        struct {
            LayoutNode* child;
        } paren;
    };

    LayoutNode() : type(NodeType::kText) { t.text[0] = 0; }
};

}  // namespace render
