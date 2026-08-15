#include "math/autoclose.hpp"

#include <cstring>

namespace math {

int close_open_parens(char* buf, std::size_t cap) {
    if (buf == nullptr || cap == 0) {
        return 0;
    }
    const std::size_t len = std::strlen(buf);

    // Find where the expression proper ends. A trailing display suffix
    // is matched the same way home_screen.cpp:476 matches it, so the
    // two cannot disagree about what counts as a suffix.
    std::size_t end = len;
    if (len > 5 && std::strcmp(buf + len - 5, ">frac") == 0) {
        end = len - 5;
    } else if (len > 4 && std::strcmp(buf + len - 4, ">dec") == 0) {
        end = len - 4;
    }
    // A store arrow, if present, comes before any suffix — so scanning
    // within the already-trimmed range picks the earlier boundary.
    for (std::size_t i = 0; i + 1 < end; ++i) {
        if (buf[i] == '-' && buf[i + 1] == '>') {
            end = i;
            break;
        }
    }

    int depth = 0;
    for (std::size_t i = 0; i < end; ++i) {
        if (buf[i] == '(') {
            ++depth;
        } else if (buf[i] == ')') {
            if (depth == 0) {
                return 0;  // over-closed: a real error, report it as one
            }
            --depth;
        }
    }
    if (depth <= 0) {
        return 0;
    }
    if (len + static_cast<std::size_t>(depth) + 1 > cap) {
        return 0;  // would not fit; let it error rather than truncate
    }

    // Shift the tail (store target / suffix / NUL) right and fill.
    std::memmove(buf + end + static_cast<std::size_t>(depth), buf + end, len - end + 1);
    for (int i = 0; i < depth; ++i) {
        buf[end + static_cast<std::size_t>(i)] = ')';
    }
    return depth;
}

}  // namespace math
