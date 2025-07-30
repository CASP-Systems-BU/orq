#ifndef SECRECY_DM_DUMMY_PERMUTATION_GENERATOR_H
#define SECRECY_DM_DUMMY_PERMUTATION_GENERATOR_H

#ifdef MPC_PROTOCOL_BEAVER_TWO

#include "../correlation_generator.h"
#include "dm_sharded_permutation_generator.h"
#include "hm_sharded_permutation_generator.h"
#include "sharded_permutation_generator.h"

#include <utility>

namespace secrecy::random {

/**
 * Dummy Dishonest Majority Generator
 */
template <typename T>
class DMDummyGenerator : public DMShardedPermutationGenerator<T> {
    using DMBase = DMShardedPermutationGenerator<T>;

    std::shared_ptr<CommonPRG> all_prg;

   public:
    DMDummyGenerator(int _rank, int thread, std::shared_ptr<CommonPRGManager> common,
                     std::optional<Communicator*> _comm = std::nullopt)
        : DMBase(_rank, _comm) {
        all_prg = common->get({0, 1});
    }

    /**
     * getNext - Generate and return a DMShardedPermutation.
     * @param n - The size of the permutation.
     * @return The DMShardedPermutation.
     */
    std::shared_ptr<ShardedPermutation> getNext(size_t n) {
        auto dm_perm = std::make_shared<DMShardedPermutation<T>>(n);
        getNext(dm_perm);
        return dm_perm;
    }

    /**
     * getNext - In-place generation of a DMShardedPermutation.
     * @param perm - The shared pointer to the ShardedPermutation to modify in place.
     */
    void getNext(std::shared_ptr<ShardedPermutation> perm) {
        //stopwatch::profile_preprocessing();

        auto dm_perm = std::static_pointer_cast<DMShardedPermutation<T>>(perm);
        
        // get the size from the existing permutation
        size_t n = dm_perm->size();

        Vector<int> pi_0(n);
        Vector<int> pi_1(n);

        // if the permutation has a CommonPRG, use it
        std::shared_ptr<CommonPRG> prg;
        if (dm_perm->hasCommonPRG()) {
            // use existing CommonPRG to generate a pair, requires communication
            prg = dm_perm->getCommonPRG();

            auto comm = DMBase::getComm();
            if (comm == nullptr) {
                throw std::runtime_error("Communicator is not set");
            }

            if (DMBase::getRank() == 0) {
                std::vector<int> random_perm_0(n);
                gen_perm(random_perm_0, prg);
                pi_0 = random_perm_0;
                comm->exchangeShares(pi_0, pi_1, 1, n);
            } else {
                std::vector<int> random_perm_1(n);
                gen_perm(random_perm_1, prg);
                pi_1 = random_perm_1;
                comm->exchangeShares(pi_1, pi_0, 1, n);
            }
        } else {
            // use the all_prg to generate without communication

            // generate random permutations
            std::vector<int> random_perm_0(n);
            gen_perm(random_perm_0, all_prg);
            pi_0 = random_perm_0;

            std::vector<int> random_perm_1(n);
            gen_perm(random_perm_1, all_prg);
            pi_1 = random_perm_1;
        }

        getNextImpl(dm_perm, pi_0, pi_1);
    }

    /**
     * getNextImpl - The implementation function that generates a permutation correlation for a
     * fixed permutation.
     * @param perm - The sharded permutation to generate.
     * @param pi_0 - The permutation of the first party in the correlation.
     * @param pi_1 - The permutation of the second party in the correlation.
     */
    void getNextImpl(std::shared_ptr<ShardedPermutation> perm, Vector<int> pi_0, Vector<int> pi_1) {
        auto dm_perm = std::static_pointer_cast<DMShardedPermutation<T>>(perm);
        size_t n = dm_perm->size();

        // generate 2 random vectors
        Vector<T> A_0(n);
        Vector<T> B_0(n);
        all_prg->getNext(A_0);
        all_prg->getNext(B_0);

        Vector<T> A_1(n);
        Vector<T> B_1(n);
        all_prg->getNext(A_1);
        all_prg->getNext(B_1);

        // calculate pi(A)
        Vector<T> pi_A_0(n);
        Vector<T> pi_A_1(n);
        for (int i = 0; i < n; i++) {
            pi_A_0[i] = A_0[i];
            pi_A_1[i] = A_1[i];
        }
        secrecy::operators::local_apply_perm_single_threaded(pi_A_0, pi_0);
        secrecy::operators::local_apply_perm_single_threaded(pi_A_1, pi_1);

        // set C = pi(A) - B
        Vector<T> C_0(n);
        Vector<T> C_1(n);

        C_0 = pi_A_0 ^ B_0;
        C_1 = pi_A_1 ^ B_1;

        // dereference the shared pointer to the tuple
        // assign to the underlying tuple
        // this modified the input in place
        auto &perm_tuple = *(dm_perm->getTuple());
        if (DMBase::getRank() == 0) {
            perm_tuple = std::make_tuple(std::move(pi_0), std::move(A_1), std::move(B_1), std::move(C_0));
        } else {
            perm_tuple = std::make_tuple(std::move(pi_1), std::move(A_0), std::move(B_0), std::move(C_1));
        }

        //stopwatch::profile_preprocessing("dm-dummyperm");
    }
        
    /**
     * generateBatch - Generate a batch of DMShardedPermutations, invoked by the runtime.
     * @param ret - A vector of DMShardedPermutations to fill.
     */
    void generateBatch(std::vector<std::shared_ptr<ShardedPermutation>>& ret) {
        // generate a DMShardedPermutation for each element in the batch
        for (size_t i = 0; i < ret.size(); ++i) {
            getNext(ret[i]);
        }
    }

};

}  // namespace secrecy::random


#endif // MPC_PROTOCOL_BEAVER_TWO
#endif // SECRECY_DM_DUMMY_PERMUTATION_GENERATOR_H
