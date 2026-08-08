#include "render/layout_builder.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "gfx/font.hpp"
#include "render/pool.hpp"

namespace render {

namespace {

// The radical glyph, used as the display name of the `sqrt` call (see
// parse_atom). Kept out of preprocess_glyphs so `sqrt(x)` still parses as
// a function call.
const char kSqrtGlyph[2] = {gfx::kGlyphSqrt, 0};

// Rewrite known identifiers/operators to the real math glyphs the main
// font bakes at high slots, before parsing (D24 pi; complex i, polar
// theta, and the store arrow added testdrive 2026-07-21). Doing it as a
// string pass — rather than per-atom — means the substitution also
// reaches the plain-text fallback path (e.g. "3+2i", where the implicit
// multiply stops the parser). Identifiers are matched whole, so 'i' in
// "sin" or "pi" in "pit" is untouched; a lone 'i' is always the
// imaginary unit. The output never grows (multi-char names collapse to
// one glyph), so out_len == input capacity is safe.
void preprocess_glyphs(const char* in, char* out, size_t out_len) {
    size_t o = 0;
    const auto put = [&](char c) {
        if (o + 1 < out_len) {
            out[o++] = c;
        }
    };
    for (const char* p = in; *p != 0;) {
        if (std::isalpha(static_cast<unsigned char>(*p)) != 0) {
            const char* start = p;
            while (std::isalnum(static_cast<unsigned char>(*p)) != 0) {
                ++p;
            }
            const auto len = static_cast<size_t>(p - start);
            if (len == 2 && std::memcmp(start, "pi", 2) == 0) {
                put(gfx::kGlyphPi);
            } else if (len == 1 && start[0] == 'i') {
                put(gfx::kGlyphImagI);
            } else if (len == 5 && std::memcmp(start, "theta", 5) == 0) {
                put(gfx::kGlyphTheta);
            } else {
                for (size_t i = 0; i < len; ++i) {
                    put(start[i]);
                }
            }
        } else if (p[0] == '-' && p[1] == '>') {
            put(gfx::kGlyphStore);
            p += 2;
        } else {
            put(*p++);
        }
    }
    out[o < out_len ? o : out_len - 1] = 0;
}

// ---- Node factories (compute size at construction) ----

LayoutNode* make_text(const char* s, int len, const Metrics& m) {
    auto* n = pool_new<LayoutNode>();
    if (n == nullptr) {
        return nullptr;
    }
    n->type = NodeType::kText;
    len = std::min(len, LayoutNode::kMaxText - 1);
    std::memcpy(n->t.text, s, static_cast<size_t>(len));
    n->t.text[len] = 0;
    n->width = len * m.char_w;
    n->height = m.char_h;
    n->baseline = m.char_h;
    return n;
}

// A function call parses to HBox[name, paren-args]. Recognize that shape
// structurally — the name check keeps unary-minus HBoxes ("-(x)") out.
bool is_call(const LayoutNode* n) {
    // name(args): a Text name followed by a Paren. The name is normally
    // alphabetic, but sqrt renders as the single radical glyph.
    if (n->type != NodeType::kHBox || n->h.count != 2 || n->h.items[0]->type != NodeType::kText) {
        return false;
    }
    if (n->h.items[0]->t.text[0] == gfx::kGlyphSqrt) {
        // Radicals also take the bare-radicand shape (Stage 4): "√2" is
        // HBox[radical, Text] rather than HBox[radical, Paren]. Keeping it a
        // "call" is what lets sqrt(2)/2 still stack as a fraction.
        return n->h.items[1]->type == NodeType::kParen || n->h.items[1]->type == NodeType::kText;
    }
    return std::isalpha(static_cast<unsigned char>(n->h.items[0]->t.text[0])) != 0 &&
           n->h.items[1]->type == NodeType::kParen;
}

// A radical call, in either the parenthesized or bare-radicand shape.
bool is_radical(const LayoutNode* n) {
    return is_call(n) && n->h.items[0]->t.text[0] == gfx::kGlyphSqrt;
}

// A single-glyph atom that reads as a symbol rather than a value — pi,
// theta and the other Greek letters. A coefficient multiplies these
// implicitly ("2pi" renders as 2 followed by the glyph, no '*'), which is
// how they are written by hand. Deliberately excludes the operator-ish
// glyphs (store arrow, not-equal, ellipsis) and the radical, which
// is_radical() covers in its own right.
bool is_symbol_glyph(const LayoutNode* n) {
    if (n == nullptr || n->type != NodeType::kText || n->t.text[0] == 0 || n->t.text[1] != 0) {
        return false;
    }
    const char c = n->t.text[0];
    return c == gfx::kGlyphPi || c == gfx::kGlyphTheta || c == gfx::kGlyphSigmaLower ||
           c == gfx::kGlyphSigmaUpper || c == gfx::kGlyphChi || c == gfx::kGlyphMu ||
           c == gfx::kGlyphImagI || c == gfx::kGlyphLambda;
}

bool is_simple(const LayoutNode* n) {
    // Function calls and powers count as simple so 1/sqrt(2) and x^2/2
    // stack (D2, revised on 2026-07-11 test-drive feedback).
    return n != nullptr &&
           (n->type == NodeType::kText || n->type == NodeType::kParen ||
            n->type == NodeType::kFraction || n->type == NodeType::kSuperscript || is_call(n));
}

LayoutNode* make_fraction(LayoutNode* num, LayoutNode* den, const Metrics& m) {
    if (num == nullptr || den == nullptr) {
        return num != nullptr ? num : den;
    }
    auto* n = pool_new<LayoutNode>();
    if (n == nullptr) {
        return num;  // Degrade gracefully
    }
    n->type = NodeType::kFraction;
    n->bin.a = num;
    n->bin.b = den;
    const int wider = num->width > den->width ? num->width : den->width;
    n->width = wider + 4;
    n->height = num->height + den->height + 3;  // bar + two 1px gaps
    // The bar centers on the midline of baseline-aligned text siblings
    // (4D.5; a text node's baseline is its bottom row, so the old
    // "bar row = baseline" hung the whole stack half a line too low).
    n->baseline = num->height + 1 + m.char_h / 2;
    return n;
}

LayoutNode* make_super(LayoutNode* base, LayoutNode* exp) {
    if (base == nullptr || exp == nullptr) {
        return base != nullptr ? base : exp;
    }
    auto* n = pool_new<LayoutNode>();
    if (n == nullptr) {
        return base;
    }
    n->type = NodeType::kSuperscript;
    n->bin.a = base;
    n->bin.b = exp;
    const int raise = base->height / 2;
    n->width = base->width + exp->width;

    // The renderer draws the exponent flush with the node's *top* and the
    // base at (node->baseline - base->baseline), so the raise actually
    // achieved is node->baseline - exp->baseline. Sizing the baseline off
    // the *base* instead only works while the exponent is plain text: for a
    // nested superscript (`2^2^2`), exp->baseline exceeds base->baseline by
    // exactly the inner raise, cancelling it to zero — every level of a power
    // tower landed on one line and `2^2^2^2` drew as "222^2" (HW 2026-08-08).
    //
    // So derive the baseline from the exponent, and keep it at least
    // base->baseline so the base never has to draw above the node's top.
    const int want = raise + exp->baseline;
    n->baseline = want > base->baseline ? want : base->baseline;
    const int base_bottom = n->baseline - base->baseline + base->height;
    n->height = exp->height > base_bottom ? exp->height : base_bottom;
    return n;
}

LayoutNode* make_paren(LayoutNode* child, const Metrics& m) {
    // A null child means the pool ran dry further down; propagate the
    // failure rather than dereferencing it. Every make_* below takes the
    // same line — build_layout's callers already handle a null result by
    // falling back to plain text.
    if (child == nullptr) {
        return nullptr;
    }
    auto* n = pool_new<LayoutNode>();
    if (n == nullptr) {
        return child;
    }
    n->type = NodeType::kParen;
    n->paren.child = child;
    n->width = child->width + 2 * m.char_w;
    n->height = child->height;
    n->baseline = child->baseline;
    return n;
}

// Wrap a run of children in an HBox (baseline-aligned). If there is only
// one child, return it directly to keep trees small.
LayoutNode* make_hbox(LayoutNode** items, int count) {
    if (count == 1) {
        return items[0];
    }
    auto* n = pool_new<LayoutNode>();
    if (n == nullptr) {
        return count > 0 ? items[0] : nullptr;
    }
    n->type = NodeType::kHBox;
    n->h.count = 0;
    int ascent = 0;
    int descent = 0;
    int width = 0;
    for (int i = 0; i < count && i < LayoutNode::kMaxChildren; ++i) {
        if (items[i] == nullptr) {
            continue;  // Pool ran dry building this child — skip, don't deref.
        }
        n->h.items[n->h.count++] = items[i];
        ascent = std::max(items[i]->baseline, ascent);
        const int d = items[i]->height - items[i]->baseline;
        descent = std::max(d, descent);
        width += items[i]->width;
    }
    n->width = width;
    n->height = ascent + descent;
    n->baseline = ascent;
    return n;
}

// ---- Recursive-descent parser ----

// Per-level staging for the flat runs parse_expr/parse_term collect before
// handing them to make_hbox. These arrays used to be stack locals — 128 B
// each, paid at every recursion level, inside a build that runs from
// HomeScreen::render() (D47). They live at the pool's scratch end now.
//
// Failure is graceful by construction: cap 0 means "no staging", and both
// callers already guard every append on `count < cap`, so the result is a
// shorter hbox rather than a bad write.
struct Stage {
    std::size_t mark;
    LayoutNode** items;
    int cap;

