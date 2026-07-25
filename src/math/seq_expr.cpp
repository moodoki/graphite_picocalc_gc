#include "math/seq_expr.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>

#include "math/engine.hpp"

namespace math::seqexpr {

namespace {

constexpr size_t kMaxSrc = 256;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr int kNSlot = 'n' - 'a';

// Per-sequence compile products + reference flags from the rewrite.
struct SeqState {
    char src[kMaxSrc] = {};        // Definition text as last compiled ("" = none)
    void* handle = nullptr;        // Engine::compile_with result
    bool refs[kSeqCount][2] = {};  // [target seq][lag-1 / lag-2]
    bool refs_n = false;           // Bare n outside a lag reference
};

SeqState g_seq[kSeqCount];
double g_seed1[kSeqCount];
double g_seed2[kSeqCount];
long g_nmin = 1;
bool g_any = false;      // Any sequence compiled
bool g_any_lag = false;  // Any sequence references any lag

// Lag placeholder storage, bound by name via Engine::compile_with:
// g_lag[t][0] = value at n-1 of sequence t, [1] = at n-2.
double g_lag[kSeqCount][2];
const char* const kLagNames[kSeqCount][2] = {{"u1", "u2"}, {"v1", "v2"}, {"w1", "w2"}};

// Lockstep iterator: cur_[s] = value at cur_n_, prev_[s] at cur_n_-1.
long g_cur_n = 0;
double g_cur[kSeqCount];
double g_prev[kSeqCount];
bool g_primed = false;

bool ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Rewrite u/v/w(n-1|n-2) into the lag placeholders, recording which
// references each sequence makes. False on a malformed lag reference
// (anything inside the parens other than n-1/n-2, e.g. circular u(n)).
bool rewrite_lags(const char* in, char* out, size_t cap, SeqState& st) {
    size_t o = 0;
    const char* p = in;
    while (*p != 0) {
        const char c = *p;
        const bool seq_ref =
            (c == 'u' || c == 'v' || c == 'w') && p[1] == '(' && (p == in || !ident_char(p[-1]));
        if (!seq_ref) {
            if (c == 'n' && (p == in || !ident_char(p[-1])) && !ident_char(p[1])) {
                st.refs_n = true;
            }
            if (o + 1 >= cap) {
                return false;
            }
            out[o++] = c;
            ++p;
            continue;
        }
        // Collect the argument span (no nesting inside a lag ref).
        const int target = c == 'u' ? 0 : c == 'v' ? 1 : 2;
        const char* q = p + 2;
        char arg[8];
        size_t a = 0;
        while (*q != 0 && *q != ')') {
            if (*q != ' ') {
                if (a + 1 >= sizeof(arg)) {
                    return false;
                }
                arg[a++] = *q;
            }
            ++q;
        }
        arg[a] = 0;
        int lag = 0;
        if (std::strcmp(arg, "n-1") == 0) {
            lag = 1;
        } else if (std::strcmp(arg, "n-2") == 0) {
            lag = 2;
        } else {
            return false;  // u(n), u(3), u(n-3), ... — not a v1 form
        }
        if (*q != ')') {
            return false;
        }
        st.refs[target][lag - 1] = true;
        const char* name = kLagNames[target][lag - 1];
        if (o + std::strlen(name) >= cap) {
            return false;
        }
        std::memcpy(out + o, name, std::strlen(name));
        o += std::strlen(name);
        p = q + 1;
    }
    if (o >= cap) {
        return false;
    }
    out[o] = 0;
    return true;
}

bool has_lag(const SeqState& st) {
    return std::any_of(std::begin(st.refs), std::end(st.refs),
                       [](const bool (&r)[2]) { return r[0] || r[1]; });
}

void free_seq(SeqState& st) {
    if (st.handle != nullptr) {
        engine().free_compiled(st.handle);
        st.handle = nullptr;
    }
    st.src[0] = 0;
    std::memset(st.refs, 0, sizeof(st.refs));
    st.refs_n = false;
}

// Evaluate sequence s's compiled expression at n with the current lag
// bindings (the caller sets g_lag first).
double eval_at(int s, long n) {
    engine().vars().vars[kNSlot] = static_cast<double>(n);
    return engine().eval_compiled_raw(g_seq[s].handle);
}

// Compute all three sequences' values for step n into out[], reading
// lags from (cur, prev) — the values at n-1 and n-2.
void compute_step(long n, const double* cur, const double* prev, double* out) {
    for (int t = 0; t < kSeqCount; ++t) {
        g_lag[t][0] = cur[t];
        g_lag[t][1] = prev[t];
    }
    for (int s = 0; s < kSeqCount; ++s) {
        if (g_seq[s].handle == nullptr) {
            out[s] = kNaN;
        } else if (n == g_nmin && has_lag(g_seq[s])) {
            out[s] = g_seed1[s];
        } else if (n == g_nmin + 1 && uses_lag2(s)) {
            out[s] = g_seed2[s];
        } else {
            out[s] = eval_at(s, n);
        }
    }
}

void restart() {
    const double nan3[kSeqCount] = {kNaN, kNaN, kNaN};
    for (double& v : g_prev) {
        v = kNaN;
    }
    compute_step(g_nmin, nan3, nan3, g_cur);
    g_cur_n = g_nmin;
    g_primed = true;
}

void advance() {
    double next[kSeqCount];
    compute_step(g_cur_n + 1, g_cur, g_prev, next);
    for (int s = 0; s < kSeqCount; ++s) {
        g_prev[s] = g_cur[s];
        g_cur[s] = next[s];
    }
    ++g_cur_n;
}

}  // namespace

bool begin(const SeqDef& def) {
    bool changed = def.n_min != g_nmin;
    for (int s = 0; s < kSeqCount; ++s) {
        const char* txt = def.expr[s] != nullptr ? def.expr[s] : "";
        changed = changed || std::strcmp(txt, g_seq[s].src) != 0 || def.seed1[s] != g_seed1[s] ||
                  def.seed2[s] != g_seed2[s];
    }
    if (!changed) {
        return g_any;
    }

    g_nmin = def.n_min;
    g_any = false;
    g_any_lag = false;
    g_primed = false;
    for (int s = 0; s < kSeqCount; ++s) {
        free_seq(g_seq[s]);
        g_seed1[s] = def.seed1[s];
        g_seed2[s] = def.seed2[s];
        const char* txt = def.expr[s] != nullptr ? def.expr[s] : "";
        if (txt[0] == 0 || std::strlen(txt) >= kMaxSrc) {
            continue;
        }
        char rewritten[kMaxSrc];
        if (!rewrite_lags(txt, rewritten, sizeof(rewritten), g_seq[s])) {
            free_seq(g_seq[s]);
            continue;
        }
        Engine::ExtraVar extras[kSeqCount * 2];
        int ne = 0;
        for (int t = 0; t < kSeqCount; ++t) {
            extras[ne++] = {kLagNames[t][0], &g_lag[t][0]};
            extras[ne++] = {kLagNames[t][1], &g_lag[t][1]};
        }
        g_seq[s].handle = engine().compile_with(rewritten, extras, ne, kNSlot);
        if (g_seq[s].handle == nullptr) {
            free_seq(g_seq[s]);
            continue;
        }
        std::strncpy(g_seq[s].src, txt, kMaxSrc - 1);
        g_seq[s].src[kMaxSrc - 1] = 0;
        g_any = true;
        g_any_lag = g_any_lag || has_lag(g_seq[s]);
    }
    return g_any;
}

void refresh() {
    g_primed = false;
}

bool defined(int s) {
    return s >= 0 && s < kSeqCount && g_seq[s].handle != nullptr;
}

bool uses_lag2(int s) {
    if (s < 0 || s >= kSeqCount) {
        return false;
    }
    return std::any_of(std::begin(g_seq[s].refs), std::end(g_seq[s].refs),
                       [](const bool (&r)[2]) { return r[1]; });
}

bool lag1_only(int s) {
    if (!defined(s) || g_seq[s].refs_n) {
        return false;
    }
    for (int t = 0; t < kSeqCount; ++t) {
        if (g_seq[s].refs[t][1] || (t != s && g_seq[s].refs[t][0])) {
            return false;
        }
    }
    return g_seq[s].refs[s][0];
}

double value(int s, long n) {
    if (!defined(s) || n < g_nmin || n > g_nmin + kMaxN) {
        return kNaN;
    }
    if (!g_any_lag) {
        return eval_at(s, n);  // All explicit: no iteration needed
    }
    if (!g_primed || n < g_cur_n) {
        restart();
    }
    while (g_cur_n < n) {
        advance();
    }
    return g_cur[s];
}

double map_value(int s, double x) {
    if (!defined(s)) {
        return kNaN;
    }
    g_lag[s][0] = x;
    return engine().eval_compiled_raw(g_seq[s].handle);
}

}  // namespace math::seqexpr
