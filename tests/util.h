/* Assertions for testing, dynamic checks, etc.
 *
 */
#pragma once

#include <bit>

#define ASSERT_SAME(x, y) {                                                    \
    if (! ((x) == (y))) {                                                      \
        if (secrecy::service::runTime->getPartyID() == 0) {                     \
            std::cerr << "assertion failed: " #x " != " #y << "\n";            \
            std::cerr << "values: (" << (long)(x) << ") != (" << (long)(y)     \
                      << ")\n";                                                \
        }                                                                      \
        assert((x) == (y));                                                    \
    }                                                                          \
}

constexpr bool is_power_of_two(uint64_t x) {
    return (1 << std::bit_width(x - 1)) == x;
}

template<typename T>
bool same_elements(secrecy::Vector<T> x, secrecy::Vector<T> y) {
    std::sort(x.begin(), x.end(), std::greater<T>());
    std::sort(y.begin(), y.end(), std::greater<T>());
    return x.same_as(y);
}

#define ASSERT_POWER_OF_TWO(x) {                                               \
    auto s = (x);                                                              \
    if (! is_power_of_two(x)) {                                                \
        std::cerr << "assertion failed: " #x " (= " << (s)                     \
                  << ") not power of two\n";                                   \
        assert(false);                                                         \
    }                                                                          \
}

#define _COUNT(x, y) (std::count((x).begin(), (x).end(), (y)))

#define ASSERT_CONTAINS(x, y) (assert(_COUNT((x), (y)) >  0))
#define REFUTE_CONTAINS(x, y) (assert(_COUNT((x), (y)) == 0))