    Stage()
        : mark(pool_scratch_mark()),
          items(static_cast<LayoutNode**>(pool_scratch_alloc(
              sizeof(LayoutNode*) * LayoutNode::kMaxChildren, alignof(LayoutNode*)))),
          cap(items != nullptr ? LayoutNode::kMaxChildren : 0) {}
    ~Stage() { pool_scratch_release(mark); }
    Stage(const Stage&) = delete;
    Stage& operator=(const Stage&) = delete;
    Stage(Stage&&) = delete;
    Stage& operator=(Stage&&) = delete;
};

// Hard cap on parser nesting, the backstop behind the staging move above.
// Even at ~96 B a level (down from 376 before the staging moved into the
// pool) the stack is finite, and build_layout is called from the deepest
// point in the program; past this the tail renders as an ellipsis instead
// of risking the overrun that faulted the board at four nested parens
// (HW 2026-08-08).
constexpr int kMaxParseDepth = 16;

struct Parser {
    const char* p = nullptr;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members) short-lived parser
    const Metrics& m;
    int depth = 0;

    // Both recursion cycles pass through parse_power, so one guard there
    // covers both: paren/function nesting (parse_atom -> parse_expr -> ... ->
    // parse_power) and the right-associative '^' chain (parse_power calling
    // itself, with no parentheses involved).
    struct Depth {
        Parser& parser;
        explicit Depth(Parser& q) : parser(q) { ++parser.depth; }
        ~Depth() { --parser.depth; }
        Depth(const Depth&) = delete;
        Depth& operator=(const Depth&) = delete;
        Depth(Depth&&) = delete;
        Depth& operator=(Depth&&) = delete;
        bool too_deep() const { return parser.depth > kMaxParseDepth; }
    };

