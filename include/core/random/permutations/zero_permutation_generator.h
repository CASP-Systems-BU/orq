#ifndef SECRECY_ZERO_PERMUTATION_GENERATOR_H
#define SECRECY_ZERO_PERMUTATION_GENERATOR_H

#include <algorithm>
#include <numeric>

#include "../correlation_generator.h"
#include "sharded_permutation_generator.h"

namespace secrecy::random {

class ZeroPermutation : public ShardedPermutation {
    size_t m_size;

   public:
    ZeroPermutation() {}

    ZeroPermutation(size_t size) : m_size(size) {}

    size_t size() { return m_size; }

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
    ZeroPermutationGenerator(int _rank, std::shared_ptr<CommonPRGManager> _commonPRGManager,
                             std::vector<std::set<int>> _groups)
        : ShardedPermutationGenerator(_rank) {}

    /**
     * getNext - Generate a mapping of permutations for the given size.
     * @param n - The size of the permutations to generate.
     * @return A set of permutations, one for each group.
     */
    std::shared_ptr<ShardedPermutation> getNext(size_t n) {
        return std::make_shared<ZeroPermutation>(n);
    }

    std::vector<std::shared_ptr<ShardedPermutation>> allocate(size_t num_permutations,
                                                              size_t size_permutation) {
        std::vector<std::shared_ptr<ShardedPermutation>> ret;
        for (int i = 0; i < num_permutations; ++i) {
            ret.push_back(std::make_shared<ZeroPermutation>(size_permutation));
        }

        return ret;
    }

    void generateBatch(std::vector<std::shared_ptr<ShardedPermutation>>& ret) {
    }
};

}  // namespace secrecy::random

#endif  // SECRECY_ZERO_PERMUTATION_GENERATOR_H