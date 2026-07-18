#include "math/lists.hpp"

namespace math {

ListStore& lists() {
    static ListStore instance;
    return instance;
}

}  // namespace math