    void skip_spaces() {
        while (*p == ' ') {
            ++p;
        }
    }

    // number | identifier[(...)] | (expr) | function-call
    LayoutNode* parse_atom() {
        skip_spaces();
        const char* start = p;

        if (*p == '(') {
            ++p;
            LayoutNode* inner = parse_expr();
            skip_spaces();
            if (*p == ')') {
                ++p;
            }
            return make_paren(inner, m);
        }

        if (std::isdigit(static_cast<unsigned char>(*p)) != 0 || *p == '.') {
            while (std::isdigit(static_cast<unsigned char>(*p)) != 0 || *p == '.') {
                ++p;
            }
            // Scientific-notation suffix (1e10, 2.5e-3): consume only
            // when a (optionally signed) digit follows the e/E, so a
            // bare Euler's-e after a number ("2e") stays an identifier.
            if (*p == 'e' || *p == 'E') {
                const char* q = p + 1;
                if (*q == '+' || *q == '-') {
                    ++q;
                }
                if (std::isdigit(static_cast<unsigned char>(*q)) != 0) {
                    p = q;
                    while (std::isdigit(static_cast<unsigned char>(*p)) != 0) {
                        ++p;
                    }
                }
            }
            return make_text(start, static_cast<int>(p - start), m);
        }

        if (std::isalpha(static_cast<unsigned char>(*p)) != 0) {
            while (std::isalnum(static_cast<unsigned char>(*p)) != 0) {
                ++p;
            }
            const int ident_len = static_cast<int>(p - start);
            // Most known-name glyph substitution (pi/i/theta) happens up
            // front in preprocess_glyphs(). `sqrt` is special: it's a
            // function, so it stays an identifier here (the call parsing
            // below keeps the "(args)") but its name renders as the radical
            // glyph — an inline "√(x)". (A big radical over the argument is
            // KIV — testdrive 2026-07-21.)
            const bool is_sqrt = ident_len == 4 && std::memcmp(start, "sqrt", 4) == 0;
            LayoutNode* name =
                is_sqrt ? make_text(kSqrtGlyph, 1, m) : make_text(start, ident_len, m);
            skip_spaces();
            if (*p == '(') {  // Function call: name followed by (args)
                ++p;
                skip_spaces();
                // Empty arg list (rand()): parse_expr would consume the
                // ')' as a stray-punctuation text node and make_paren
                // would then add its own — "rand())".
                LayoutNode* args = *p == ')' ? make_text("", 0, m) : parse_expr();
                skip_spaces();
                if (*p == ')') {
                    ++p;
                }
                skip_spaces();
                // Bare radicand (Stage 4): "√2", not "√(2)". Only when the
                // argument is a single atom, and never when a '^' follows —
                // there is no vinculum, so the parens are the only grouping
                // and "√x^2" must not read as sqrt(x^2).
                const bool bare = is_sqrt && args->type == NodeType::kText && *p != '^';
                LayoutNode* group = bare ? args : make_paren(args, m);
                LayoutNode* pair[2] = {name, group};
                return make_hbox(pair, 2);
            }
            return name;
        }

        // Lone operator / punctuation (e.g. a comma inside args): emit as
        // a one-char text node so it still renders.
        if (*p != 0) {
            ++p;
            return make_text(start, 1, m);
        }
        return make_text("", 0, m);
    }

