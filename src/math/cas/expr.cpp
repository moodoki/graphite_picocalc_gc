#include "math/cas/expr.hpp"

#include <cstring>
#include <memory>

#include "math/scratch.hpp"

namespace math::cas {

ExprPool g_cas_pool;

std::size_t ExprPool::capacity() const {
    return math::scratch::kComputeBytes;
}

void* ExprPool::alloc_raw(std::size_t bytes, std::size_t align) {
    std::uint8_t* region = math::scratch::compute_region();
    void* cur = region + offset_;
    // The ceiling is the scratch end, not the arena end — held scratch arrays
    // are live memory. Without this the node end bumps straight through them
    // and a pass reads back overwritten Expr pointers.
    const std::size_t limit = math::scratch::kComputeBytes - scratch_used_;
    if (offset_ > limit) {
        overflow_ = true;
        return nullptr;
    }
    std::size_t space = limit - offset_;
    if (std::align(align, bytes, cur, space) == nullptr) {
        overflow_ = true;  // pool exhausted (spec §13 Risk 2 abort path)
        return nullptr;
    }
    offset_ = static_cast<std::size_t>(static_cast<std::uint8_t*>(cur) - region) + bytes;
    return cur;
}

Expr* ExprPool::alloc() {
    return static_cast<Expr*>(alloc_raw(sizeof(Expr), alignof(Expr)));
}

void* ExprPool::alloc_scratch(std::size_t bytes, std::size_t align) {
    // Grow down from the top. compute_region() is 16-byte aligned (see
    // scratch.cpp), so aligning the offset aligns the address.
    const std::size_t top = math::scratch::kComputeBytes - scratch_used_;
    if (top < bytes) {
        overflow_ = true;
        return nullptr;
    }
    const std::size_t base = (top - bytes) & ~(align - 1);
    if (base < offset_) {
        overflow_ = true;  // the two ends would cross
        return nullptr;
    }
    scratch_used_ = math::scratch::kComputeBytes - base;
    return math::scratch::compute_region() + base;
}

ScratchScope::ScratchScope() : mark_(g_cas_pool.scratch_mark()) {}

ScratchScope::~ScratchScope() {
    g_cas_pool.scratch_release(mark_);
}

void* ScratchScope::alloc(std::size_t bytes, std::size_t align) {
    return g_cas_pool.alloc_scratch(bytes, align);
}

// ---- Factories ----

Expr* Expr::num(calc_t val) {
    Expr* e = g_cas_pool.alloc();
    if (e == nullptr) {
        return nullptr;
    }
    e->type = ExprType::kNum;
    e->num_val = val;
    e->child = nullptr;
    e->next = nullptr;
    return e;
}

Expr* Expr::var(char name) {
    Expr* e = g_cas_pool.alloc();
    if (e == nullptr) {
        return nullptr;
    }
    e->type = ExprType::kVar;
    e->var_name = name;
    e->child = nullptr;
    e->next = nullptr;
    return e;
}

namespace {
// Build an n-ary ADD/MUL, flattening operands of the same type so
// (a+b)+c collapses to a single ADD with children a,b,c. Safe because the
// operands handed in are freshly built, unshared subtrees.
Expr* make_nary(ExprType type, Expr* a, Expr* b) {
    if (a == nullptr || b == nullptr) {
        return nullptr;
    }
    Expr* node = g_cas_pool.alloc();
    if (node == nullptr) {
        return nullptr;
    }
    node->type = type;
    node->child = nullptr;
    node->next = nullptr;

    Expr** tail = &node->child;
    Expr* operands[2] = {a, b};
    for (Expr* operand : operands) {
        if (operand->type == type) {
            Expr* c = operand->child;
            while (c != nullptr) {
                Expr* nx = c->next;
                *tail = c;
                c->next = nullptr;
                tail = &c->next;
                c = nx;
            }
        } else {
            *tail = operand;
            operand->next = nullptr;
            tail = &operand->next;
        }
    }
    return node;
}
}  // namespace

Expr* Expr::add(Expr* a, Expr* b) {
    return make_nary(ExprType::kAdd, a, b);
}

Expr* Expr::mul(Expr* a, Expr* b) {
    return make_nary(ExprType::kMul, a, b);
}

Expr* Expr::pow(Expr* base, Expr* exp) {
    if (base == nullptr || exp == nullptr) {
        return nullptr;
    }
    Expr* e = g_cas_pool.alloc();
    if (e == nullptr) {
        return nullptr;
    }
    e->type = ExprType::kPow;
    e->child = base;
    e->next = nullptr;
    base->next = exp;
    exp->next = nullptr;
    return e;
}

Expr* Expr::neg(Expr* a) {
    if (a == nullptr) {
        return nullptr;
    }
    Expr* e = g_cas_pool.alloc();
    if (e == nullptr) {
        return nullptr;
    }
    e->type = ExprType::kNeg;
    e->child = a;
    e->next = nullptr;
    a->next = nullptr;
    return e;
}

Expr* Expr::func(const char* name, Expr* arg) {
    Expr* e = g_cas_pool.alloc();
    if (e == nullptr) {
        return nullptr;
    }
    e->type = ExprType::kFunc;
    std::memset(e->func_name, 0, sizeof(e->func_name));
    std::strncpy(e->func_name, name, sizeof(e->func_name) - 1);
    e->child = arg;  // may be nullptr for a named constant (pi)
    e->next = nullptr;
    if (arg != nullptr) {
        arg->next = nullptr;
    }
    return e;
}

Expr* Expr::eq(Expr* lhs, Expr* rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        return nullptr;
    }
    Expr* e = g_cas_pool.alloc();
    if (e == nullptr) {
        return nullptr;
    }
    e->type = ExprType::kEq;
    e->child = lhs;
    e->next = nullptr;
    lhs->next = rhs;
    rhs->next = nullptr;
    return e;
}

