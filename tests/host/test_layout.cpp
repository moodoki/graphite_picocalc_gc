// Host-side structural tests for the layout builder (no framebuffer).
// Verifies node types and sizing for the Phase 1 constructs.

#include <cstdio>
#include <cstring>

#include "gfx/font.hpp"
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

    // "pi" renders as the baked Greek glyph (D24) — one char wide.
    {
        auto* n = build("pi");
        expect(n != nullptr && n->type == NodeType::kText &&
                   n->t.text[0] == gfx::kGlyphPi && n->t.text[1] == 0,
               "'pi' is the pi glyph");
        expect(n != nullptr && n->width == 8, "'pi' width = 1 char");
        auto* h = build("2*pi");
        expect(h != nullptr && h->type == NodeType::kHBox && h->h.count == 3 &&
                   h->h.items[2]->t.text[0] == gfx::kGlyphPi,
               "'2*pi' substitutes the glyph");
        // Identifiers merely containing 'pi' are untouched.
        auto* s = build("pit");
        expect(s != nullptr && s->type == NodeType::kText &&
                   std::strcmp(s->t.text, "pit") == 0,
               "'pit' is not substituted");
    }

    // Complex 'i' and polar 'theta' render as their glyphs; a lone 'i'
    // is always the imaginary unit (testdrive 2026-07-21).
    {
        auto* n = build("i");
        expect(n != nullptr && n->type == NodeType::kText &&
                   n->t.text[0] == gfx::kGlyphImagI && n->t.text[1] == 0,
               "'i' is the imaginary-unit glyph");
        // "3+2i" uses implicit multiply, so it renders via the plain-text
        // fallback — but preprocessing still swaps the trailing 'i'.
        auto* h = build("3+2i");
        expect(h != nullptr && h->type == NodeType::kText &&
                   h->t.text[std::strlen(h->t.text) - 1] == gfx::kGlyphImagI,
               "'3+2i' ends in the imaginary glyph");
        auto* t = build("theta");
        expect(t != nullptr && t->type == NodeType::kText &&
                   t->t.text[0] == gfx::kGlyphTheta && t->t.text[1] == 0,
               "'theta' is the theta glyph");
        // Identifiers merely containing 'i'/'theta' are untouched.
        auto* s = build("sin");
        expect(s != nullptr && s->type == NodeType::kText && std::strcmp(s->t.text, "sin") == 0,
               "'sin' is not substituted");
        // "sqrt(9)" renders as the radical glyph + (9) — a real call, not
        // a text fallback (big radical over the arg is KIV).
        auto* q = build("sqrt(9)");
        expect(q != nullptr && q->type == NodeType::kHBox && q->h.count == 2 &&
                   q->h.items[0]->t.text[0] == gfx::kGlyphSqrt,
               "'sqrt(9)' uses the radical glyph");
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

    // Function call in a fraction (HW-found 2026-07-11: rendered inline):
    // 1/sqrt(2) stacks, denominator is the call HBox.
    {
        auto* n = build("1/sqrt(2)");
        expect(n != nullptr && n->type == NodeType::kFraction,
               "'1/sqrt(2)' is Fraction");
        expect(n != nullptr && n->bin.b->type == NodeType::kHBox &&
                   n->bin.b->h.count == 2 &&
                   n->bin.b->h.items[0]->t.text[0] == gfx::kGlyphSqrt,
               "denominator is the sqrt call");
    }
    {
        auto* n = build("sin(x)/2");
        expect(n != nullptr && n->type == NodeType::kFraction,
               "'sin(x)/2' is Fraction");
    }

    // Power in a fraction: x^2/2 stacks with a Superscript numerator.
    {
        auto* n = build("x^2/2");
        expect(n != nullptr && n->type == NodeType::kFraction,
               "'x^2/2' is Fraction");
        expect(n != nullptr && n->bin.a->type == NodeType::kSuperscript,
               "numerator is Superscript");
    }

    // Unary minus is not a simple operand: 1/-2 stays inline.
    {
        auto* n = build("1/-2");
        expect(n != nullptr && n->type == NodeType::kHBox, "'1/-2' stays inline");
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

    // Scientific literal is one Text node (HW-found: "1e10" rendered "1")
    {
        auto* n = build("1e10");
        expect(n != nullptr && n->type == NodeType::kText, "'1e10' is Text");
        expect(n != nullptr && n->width == 4 * 8, "'1e10' width = 4 chars");
    }
    {
        auto* n = build("2.5e-3");
        expect(n != nullptr && n->type == NodeType::kText, "'2.5e-3' is Text");
    }
    // "2e" is 2 * Euler's e, not a truncated literal — but the grammar
    // has no implicit multiplication, so it falls back to plain text.
    {
        auto* n = build("2e");
        expect(n != nullptr && n->type == NodeType::kText &&
                   std::strcmp(n->t.text, "2e") == 0,
               "'2e' falls back to whole-string text");
    }

    // Store "2->A": the "->" collapses to the store-arrow glyph in
    // preprocessing; the result renders via the plain-text fallback
    // (testdrive 2026-07-21).
    {
        auto* n = build("2->A");
        const char store[2] = {gfx::kGlyphStore, 0};
        expect(n != nullptr && n->type == NodeType::kText && std::strstr(n->t.text, store) != nullptr,
               "'2->A' renders with the store-arrow glyph");
    }
    {
        auto* n = build("ncr(10,3)");
        expect(n != nullptr && n->type == NodeType::kText &&
                   std::strcmp(n->t.text, "ncr(10,3)") == 0,
               "'ncr(10,3)' falls back (grammar has no comma support)");
    }

    // Zero-arg call: rand() must not double the close paren (the empty
    // arg list once parsed ')' as a stray text atom -> "rand())").
    {
        auto* n = build("rand()");
        expect(n != nullptr && n->type == NodeType::kHBox, "'rand()' is HBox");
        expect(n != nullptr && n->width == 6 * 8, "'rand()' width = 6 chars");
        if (n != nullptr && n->type == NodeType::kHBox && n->h.count == 2) {
            auto* group = n->h.items[1];
            expect(group != nullptr && group->type == NodeType::kParen,
                   "'rand()' arg group is Paren");
            expect(group != nullptr && group->paren.child != nullptr &&
                       group->paren.child->width == 0,
                   "'rand()' arg list is empty");
        }
    }

    // Robustness: empty string
    expect(build("") == nullptr, "empty string -> nullptr");

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
