// Host-side structural tests for the layout builder (no framebuffer).
// Verifies node types and sizing for the Phase 1 constructs.

#include <cstdio>
#include <cstring>

#include "render/layout_builder.hpp"
#include "render/layout_node.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

const render::Metrics kM{8, 12};  // 8x12 interim font

void expect(bool cond, const char* what) {
    ++g_checks;
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

render::LayoutNode* build(const char* s) {
    return render::build_layout(s, kM);
}

}  // namespace

int main() {
    using render::NodeType;

    // Plain number -> single TextNode
    {
        auto* n = build("42");
        expect(n != nullptr && n->type == NodeType::kText, "'42' is Text");
        expect(n != nullptr && n->width == 2 * 8, "'42' width = 16");
        expect(n != nullptr && n->height == 12, "'42' height = 12");
    }

    // Simple fraction 1/2 -> FractionNode, taller than one line
    {
        auto* n = build("1/2");
        expect(n != nullptr && n->type == NodeType::kFraction,
               "'1/2' is Fraction");
        expect(n != nullptr && n->height == 12 + 12 + 3,
               "'1/2' height = num+den+3");
        expect(n != nullptr && n->bin.a->type == NodeType::kText &&
                   n->bin.b->type == NodeType::kText,
               "'1/2' operands are Text");
    }

    // Complex operand: (1+2)/x keeps a paren numerator, still a fraction
    {
        auto* n = build("(1+2)/x");
        expect(n != nullptr && n->type == NodeType::kFraction,
               "'(1+2)/x' is Fraction");
        expect(n != nullptr && n->bin.a->type == NodeType::kParen,
               "numerator is Paren");
    }

    // Non-simple operand: sum in numerator without parens -> inline '/'
    {
        auto* n = build("1+2/3+4");
        // Top level is an addition HBox; the middle term "2/3" is simple
        // on both sides so it becomes a fraction inside.
        expect(n != nullptr && n->type == NodeType::kHBox,
               "'1+2/3+4' top is HBox");
    }

    // Superscript x^2
    {
        auto* n = build("x^2");
        expect(n != nullptr && n->type == NodeType::kSuperscript,
               "'x^2' is Superscript");
        expect(n != nullptr && n->height > 12, "'x^2' taller than one line");
        expect(n != nullptr && n->width == 8 + 8, "'x^2' width = base+exp");
    }

    // Right-associative: 2^3^2 -> Super(2, Super(3,2))
    {
        auto* n = build("2^3^2");
        expect(n != nullptr && n->type == NodeType::kSuperscript,
               "'2^3^2' is Superscript");
        expect(n != nullptr && n->bin.b->type == NodeType::kSuperscript,
               "'2^3^2' exponent is itself a Superscript");
    }

    // Parenthesized group
    {
        auto* n = build("(1+2)");
        expect(n != nullptr && n->type == NodeType::kParen, "'(1+2)' is Paren");
        expect(n != nullptr && n->width == (3 * 8) + 2 * 8,
               "'(1+2)' width = inner + 2 chars");
    }

    // Function call sin(x) -> HBox[Text("sin"), Paren(x)]
    {
        auto* n = build("sin(x)");
        expect(n != nullptr && n->type == NodeType::kHBox, "'sin(x)' is HBox");
        expect(n != nullptr && n->h.count == 2 &&
                   n->h.items[0]->type == NodeType::kText &&
                   n->h.items[1]->type == NodeType::kParen,
               "'sin(x)' = name + paren");
    }

    // Nested: (1+2)/(3^4) — fraction of two parens, den paren contains super
    {
        auto* n = build("(1+2)/(3^4)");
        expect(n != nullptr && n->type == NodeType::kFraction,
               "'(1+2)/(3^4)' is Fraction");
        expect(n != nullptr && n->bin.b->type == NodeType::kParen &&
                   n->bin.b->paren.child->type == NodeType::kSuperscript,
               "denominator paren wraps a superscript");
    }

    // Robustness: empty string
    expect(build("") == nullptr, "empty string -> nullptr");

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
