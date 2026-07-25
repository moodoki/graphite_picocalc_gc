#include "math/named_lists.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iterator>

#include "math/catalog.hpp"

namespace math {

namespace {

// Identifiers the engine/list layers already own that the catalog and
// constants tables don't cover.
const char* const kReserved[] = {
    "ans",  "theta", "pi", "matans", "std", "lopa", "lopb", "lopc",
    "lopd", "u1",    "u2", "v1",     "v2",  "w1",   "w2",
};

}  // namespace

int NamedLists::find(const char* name) const {
    for (int i = 0; i < kMax; ++i) {
        if (used_[i] && std::strcmp(names_[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

int NamedLists::create(const char* name) {
    if (!valid_name(name) || find(name) >= 0) {
        return -1;
    }
    for (int i = 0; i < kMax; ++i) {
        if (!used_[i]) {
            used_[i] = true;
            loaded_[i] = true;  // Fresh in-session entry: nothing to load
            std::snprintf(names_[i], sizeof(names_[i]), "%s", name);
            lists_[i].clear();
            lists_[i].set_dtype(Dtype::kDouble);
            return i;
        }
    }
    return -1;  // Full
}

bool NamedLists::remove(int idx) {
    if (!used(idx)) {
        return false;
    }
    lists_[idx].clear();
    lists_[idx].set_dtype(Dtype::kDouble);
    names_[idx][0] = 0;
    used_[idx] = false;
    return true;
}

bool NamedLists::rename(int idx, const char* new_name) {
    if (!used(idx) || !valid_name(new_name) || find(new_name) >= 0) {
        return false;
    }
    std::snprintf(names_[idx], sizeof(names_[idx]), "%s", new_name);
    return true;
}

int NamedLists::count() const {
    int n = 0;
    for (const bool u : used_) {
        n += u ? 1 : 0;
    }
    return n;
}

int NamedLists::nth(int n) const {
    for (int i = 0; i < kMax; ++i) {
        if (used_[i] && n-- == 0) {
            return i;
        }
    }
    return -1;
}

bool NamedLists::valid_name(const char* name) {
    const size_t len = std::strlen(name);
    if (len < 2 || len > kMaxName) {
        return false;  // Single letters are engine variables
    }
    if (std::islower(static_cast<unsigned char>(name[0])) == 0) {
        return false;
    }
    for (size_t i = 1; i < len; ++i) {
        const auto c = static_cast<unsigned char>(name[i]);
        if (std::islower(c) == 0 && std::isdigit(c) == 0) {
            return false;
        }
    }
    if (name[0] == 'l' && std::isdigit(static_cast<unsigned char>(name[1])) != 0) {
        return false;  // The l1..l6 namespace (and l7.. reads as it)
    }
    int n = 0;
    const FnDescriptor* cat = catalog(&n);
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(cat[i].name, name) == 0) {
            return false;
        }
    }
    const ConstDescriptor* cs = constants(&n);
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(cs[i].name, name) == 0) {
            return false;
        }
    }
    return std::all_of(std::begin(kReserved), std::end(kReserved),
                       [name](const char* r) { return std::strcmp(r, name) != 0; });
}

NamedLists& named_lists() {
    static NamedLists instance;
    return instance;
}

Array& list_by_ref(int ref) {
    if (ref >= kNamedRefBase) {
        return named_lists().list(ref - kNamedRefBase);
    }
    return lists().list(ref);
}

void list_ref_name(int ref, char* buf, int cap) {
    if (ref >= kNamedRefBase) {
        std::snprintf(buf, static_cast<size_t>(cap), "%s", named_lists().name(ref - kNamedRefBase));
    } else {
        std::snprintf(buf, static_cast<size_t>(cap), "l%d", ref + 1);
    }
}

}  // namespace math
