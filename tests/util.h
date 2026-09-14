/* Assertions for testing, dynamic checks, etc.
 *
 */
#pragma once

template <typename T>
bool same_elements(orq::Vector<T> x, orq::Vector<T> y) {
    std::sort(x.begin(), x.end(), std::greater<T>());
    std::sort(y.begin(), y.end(), std::greater<T>());
    return x.same_as(y);
}

#define _COUNT(x, y) (std::count((x).begin(), (x).end(), (y)))

#define ASSERT_CONTAINS(x, y) (assert(_COUNT((x), (y)) > 0))
#define REFUTE_CONTAINS(x, y) (assert(_COUNT((x), (y)) == 0))