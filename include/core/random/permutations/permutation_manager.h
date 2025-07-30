#ifndef SECRECY_PERMUTATION_MANAGER_H
#define SECRECY_PERMUTATION_MANAGER_H

#include "../../../service/common/runtime.h"
#include "dm_sharded_permutation_generator.h"
#include "hm_sharded_permutation_generator.h"

using namespace secrecy::service;

namespace secrecy::random {

// helper function to share CommonPRG between a pair of DMShardedPermutation correlations
template <typename T>
void setup_dm_pair(std::shared_ptr<ShardedPermutation>& perm1,
                   std::shared_ptr<ShardedPermutation>& perm2) {
    // create a fresh AES key for common randomness
    std::vector<unsigned char> seed(crypto_aead_aes256gcm_KEYBYTES);
    AESPRGAlgorithm::aesKeyGen(std::span<unsigned char>(seed.data(), seed.size()));

    // instantiate two AES-based PRG algorithms using the same seed so that parties share randomness
    auto algo1 = std::make_unique<AESPRGAlgorithm>(seed);
    auto algo2 = std::make_unique<AESPRGAlgorithm>(seed);

    // wrap the algorithms in CommonPRG instances
    auto first_prg  = std::make_shared<CommonPRG>(std::move(algo1), -1);
    auto second_prg = std::make_shared<CommonPRG>(std::move(algo2), -1);

    // Cast to DMShardedPermutation so we can set the CommonPRG instances
    auto dm_perm_1 = std::static_pointer_cast<DMShardedPermutation<T>>(perm1);
    auto dm_perm_2 = std::static_pointer_cast<DMShardedPermutation<T>>(perm2);

    dm_perm_1->setCommonPRG(first_prg);
    dm_perm_2->setCommonPRG(second_prg);
}

class PermutationManager {
    // the singleton instance
    static std::shared_ptr<PermutationManager> instance;

    // queue of sharded permutations
    std::queue<std::shared_ptr<ShardedPermutation>> queue;

    // queue of pairs of sharded permutations (2PC only)
    std::queue<std::pair<std::shared_ptr<ShardedPermutation>,
                         std::shared_ptr<ShardedPermutation>>
              > pair_queue;

    // the size of the individual permutations stored in the queue
    size_t stored_size = 0;

    // whether we've shown the warning about no permutations in the queue
    bool have_shown_warning = false;

   public:
    /**
     * Empty constructor for the PermutationManager.
     */
    PermutationManager() : stored_size(0) {}

    // delete assignment operator
    void operator=(const PermutationManager &) = delete;

    /**
     * get - Get the singleton instance of the PermutationManager.
     * @return The singleton instance of the PermutationManager.
     */
    static std::shared_ptr<PermutationManager> get() {
        if (instance == nullptr) {
            instance = std::make_shared<PermutationManager>();
        }
        return instance;
    }

    /**
     * size - Get the number of sharded permutations in the queue.
     * @return The number of sharded permutations in the queue.
     */
    size_t size() {
        return queue.size();
    }

    /**
     * size_pairs - Get the number of pairs of sharded permutations in the queue.
     * @return The number of pairs of sharded permutations in the queue.
     */
    size_t size_pairs() {
        return pair_queue.size();
    }

    /**
     * reserve - Reserve a number of sharded permutations in the queue.
     * @param size_permutation - The size of each sharded permutation.
     * @param num_permutations - The number of sharded permutations to reserve.
     * @param num_pairs - (2PC only) how many pairs of correlated permutations to reserve.
     */
    void reserve(size_t size_permutation, size_t num_permutations, size_t num_pairs = 0)
    {
	if (runTime->getNumParties() == 2) {
	    stopwatch::profile_preprocessing();
	}
	
	// except 2PC, pairs are just individual permutations (same generation process)
        if (runTime->getNumParties() != 2) {
            num_permutations += num_pairs;
            num_pairs = 0;
        }

        auto generator =
            runTime->rand0()->getCorrelation<__int128_t, secrecy::random::Correlation::ShardedPermutation>();

        // if the queue is not empty and the sizes don't match, empty it
        if (((!queue.empty()) || (!pair_queue.empty())) && (stored_size != size_permutation)) {
            single_cout(
                "WARNING: Emptying "
                << queue.size()
                << " stored permutations due to incompatible sizes")
            
            while (!queue.empty()) {
                queue.pop();
            }
            while (!pair_queue.empty()) {
                pair_queue.pop();
            }
        }

        // set the size of the stored permutations
        stored_size = size_permutation;

        int rank = runTime->getPartyID();

        if ((num_permutations <= queue.size()) && (num_pairs <= pair_queue.size())) {
            // we have enough permutations and pairs, so don't do anything
            return;
        } else {
            // only allocate the extra, if needed
            if (num_permutations > queue.size()) {
                num_permutations -= queue.size();
            }
            if (num_pairs > pair_queue.size()) {
                num_pairs -= pair_queue.size();
            }
        }

        // calculate how many individual permutations to generate
        int num_to_generate = num_permutations + 2 * num_pairs;

        // allocate remaining permutations
        // TODO: rename to prepare
        std::vector<std::shared_ptr<ShardedPermutation>> ret =
            generator->allocate(num_to_generate, size_permutation);

        // if 2PC, make the pairs share the same CommonPRG
        if (runTime->getNumParties() == 2) {
            for (int i = 0; i < num_pairs; i++) {
                setup_dm_pair<__int128_t>(ret[2 * i], ret[2 * i + 1]);
            }
        }

        runTime->generate_permutations<__int128_t>(ret);

        // if 2PC, add to pair queue
        if (runTime->getNumParties() == 2) {
            for (int i = 0; i < num_pairs; i++) {
                pair_queue.push(std::make_pair(ret[2 * i], ret[2 * i + 1]));
            }
        }

        // add non-pair permutations to the queue
        // for 3PC and 4PC, num_pairs is 0
        for (int i = num_pairs * 2; i < num_to_generate; i++) {
            queue.push(ret[i]);
        }

	if (runTime->getNumParties() == 2) {
	    stopwatch::profile_preprocessing("dm-dummyperm");
	}
    }