// ---- Tree operations ----

int Expr::child_count() const {
    int n = 0;
    for (const Expr* c = child; c != nullptr; c = c->next) {
        ++n;
    }
    return n;
}

bool Expr::contains(char v) const {
    if (type == ExprType::kVar) {
        return var_name == v;
    }
    for (const Expr* c = child; c != nullptr; c = c->next) {
        if (c->contains(v)) {
            return true;
        }
    }
    return false;
}

bool Expr::equals(const Expr* other) const {
    if (other == nullptr || type != other->type) {
        return false;
    }
    switch (type) {
        case ExprType::kNum:
            if (num_val != other->num_val) {
                return false;
            }
            break;
        case ExprType::kVar:
            if (var_name != other->var_name) {
                return false;
            }
            break;
        case ExprType::kFunc:
            if (std::strncmp(func_name, other->func_name, sizeof(func_name)) != 0) {
                return false;
            }
            break;
        default:
            break;
    }
    const Expr* a = child;
    const Expr* b = other->child;
    while (a != nullptr && b != nullptr) {
        if (!a->equals(b)) {
            return false;
        }
        a = a->next;
        b = b->next;
    }
    return a == nullptr && b == nullptr;
}

Expr* Expr::clone() const {
    Expr* n = g_cas_pool.alloc();
    if (n == nullptr) {
        return nullptr;
    }
    n->type = type;
    switch (type) {
        case ExprType::kNum:
            n->num_val = num_val;
            break;
        case ExprType::kVar:
            n->var_name = var_name;
            break;
        case ExprType::kFunc:
            std::memcpy(n->func_name, func_name, sizeof(func_name));
            break;
        default:
            break;
    }
    n->child = nullptr;
    n->next = nullptr;
    Expr** tail = &n->child;
    for (const Expr* c = child; c != nullptr; c = c->next) {
        Expr* cc = c->clone();
        if (cc == nullptr) {
            return nullptr;
        }
        *tail = cc;
        cc->next = nullptr;
        tail = &cc->next;
    }
    return n;
}

}  // namespace math::cas
