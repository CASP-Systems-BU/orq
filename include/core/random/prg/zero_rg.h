#pragma once

namespace orq::random {

/**
 * @brief Random generator that outputs all zeros.
 *
 * Used for testing and benchmarking where actual randomness is not required.
 */
class ZeroRandomGenerator : public CommonPRG {
   public:
    /**
     * Constructor with seed parameter.
     * @param _seed The seed (unused).
     * @param rank The rank of this party.
     */
    ZeroRandomGenerator(unsigned short _seed = 0, int rank = 0) : CommonPRG({}, 0) {}

    /**
     * Constructor with seed vector.
     * @param _seed The seed vector (unused).
     * @param rank The rank of this party.
     */
    ZeroRandomGenerator(std::vector<unsigned char> _seed, int rank = 0) : CommonPRG({}, rank) {}

    /**
     * Generate zero value (no-op).
     * @param num The variable to fill (left unchanged).
     */
    template <typename T>
    void getNext(T &num) {}

    /**
     * Generate zero values (no-op).
     * @param nums The vector to fill (left unchanged).
     */
    template <template <typename...> class V, typename T>
    void getNext(V<T> &nums) {}
};
}  // namespace orq::random
