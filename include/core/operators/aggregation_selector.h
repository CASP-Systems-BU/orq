#pragma once

#include "core/operators/aggregation.h"

namespace orq::aggregators {

/**
 * @brief A helper type to store an aggregation of either AShared or BShared type.
 *
 * @tparam A
 * @tparam B
 */
template <typename A, typename B>
class AggregationSelector {
    using af_t = void (*)(const A &, A &, const A &);
    using bf_t = void (*)(const B &, B &, const B &);

   private:
    bf_t bFunc;
    af_t aFunc;

   public:
    AggregationSelector(bf_t bFunc) : bFunc(bFunc), aFunc(nullptr) {}

    AggregationSelector(af_t aFunc) : aFunc(aFunc), bFunc(nullptr) {}

    /**
     * @brief Get the AShared function. If this object does not store an AShared function,
     * abort.
     *
     * @return af_t
     */
    af_t getA() {
        if (aFunc == nullptr) {
            std::cerr << "Error: requested A func, but was null\n";
            exit(-1);
        }
        return aFunc;
    }

    /**
     * @brief Get the BShared function. If this object does not store a BShared function,
     * abort.
     *
     * @return bf_t
     */
    bf_t getB() {
        if (bFunc == nullptr) {
            std::cerr << "Error: requested B func, but was null\n";
            exit(-1);
        }

        return bFunc;
    }

    /**
     * @brief Check if the stored function is a copy aggregation or aggregation
     *
     * @return true the stored function is an aggregation (non-copy)
     * @return false the stored function is a copy function
     */
    bool isAggregation() { return !(aFunc == &copy<A> || bFunc == &copy<B> || bFunc == &valid<B>); }
};
}  // namespace orq::aggregators