    // atom ('^' power)?  — right-associative superscript
    LayoutNode* parse_power() {
        const Depth guard(*this);
        if (guard.too_deep()) {
            // Consume the rest so no enclosing loop can spin on unparsed
            // input, and show an ellipsis in place of the truncated tail.
            while (*p != 0) {
                ++p;
            }
            const char ell[2] = {static_cast<char>(gfx::kGlyphEllipsis), 0};
            return make_text(ell, 1, m);
        }
        LayoutNode* base = parse_atom();
        skip_spaces();
        if (*p == '^') {
            ++p;
            LayoutNode* exp = parse_power();
            return make_super(base, exp);
        }
        return base;
    }

    // Optional leading unary minus.
    LayoutNode* parse_unary() {
        skip_spaces();
        if (*p == '-') {
            ++p;
            LayoutNode* rhs = parse_power();
            LayoutNode* minus = make_text("-", 1, m);
            LayoutNode* pair[2] = {minus, rhs};
            return make_hbox(pair, 2);
        }
        return parse_power();
    }

    // power (('*'|'/') power)* — '/' builds a fraction when both sides
    // are "simple", else inline (D2).
    LayoutNode* parse_term() {
        LayoutNode* cur = parse_unary();
        skip_spaces();
        if (*p != '*' && *p != '/') {
            return cur;  // Single factor — no staging needed at all.
        }
        // Staging is acquired lazily and only here. A deeply nested
        // expression is one factor per level, so it now costs no scratch;
        // reserving eagerly meant 16 levels claimed 4 KB of the 8 KB pool
        // and starved the nodes (caught by test_layout, 2026-08-08).
        const Stage stage;
        LayoutNode** const items = stage.items;
        const int cap = stage.cap;
        int count = 0;
        while (true) {
            skip_spaces();
            const char op = *p;
            if (op != '*' && op != '/') {
                break;
            }
            ++p;
            LayoutNode* rhs = parse_unary();
            if (op == '/' && is_simple(cur) && is_simple(rhs) && count == 0) {
                cur = make_fraction(cur, rhs, m);
            } else {
                if (count == 0 && cap > 0) {
                    items[count++] = cur;
                }
                // Implicit multiplication before a radical or a symbol glyph
                // (Stage 4): "2*sqrt(2)" renders "2√2" and "2*pi" renders
                // "2π", the way they are written by hand.
                const bool implicit = op == '*' && (is_radical(rhs) || is_symbol_glyph(rhs));
                if (!implicit && count < cap) {
                    items[count++] = make_text(op == '*' ? "*" : "/", 1, m);
                }
                if (count < cap) {
                    items[count++] = rhs;
                }
            }
        }
        if (count == 0) {
            return cur;
        }
        return make_hbox(items, count);
    }

