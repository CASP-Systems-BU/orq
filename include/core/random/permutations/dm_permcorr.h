#pragma once

#ifdef MPC_PROTOCOL_BEAVER_TWO

#include <sodium.h>

#include <utility>

#include "../correlation/correlation_generator.h"
#include "dm_sharded_permutation_generator.h"
#include "hm_sharded_permutation_generator.h"
#include "sharded_permutation_generator.h"

namespace orq::random {

/**
 * @brief Real Dishonest Majority Permutation Correlation Generator
 *
 * A cryptographic implementation that generates permutation correlations using an
 * OPRF and a random oracle. This is the secure version of the DM permutation generator.
 *
 * @tparam T The data type for the permutation correlation elements
 */
template <typename T>
class DMPermutationCorrelationGenerator : public DMShardedPermutationGenerator<T> {
    using DMBase = DMShardedPermutationGenerator<T>;

    int rank;
    int thread;

    std::shared_ptr<CommonPRG> local_prg;
    std::shared_ptr<CommonPRG> all_prg;

   public:
    /**
     * Constructor for the cryptographic DM permutation
     * generator.
     * @param _rank The rank of this party (0 or 1).
     * @param thread The thread number.
     * @param common The CommonPRGManager.
     * @param _comm Optional communicator.
     */
    DMPermutationCorrelationGenerator(int _rank, int thread,
                                      std::shared_ptr<CommonPRGManager> common,
                                      std::optional<Communicator*> _comm = std::nullopt)
        : DMBase(_rank, _comm), rank(_rank), thread(thread) {
        local_prg = std::make_shared<CommonPRG>();
        all_prg = common->get({0, 1});
    }

    /**
     * Query the random oracle for a given number of hashes.
     * Parties share the same hash key to generate the same values.
     * @param n The number of hashes to query.
     * @return A vector of hashes.
     */
    Vector<__uint128_t> queryRandomOracle(int n) {
        constexpr size_t key_size = crypto_generichash_KEYBYTES;

        // generate a random 32-byte key using common randomness
        Vector<uint8_t> common_key(key_size);
        all_prg->getNext(common_key);
        // view the underlying data as unsigned char* without copying
        const unsigned char* key = static_cast<const unsigned char*>(common_key.span().data());

        // vector to store the hashes
        std::vector<__int128_t> hashes(n);

        for (int i = 0; i < n; i++) {
            // generate the hash directly into the appropriate slot of the output vector
            const unsigned char* data = reinterpret_cast<const unsigned char*>(&i);
            unsigned char* out_ptr = reinterpret_cast<unsigned char*>(&hashes[i]);

            if (crypto_generichash(out_ptr, sizeof(__int128_t), data, sizeof(i), key, key_size) !=
                0) {
                std::cerr << "Failed to compute hash for " << i << std::endl;
                continue;
            }
        }

        return Vector<__int128_t>(hashes);
    }

    /**
     * Generate and return a DMShardedPermutation.
     * @param n The size of the permutation.
     * @return The DMShardedPermutation.
     */
    std::shared_ptr<ShardedPermutation> getNext(size_t n) {
        auto dm_perm = std::make_shared<DMShardedPermutation<T>>(n);
        getNext(dm_perm);
        return dm_perm;
    }

    /**
     * In-place generation of a DMShardedPermutation.
     * @param perm The shared pointer to the ShardedPermutation to modify in place.
     */
    void getNext(std::shared_ptr<ShardedPermutation> perm) {
        auto dm_perm = std::static_pointer_cast<DMShardedPermutation<T>>(perm);

        // if the permutation has a CommonPRG, use it
        std::shared_ptr<CommonPRG> prg;
        if (dm_perm->hasCommonPRG()) {
            prg = dm_perm->getCommonPRG();
        } else {
            prg = local_prg;
        }

        // get the size from the existing permutation
        size_t n = dm_perm->size();

        // get reference to the underlying tuple
        auto& perm_tuple = *(dm_perm->getTuple());

        // create a new OPRF object
        std::unique_ptr<OPRF> oprf = std::make_unique<OPRF>(rank, thread);

        // the parties agree on these
        Vector<__int128_t> hashes_0 = queryRandomOracle(n);
        Vector<__int128_t> hashes_1 = queryRandomOracle(n);

        if (DMBase::getRank() == 0) {
            // perform the plaintext PRF evaluations
            OPRF::key_t key = oprf->keyGen();
            Vector<__int128_t> A_1 =
                oprf->evaluate_plaintext<__int128_t>(hashes_0.as_std_vector(), key);

            // permute the hashes
            std::vector<int> pi_vec(n);
            gen_perm(pi_vec, prg);
            Vector<int> pi_0(std::move(pi_vec));
            orq::operators::local_apply_perm_single_threaded(hashes_1, pi_0);

            Vector<__int128_t> B_1 = oprf->evaluate_sender<__int128_t>(key, n);
            Vector<__int128_t> C_0 = oprf->evaluate_receiver(hashes_1);

            // pack and move into the tuple in a single assignment
            perm_tuple =
                std::make_tuple(std::move(pi_0), std::move(A_1), std::move(B_1), std::move(C_0));
        } else {
            // perform the plaintext PRF evaluations
            OPRF::key_t key = oprf->keyGen();
            Vector<__int128_t> A_0 =
                oprf->evaluate_plaintext<__int128_t>(hashes_1.as_std_vector(), key);

            // permute the hashes
            std::vector<int> pi_vec(n);
            gen_perm(pi_vec, prg);
            Vector<int> pi_1(std::move(pi_vec));
            orq::operators::local_apply_perm_single_threaded(hashes_0, pi_1);

            Vector<__int128_t> C_1 = oprf->evaluate_receiver<__int128_t>(hashes_0);
            Vector<__int128_t> B_0 = oprf->evaluate_sender<__int128_t>(key, n);

            // pack and move into the tuple in a single assignment
            perm_tuple =
                std::make_tuple(std::move(pi_1), std::move(A_0), std::move(B_0), std::move(C_1));
        }
    }

    /**
     * Generate a batch of DMShardedPermutations, invoked by the runtime.
     * @param ret A vector of DMShardedPermutations to fill.
     */
    void generateBatch(std::vector<std::shared_ptr<ShardedPermutation>>& ret) {
        // generate a DMShardedPermutation for each element in the batch
        for (size_t i = 0; i < ret.size(); ++i) {
            getNext(ret[i]);
        }
    }
};

}  // namespace orq::random

#endif  // MPC_PROTOCOL_BEAVER_TWO