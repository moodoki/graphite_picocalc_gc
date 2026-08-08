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
        // 4D.5: the bar row (num height + 1) sits half a char above the
        // node baseline, centering on the midline of adjacent text.
        expect(n != nullptr && n->baseline == 12 + 1 + 12 / 2,
               "'1/2' bar centers on text midline");
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
        // Stage 4: a coefficient before a symbol glyph multiplies
        // implicitly, so "2*pi" typesets as "2pi" with no '*' node.
        auto* h = build("2*pi");
        expect(h != nullptr && h->type == NodeType::kHBox && h->h.count == 2 &&
                   h->h.items[1]->t.text[0] == gfx::kGlyphPi,
               "'2*pi' substitutes the glyph, no '*'");
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

    // Exact-form display (Phase 5 Stage 4): a single-atom radicand loses its
    // parens, and a coefficient in front of a radical multiplies implicitly,
    // so the CAS's "2*sqrt(2)" typesets as the handwritten "2√2".
    {
        auto* n = build("sqrt(2)");
        expect(n != nullptr && n->type == NodeType::kHBox && n->h.count == 2 &&
                   n->h.items[0]->t.text[0] == gfx::kGlyphSqrt &&
                   n->h.items[1]->type == NodeType::kText,
               "'sqrt(2)' is a bare radicand");
        expect(n != nullptr && n->width == 2 * 8, "'sqrt(2)' width = 2 chars");

        // A compound radicand still needs its parens — there is no vinculum.
        auto* c = build("sqrt(2+3)");
        expect(c != nullptr && c->type == NodeType::kHBox && c->h.count == 2 &&
                   c->h.items[1]->type == NodeType::kParen,
               "'sqrt(2+3)' keeps its parens");

        // ... and so does a radicand followed by '^', or "√x^2" would read
        // as sqrt(x^2).
        auto* s = build("sqrt(x)^2");
        expect(s != nullptr && s->type == NodeType::kSuperscript &&
                   s->bin.a->type == NodeType::kHBox &&
                   s->bin.a->h.items[1]->type == NodeType::kParen,
               "'sqrt(x)^2' keeps its parens under a power");

        auto* h = build("2*sqrt(2)");
        expect(h != nullptr && h->type == NodeType::kHBox && h->h.count == 2 &&
                   h->h.items[1]->type == NodeType::kHBox,
               "'2*sqrt(2)' drops the '*'");
        expect(h != nullptr && h->width == 3 * 8, "'2*sqrt(2)' width = 3 chars");

        // Anti-regression for the is_call() relaxation: a bare radicand must
        // still count as "simple" so it stacks as a fraction numerator.
        auto* f = build("sqrt(2) / 2");
        expect(f != nullptr && f->type == NodeType::kFraction &&
                   f->bin.a->type == NodeType::kHBox &&
                   f->bin.a->h.items[0]->t.text[0] == gfx::kGlyphSqrt,
               "'sqrt(2) / 2' stacks with a radical numerator");
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

    // Nested superscripts must actually step upward (HW 2026-08-08:
    // `2^2^2^2` drew as "222^2"). render_node places the exponent flush with
    // the node's top and the base at (node->baseline - base->baseline), so
    // the raise a level actually achieves is node->baseline - exp->baseline.
    // Sizing the baseline off the base cancelled that to zero as soon as the
    // exponent was itself a superscript.
    {
        const NodeType kSup = NodeType::kSuperscript;
        const render::LayoutNode* a = build("2^2");
        expect(a != nullptr && a->type == kSup, "'2^2' is a superscript");
        // Flat case must be unchanged by the fix.
        expect(a != nullptr && a->baseline - a->bin.b->baseline == a->bin.a->height / 2,
               "'2^2' raises the exponent by half the base height");

        const render::LayoutNode* b = build("2^2^2");
        expect(b != nullptr && b->type == kSup && b->bin.b->type == kSup,
               "'2^2^2' nests right-associatively");
        expect(b != nullptr && b->baseline - b->bin.b->baseline == b->bin.a->height / 2,
               "'2^2^2' outer level still raises its exponent");
        expect(b != nullptr && b->height > b->bin.b->height,
               "'2^2^2' is taller than its own exponent");

        const render::LayoutNode* c = build("2^2^2^2");
        expect(c != nullptr && c->baseline - c->bin.b->baseline == c->bin.a->height / 2,
               "'2^2^2^2' outer level still raises its exponent");
        // Each level steps up by the same amount, so heights strictly grow.
        expect(c != nullptr && c->height > c->bin.b->height &&
                   c->bin.b->height > c->bin.b->bin.b->height,
               "'2^2^2^2' heights grow at every level");
        // The base must never be asked to draw above the node's top.
        expect(c != nullptr && c->baseline >= c->bin.a->baseline,
               "'2^2^2^2' base fits inside the node box");
    }

    // Robustness: empty string
    expect(build("") == nullptr, "empty string -> nullptr");

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