    // term (('+'|'-') term)*
    LayoutNode* parse_expr() {
        LayoutNode* first = parse_term();
        skip_spaces();
        if ((*p != '+' && *p != '-')) {
            return first;  // Single term — no staging needed.
        }
        const Stage stage;
        LayoutNode** const items = stage.items;
        const int cap = stage.cap;
        if (cap == 0) {
            return first;  // Pool exhausted; show what we have.
        }
        int count = 0;
        items[count++] = first;
        while (true) {
            skip_spaces();
            const char op = *p;
            if (op != '+' && op != '-') {
                break;
            }
            ++p;
            if (count < cap) {
                items[count++] = make_text(op == '+' ? "+" : "-", 1, m);
            }
            LayoutNode* rhs = parse_term();
            if (count < cap) {
                items[count++] = rhs;
            }
        }
        return make_hbox(items, count);
    }
};

}  // namespace

LayoutNode* build_layout(const char* expr, const Metrics& m) {
    pool_reset();
    if (expr == nullptr || *expr == 0) {
        return nullptr;
    }
    // Substitute the math glyphs first, then parse/fall back on the
    // rewritten string so glyphs show in both cases (testdrive 2026-07-21).
    char pre[256];
    preprocess_glyphs(expr, pre, sizeof(pre));
    Parser parser{pre, m};
    LayoutNode* root = parser.parse_expr();
    parser.skip_spaces();
    if (root == nullptr || *parser.p != 0) {
        // Pool exhaustion, or the grammar stopped before the end of the
        // input (e.g. store "2->A", multi-arg calls): render the whole
        // string as plain text rather than a silently-truncated tree
        // (HW-found 2026-07-11: "1e10" displayed as just "1").
        pool_reset();
        root = make_text(pre, static_cast<int>(std::strlen(pre)), m);
    }
    return root;
}

}  // namespace render