    /**
     * getNext - Get the next sharded permutation in the queue.
     * @param size_permutation - The size of the permutation to get.
     * @return The next sharded permutation in the queue.
     */
    template <typename T>
    std::shared_ptr<ShardedPermutation> getNext(
        size_t size_permutation,
        std::optional<secrecy::Encoding> dm_encoding = secrecy::Encoding::BShared)
    {
        if (runTime->getNumParties() == 2) {
	    stopwatch::profile_preprocessing();
	}

        std::shared_ptr<ShardedPermutation> next;
        
        if (size() == 0) {
            if (! have_shown_warning) {
                single_cout("NOTE: no permutations in queue. Recommend calling reserve().");
                have_shown_warning = true;
            }

            auto generator =
                runTime->rand0()->getCorrelation<__int128_t, secrecy::random::Correlation::ShardedPermutation>();
            
            next = generator->getNext(size_permutation);
        } else {
            // make sure the item retrieved from the queue is of the right size
            if (size_permutation != stored_size) {
                single_cout("WARNING: Returning wrong size permutation");
                reserve(size_permutation, 1, 0);
            }

            next = queue.front();
            assert(next->size() == size_permutation);
            queue.pop();
        }
        
        if (runTime->getNumParties() == 2) {
            auto dm_perm = std::dynamic_pointer_cast<DMShardedPermutation<__int128_t>>(next);
            auto converted = dm_perm_convert_type<__int128_t, T>(dm_perm);

            if (dm_encoding.value() == secrecy::Encoding::AShared) {
                next = dm_perm_convert_b2a(converted);
            } else {
                next = converted;
            }
        }

        if (runTime->getNumParties() == 2) {
	    stopwatch::profile_preprocessing("dm-dummyperm");
	}
	return next;
    }

    /**
     * getNextPair - Get a pair of sharded permutations.
     * @param size_permutation - The size of the permutation to get.
     * @return A pair containing two sharded permutations.
     */
    template <typename T1, typename T2>
    std::pair<std::shared_ptr<ShardedPermutation>, std::shared_ptr<ShardedPermutation>> getNextPair(
        size_t size_permutation,
        std::optional<secrecy::Encoding> dm_encoding_1 = secrecy::Encoding::BShared,
        std::optional<secrecy::Encoding> dm_encoding_2 = secrecy::Encoding::BShared)
    {
        if (runTime->getNumParties() == 2) {
	    stopwatch::profile_preprocessing();
	}

        // 3PC and 4PC logic (simpler)
        if (runTime->getNumParties() != 2) {
            std::shared_ptr<ShardedPermutation> perm;
            if (size() == 0) {
                if (! have_shown_warning) {
                    single_cout("NOTE: no permutations in queue. Recommend calling reserve().");
                    have_shown_warning = true;
                }

                auto generator =
                    runTime->rand0()->getCorrelation<__int128_t, secrecy::random::Correlation::ShardedPermutation>();

                perm = generator->getNext(size_permutation);
            } else {
                // make sure the item retrieved from the queue is of the right size
                if (size_permutation != stored_size) {
                    single_cout("WARNING: Returning wrong size permutation");
                }

                perm = queue.front();
                assert(perm->size() == size_permutation);
                queue.pop();
            }

            // create a deep copy for the second element
            auto copy = perm->clone();
	
	    if (runTime->getNumParties() == 2) {
	        stopwatch::profile_preprocessing("dm-dummyperm");
	    }
	    return std::make_pair(perm, copy);
        }

        // 2PC logic below here

        if (size_pairs() == 0) {
            if (! have_shown_warning) {
                single_cout("NOTE: no permutations in queue. Recommend calling reserve().");
                have_shown_warning = true;
            }

            // generate a pair of permutations
            reserve(size_permutation, 0, 1);
        }
        
        auto pair = pair_queue.front();
        pair_queue.pop();

        // get the pair of permutations as two separate elements
        auto perm1 = pair.first;
        auto perm2 = pair.second;

        // convert the permutations to the desired types
        auto dm_perm1 = std::static_pointer_cast<DMShardedPermutation<__int128_t>>(perm1);
        auto dm_perm2 = std::static_pointer_cast<DMShardedPermutation<__int128_t>>(perm2);

        auto converted_1 = dm_perm_convert_type<__int128_t, T1>(dm_perm1);
        auto converted_2 = dm_perm_convert_type<__int128_t, T2>(dm_perm2);

        std::shared_ptr<DMShardedPermutation<T1>> result_1 = converted_1;
        std::shared_ptr<DMShardedPermutation<T2>> result_2 = converted_2;
        
        if (dm_encoding_1.value() == secrecy::Encoding::AShared) {
            result_1 = dm_perm_convert_b2a(converted_1);
        }

        if (dm_encoding_2.value() == secrecy::Encoding::AShared) {
            result_2 = dm_perm_convert_b2a(converted_2);
        }

	if (runTime->getNumParties() == 2) {
	    stopwatch::profile_preprocessing("dm-dummyperm");
	}
	return std::make_pair(result_1, result_2);
    }

};
    
// initialize the static instance pointer
std::shared_ptr<PermutationManager> PermutationManager::instance = nullptr;

}  // namespace secrecy::random

#endif  // SECRECY_PERMUTATION_MANAGER_H
