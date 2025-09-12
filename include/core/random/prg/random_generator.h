#pragma once

#include "core/containers/e_vector.h"
#include "debug/orq_debug.h"

namespace orq::random {

/**
 * @brief Base class for random number generators.
 *
 * Provides a common interface for generating pseudorandom numbers.
 */
class RandomGenerator {
    // the seed randomness
    // depending on implementation of generator, could also be a key (eg to a PRF)
    unsigned short seed;

    // NOTE: inheriting classes should implement:
    //  - Functions to add/edit different PRG queues.

    /**
     * Helper function to generate a secure seed.
     * @return A seed value (currently returns 0 - TODO: implement).
     */
    static unsigned short generate_seed() {
        // TODO: implement this
        return 0;
    }

   public:
    /**
     * Default constructor that generates a seed automatically.
     */
    RandomGenerator() : RandomGenerator(generate_seed()) {}

    /**
     * Constructor with provided seed.
     * @param _seed The seed randomness.
     */
    RandomGenerator(unsigned short _seed) : seed(_seed) {}

    /**
     * Virtual destructor.
     */
    virtual ~RandomGenerator() {}

    /**
     * Generate the next element of some Pseudo Random Numbers Queue.
     * @param num The variable to fill with a random number.
     */
    template <typename T>
    void getNext(T &num) {}

    /**
     * Generate many next elements of some Pseudo Random Numbers Queue.
     * @param nums The vector to fill with random numbers.
     */
    template <typename T>
    void getNext(Vector<T> &nums) {
        for (size_t i = 0; i < nums.size(); ++i) {
            getNext(nums[i]);
        }
    }
};
}  // namespace orq::random
