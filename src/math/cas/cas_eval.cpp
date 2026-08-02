#include "math/cas/cas_eval.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iterator>

#include "math/cas/derivative.hpp"
#include "math/cas/expand.hpp"
#include "math/cas/factor.hpp"
#include "math/cas/integrate.hpp"
#include "math/cas/parser.hpp"
#include "math/cas/simplify.hpp"
#include "math/cas/solve.hpp"

namespace math::cas {

namespace {

constexpr int kMaxArgs = 4;

// The recognized op names, longest-first is not required (we match the exact
// identifier before '('). Kept as a table so the set is one place.
const char* const kOps[] = {"simplify", "expand", "factor", "diff", "integ", "solve"};

// Trim leading/trailing ASCII spaces; returns a pointer into `s` and writes
// the trimmed length to *len.
const char* trim(const char* s, std::size_t* len) {
    while (*s == ' ') {
        ++s;
    }
    std::size_t n = std::strlen(s);
    while (n > 0 && s[n - 1] == ' ') {
        --n;
    }
    *len = n;
    return s;
}

bool is_known_op(const char* name, std::size_t name_len) {
    return std::any_of(std::begin(kOps), std::end(kOps), [&](const char* op) {
        return std::strlen(op) == name_len && std::memcmp(op, name, name_len) == 0;
    });
}

// Split the (already inner) argument string at top-level commas — commas
// inside nested parens are not separators. Writes up to kMaxArgs NUL-terminated
// pieces into arg_buf[i] (each capacity kArgCap) and returns the count, or -1
// if there are too many args or one is too long.
constexpr std::size_t kArgCap = 96;
int split_args(const char* inner, char arg_buf[kMaxArgs][kArgCap]) {
    int count = 0;
    int depth = 0;
    std::size_t out = 0;
    if (count >= kMaxArgs) {
        return -1;
    }
    for (const char* p = inner;; ++p) {
        const char c = *p;
        if (c == '(') {
            ++depth;
        } else if (c == ')') {
            --depth;
        }
        if ((c == ',' && depth == 0) || c == '\0') {
            arg_buf[count][out < kArgCap ? out : kArgCap - 1] = '\0';
            ++count;
            out = 0;
            if (c == '\0') {
                break;
            }
            if (count >= kMaxArgs) {
                return -1;
            }
            continue;
        }
        if (out + 1 >= kArgCap) {
            return -1;
        }
        arg_buf[count][out++] = c;
    }
    return count;
}

// A single-letter (a-z) argument names a variable; anything else is invalid.
bool arg_is_var(const char* arg, char* out_var) {
    std::size_t len = 0;
    const char* t = trim(arg, &len);
    if (len == 1 && std::islower(static_cast<unsigned char>(t[0])) != 0) {
        *out_var = t[0];
        return true;
    }
    return false;
}

// Parse an integer literal argument (for diff's order n). Returns false when
// the argument is not a plain non-negative integer.
bool arg_is_int(const char* arg, int* out_n) {
    std::size_t len = 0;
    const char* t = trim(arg, &len);
    if (len == 0) {
        return false;
    }
    int n = 0;
    for (std::size_t i = 0; i < len; ++i) {
        if (std::isdigit(static_cast<unsigned char>(t[i])) == 0) {
            return false;
        }
        n = n * 10 + (t[i] - '0');
    }
    *out_n = n;
    return true;
}

HomeResult make_error(const char* msg, const char* op, char var) {
    HomeResult r;
    r.kind = HomeKind::kError;
    r.error = msg;
    r.var = var;
    std::snprintf(r.op, sizeof(r.op), "%s", op);
    return r;
}

}  // namespace

HomeResult evaluate_home(const char* input, bool allow_complex) {
    HomeResult none;  // kNone by default

    std::size_t in_len = 0;
    const char* s = trim(input, &in_len);
    if (in_len == 0) {
        return none;
    }

    // Split "<name>(<inner>)": the whole input must be exactly one call.
    const char* open = static_cast<const char*>(std::memchr(s, '(', in_len));
    if (open == nullptr || s[in_len - 1] != ')') {
        return none;
    }
    const auto name_len = static_cast<std::size_t>(open - s);
    if (!is_known_op(s, name_len)) {
        return none;
    }
    char op[10] = {};
    std::memcpy(op, s, name_len < sizeof(op) - 1 ? name_len : sizeof(op) - 1);

    // Inner = between the first '(' and the final ')'.
    const char* inner_start = open + 1;
    const auto inner_len = static_cast<std::size_t>((s + in_len - 1) - inner_start);
    char inner[kArgCap * kMaxArgs];
    if (inner_len >= sizeof(inner)) {
        return make_error("Too long", op, 'x');
    }
    std::memcpy(inner, inner_start, inner_len);
    inner[inner_len] = '\0';

    static char args[kMaxArgs][kArgCap];
    const int argc = split_args(inner, args);
    if (argc < 1 || args[0][0] == '\0') {
        return make_error("Bad arguments", op, 'x');
    }

    // Shape-based solve() split (P5-4): a numeric guess/bounds (>= 3 args) is
    // the numeric solver's job — fall through so math::solveexpr handles it.
    if (std::strcmp(op, "solve") == 0 && argc >= 3) {
        return none;
    }

    // Variable argument (position 2 for most ops).
    char var = 'x';
    if (argc >= 2) {
        char v = 'x';
        if (arg_is_var(args[1], &v)) {
            var = v;
        } else if (std::strcmp(op, "integ") != 0) {
            // integ's 2nd arg is always the variable; for others a non-var
            // 2nd arg is only meaningful where documented (none in v1).
            return make_error("Bad variable", op, var);
        }
    }

    g_cas_pool.reset();

    const char* perr = nullptr;
    Expr* e = parse_expr(args[0], &perr);
    if (e == nullptr) {
        return make_error(perr != nullptr ? perr : "Parse error", op, var);
    }

    HomeResult r;
    r.var = var;
    std::snprintf(r.op, sizeof(r.op), "%s", op);

    if (std::strcmp(op, "simplify") == 0) {
        r.result = simplify(e);
    } else if (std::strcmp(op, "expand") == 0) {
        r.result = expand(e);
    } else if (std::strcmp(op, "factor") == 0) {
        r.result = factor(e, var);
    } else if (std::strcmp(op, "diff") == 0) {
        int n = 1;
        if (argc >= 3 && !arg_is_int(args[2], &n)) {
            return make_error("Bad order", op, var);
        }
        r.result = differentiate_n(e, var, n);
    } else if (std::strcmp(op, "integ") == 0) {
        if (argc == 4) {
            Expr* lo = parse_expr(args[2], &perr);
            Expr* hi = parse_expr(args[3], &perr);
            if (lo == nullptr || hi == nullptr) {
                return make_error("Bad bounds", op, var);
            }
            const DefIntResult di = definite_integrate(e, var, lo, hi);
            if (!di.has_numeric) {
                return make_error("Cannot integrate", op, var);
            }
            r.result = Expr::num(di.numeric_val);
        } else {
            r.result = integrate(e, var);
        }
    } else if (std::strcmp(op, "solve") == 0) {
        const SolveResult sr = solve(e, var, allow_complex);
        if (sr.count == 0) {
            return make_error(sr.complex ? "No real solutions" : "No solution", op, var);
        }
        r.kind = HomeKind::kSolutions;
        r.count = sr.count;
        r.complex = sr.complex;
        for (int i = 0; i < sr.count && i < 8; ++i) {
            r.solutions[i] = sr.solutions[i];
        }
        return r;
    }

    if (r.result == nullptr) {
        return make_error("Cannot evaluate", op, var);
    }
    r.kind = HomeKind::kExpr;
    return r;
}

}  // namespace math::cas
