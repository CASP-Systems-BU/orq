#pragma once

#include "../prg/random_generator.h"

namespace orq::random {
/**
 * @brief Base correlation generator class. This is non-functional and
 * just used for organizational purposes. All correlation generators
 * should inherit from this class, and implement (at least) the
 * `getNext()` and `assertCorrelated()` methods, with the property that
 *
 *   CG.assertCorrelated(CG.getNext())
 *
 * always succeeds.
 *
 * @tparam Corr the type of the correlation for a single party.
 */
template <typename Corr>
class CorrelationGenerator : public RandomGenerator {
   public:
    /**
     * Constructor for the base correlation generator.
     * @param _rank The rank of this party.
     */
    CorrelationGenerator(int _rank) : RandomGenerator(0), rank(_rank) {}

    /**
     * @brief Get a new, random correlation of length n.
     *
     * @param n
     * @return Corr
     */
    virtual Corr getNext(const size_t n) = 0;

    void assertCorrelated(const Corr&) const {}

    /**
     * @brief The rank of this party in the MPC. Equivalent to party ID.
     *
     */
    const int rank;
};

}  // namespace orq::random
