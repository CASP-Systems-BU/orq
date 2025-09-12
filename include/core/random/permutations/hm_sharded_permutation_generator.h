#pragma once

#include <algorithm>
#include <numeric>

#include "../correlation/correlation_generator.h"
#include "sharded_permutation_generator.h"

const int random_buffer_size = 65536;

namespace orq::random {

using Group = std::set<int>;
using LocalPermutation = std::vector<int>;

/**
 * Honest Majority Sharded Permutation
 *
 * A map from permutation groups to local permutaitons.
 */
class HMShardedPermutation : public ShardedPermutation {
    size_t m_size;

    // a pointer to the underlying sharded permutation
    std::shared_ptr<std::map<Group, LocalPermutation>> perm;

   public:
    /**
     * Empty constructor that makes an empty map.
     */
    HMShardedPermutation() : perm(std::make_shared<std::map<Group, LocalPermutation>>()) {}

    /**
     * A constructor that takes an existing map and assigns it to the underlying data.
     * @param _perm The sharded permutation to set the underlying permutation to.
     */
    HMShardedPermutation(std::shared_ptr<std::map<Group, LocalPermutation>> _perm) : perm(_perm) {}

    HMShardedPermutation(size_t _size)
        : perm(std::make_shared<std::map<Group, LocalPermutation>>()), m_size(_size) {}

    /**
     * Expose the underlying permutation map.
     */
    std::shared_ptr<std::map<Group, LocalPermutation>> getPermMap() { return perm; }

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
        return std::make_shared<HMShardedPermutation>(*this);
    }
};

/**
 * Generate a pseudorandom local permutation. Uses the Fisher-Yates shuffle algorithm: creates a
 * vector `{0, 1, ..., size-1}`, permutes it, and returns.
 *
 * @param permutation storage for the permutation.
 * @param generator The common PRG object used as the pseudorandomness source.
 *
 */
void gen_perm(std::vector<int>& permutation, std::shared_ptr<CommonPRG> generator) {
    size_t size = permutation.size();

    std::iota(permutation.begin(), permutation.end(), 0);

    auto _buffer_size = AESPRGAlgorithm::MAX_AES_QUERY_BYTES;

    // we don't know a priori how many random elements we will need,
    // so we don't want to overshoot by too much
    // however, we also get speed benefits from generating randomness in batches
    orq::Vector<uint32_t> random(_buffer_size);
    generator->getNext(random);

    int bits = std::bit_width(size);
    int mask = (1 << bits) - 1;

    int rand_count = 1;

    size_t rand_index = 0;
    for (size_t i = 0; i < size; i++) {
        // get the location to put the ith element
        // generate random int in range [0, n-i-1]
        uint32_t rand = size;
        while (rand > size - i - 1) {
            rand = random[rand_index] & mask;
            rand_index++;
            if (rand_index == _buffer_size) {
                // generate new randomness and reset the index
                generator->getNext(random);
                // std::cout << std::format("get next {}; size {}\n", rand_count++, size);
                rand_index = 0;
            }
        }
        int location = i + rand;

        // swap
        std::swap(permutation[i], permutation[location]);
    }
}

/**
 * Honest Majority Sharded Permutation Generator
 *
 * Currently only supports 3PC and 4PC
 */
class HMShardedPermutationGenerator : public ShardedPermutationGenerator {
    const int rank;
    std::shared_ptr<CommonPRGManager> commonPRGManager;
    std::vector<std::set<int>> groups;

   public:
    /**
     * Constructor for HMShardedPermutationGenerator.
     * @param _rank The rank of the party.
     * @param _commonPRGManager The common PRG manager used to generate the permutations.
     * @param _groups The groups of the protocol.
     */
    HMShardedPermutationGenerator(int _rank, std::shared_ptr<CommonPRGManager> _commonPRGManager,
                                  std::vector<std::set<int>> _groups)
        : ShardedPermutationGenerator(_rank),
          rank(_rank),
          commonPRGManager(std::move(_commonPRGManager)),
          groups(_groups) {}

    /**
     * Generate a mapping of permutations for the given size.
     * @param n The size of the permutations to generate.
     * @return A set of permutations, one for each group.
     */
    std::shared_ptr<ShardedPermutation> getNext(size_t n) {
        auto group_permutation_map = std::make_shared<HMShardedPermutation>();

        // generate random permutations for each group
        for (auto group : groups) {
            if (!group.contains(rank)) {
                continue;
            }
            auto common_prg = commonPRGManager->get(group);
            LocalPermutation permutation(n);
            gen_perm(permutation, common_prg);
            group_permutation_map->getPermMap()->insert({group, permutation});
        }

        return group_permutation_map;
    }

    /**
     * Allocate memory for many HMShardedPermutations so they can be passed to and
     * generated by the runtime.
     * @param num_permutations The number of permutations to allocate memory for.
     * @param size_permutation The size of the permutations.
     * @return A vector of empty HMShardedPermutations.
     */
    std::vector<std::shared_ptr<ShardedPermutation>> allocate(size_t num_permutations,
                                                              size_t size_permutation) {
        std::vector<std::shared_ptr<ShardedPermutation>> ret;
        for (int i = 0; i < num_permutations; ++i) {
            auto group_permutation_map = std::make_shared<HMShardedPermutation>(size_permutation);
            ret.push_back(group_permutation_map);
        }

        return ret;
    }

    /**
     * Generate a set of permutations for the given size.
     * @param ret The set of permutations to generate.
     */
    void generateBatch(std::vector<std::shared_ptr<ShardedPermutation>>& ret) {
        // iterate over each ShardedPermutation in the vector
        for (std::shared_ptr<ShardedPermutation> _perm : ret) {
            std::shared_ptr<HMShardedPermutation> perm =
                std::dynamic_pointer_cast<HMShardedPermutation>(_perm);
            // generate random permutations for each group
            for (auto group : groups) {
                if (!group.contains(rank)) {
                    continue;
                }
                auto common_prg = commonPRGManager->get(group);
                std::vector<int> local_permutation(perm->size());
                (*(perm->getPermMap()))[group] = local_permutation;
                gen_perm((*(perm->getPermMap()))[group], common_prg);
            }
        }
    }
};

}  // namespace orq::random
