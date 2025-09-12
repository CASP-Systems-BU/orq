#pragma once

#include <algorithm>
#include <numeric>

#include "../correlation/correlation_generator.h"
#include "sharded_permutation_generator.h"

namespace orq::random {

class ZeroPermutation : public ShardedPermutation {
    size_t m_size;

   public:
    /**
     * Default constructor.
     */
    ZeroPermutation() {}

    /**
     * Constructor with size.
     * @param size The size of the permutation.
     */
    ZeroPermutation(size_t size) : m_size(size) {}

    /**
     * Get the size of the permutation.
     * @return The size of the permutation.
     */
    size_t size() { return m_size; }

    /**
     * Create a copy of the permutation.
     * @return A shared pointer to the cloned permutation.
     */
    std::shared_ptr<ShardedPermutation> clone() {
        return std::make_shared<ZeroPermutation>(m_size);
    }
};

/**
 * Zero "Permutation" Generator. DOES NOT generate permutations. For cost
 * model purposes only.
 */
class ZeroPermutationGenerator : public ShardedPermutationGenerator {
   public:
    /**
     * Constructor for the zero permutation generator.
     * @param _rank The rank of this party.
     * @param _commonPRGManager The CommonPRGManager (unused).
     * @param _groups The groups (unused).
     */
    ZeroPermutationGenerator(int _rank, std::shared_ptr<CommonPRGManager> _commonPRGManager,
                             std::vector<std::set<int>> _groups)
        : ShardedPermutationGenerator(_rank) {}

    /**
     * Generate a mapping of permutations for the given size.
     * @param n The size of the permutations to generate.
     * @return A set of permutations, one for each group.
     */
    std::shared_ptr<ShardedPermutation> getNext(size_t n) {
        return std::make_shared<ZeroPermutation>(n);
    }

    /**
     * Allocate memory for zero permutations.
     * @param num_permutations The number of permutations to allocate.
     * @param size_permutation The size of each permutation.
     * @return A vector of zero permutations.
     */
    std::vector<std::shared_ptr<ShardedPermutation>> allocate(size_t num_permutations,
                                                              size_t size_permutation) {
        std::vector<std::shared_ptr<ShardedPermutation>> ret;
        for (int i = 0; i < num_permutations; ++i) {
            ret.push_back(std::make_shared<ZeroPermutation>(size_permutation));
        }

        return ret;
    }

    /**
     * Generate a batch of zero permutations (no-op).
     * @param ret A vector of permutations.
     */
    void generateBatch(std::vector<std::shared_ptr<ShardedPermutation>>& ret) {}
};

}  // namespace orq::random
