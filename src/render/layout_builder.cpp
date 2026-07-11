#include "render/layout_builder.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "render/pool.hpp"

namespace render {

namespace {

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
    return n->type == NodeType::kHBox && n->h.count == 2 &&
           n->h.items[0]->type == NodeType::kText &&
           std::isalpha(static_cast<unsigned char>(n->h.items[0]->t.text[0])) != 0 &&
           n->h.items[1]->type == NodeType::kParen;
}

bool is_simple(const LayoutNode* n) {
    // Function calls and powers count as simple so 1/sqrt(2) and x^2/2
    // stack (D2, revised on 2026-07-11 test-drive feedback).
    return n != nullptr &&
           (n->type == NodeType::kText || n->type == NodeType::kParen ||
            n->type == NodeType::kFraction || n->type == NodeType::kSuperscript || is_call(n));
}

LayoutNode* make_fraction(LayoutNode* num, LayoutNode* den) {
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
    n->baseline = num->height + 1;              // bar row = math baseline
    return n;
}

LayoutNode* make_super(LayoutNode* base, LayoutNode* exp) {
    auto* n = pool_new<LayoutNode>();
    if (n == nullptr) {
        return base;
    }
    n->type = NodeType::kSuperscript;
    n->bin.a = base;
    n->bin.b = exp;
    const int raise = base->height / 2;
    n->width = base->width + exp->width;
    const int base_bottom = raise + base->height;
    n->height = exp->height > base_bottom ? exp->height : base_bottom;
    n->baseline = raise + base->baseline;
    return n;
}

LayoutNode* make_paren(LayoutNode* child, const Metrics& m) {
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

struct Parser {
    const char* p;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members) short-lived parser
    const Metrics& m;

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
            LayoutNode* name = make_text(start, static_cast<int>(p - start), m);
            skip_spaces();
            if (*p == '(') {  // Function call: name followed by (args)
                ++p;
                LayoutNode* args = parse_expr();
                skip_spaces();
                if (*p == ')') {
                    ++p;
                }
                LayoutNode* group = make_paren(args, m);
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
        LayoutNode* items[LayoutNode::kMaxChildren];
        int count = 0;
        LayoutNode* cur = parse_unary();
        while (true) {
            skip_spaces();
            const char op = *p;
            if (op != '*' && op != '/') {
                break;
            }
            ++p;
            LayoutNode* rhs = parse_unary();
            if (op == '/' && is_simple(cur) && is_simple(rhs) && count == 0) {
                cur = make_fraction(cur, rhs);
            } else {
                if (count == 0) {
                    items[count++] = cur;
                }
                if (count < LayoutNode::kMaxChildren) {
                    items[count++] = make_text(op == '*' ? "*" : "/", 1, m);
                }
                if (count < LayoutNode::kMaxChildren) {
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
        LayoutNode* items[LayoutNode::kMaxChildren];
        int count = 0;
        items[count++] = parse_term();
        while (true) {
            skip_spaces();
            const char op = *p;
            if (op != '+' && op != '-') {
                break;
            }
            ++p;
            if (count < LayoutNode::kMaxChildren) {
                items[count++] = make_text(op == '+' ? "+" : "-", 1, m);
            }
            LayoutNode* rhs = parse_term();
            if (count < LayoutNode::kMaxChildren) {
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
    Parser parser{expr, m};
    LayoutNode* root = parser.parse_expr();
    parser.skip_spaces();
    if (root == nullptr || *parser.p != 0) {
        // Pool exhaustion, or the grammar stopped before the end of the
        // input (e.g. store "2->A", multi-arg calls): render the whole
        // string as plain text rather than a silently-truncated tree
        // (HW-found 2026-07-11: "1e10" displayed as just "1").
        pool_reset();
        root = make_text(expr, static_cast<int>(std::strlen(expr)), m);
    }
    return root;
}

}  // namespace render
