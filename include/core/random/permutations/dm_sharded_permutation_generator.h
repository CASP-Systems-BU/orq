#ifndef SECRECY_DM_SHARDED_PERMUTATION_GENERATOR_H
#define SECRECY_DM_SHARDED_PERMUTATION_GENERATOR_H

#include "../correlation_generator.h"
#include "../common_prg.h"
#include "sharded_permutation_generator.h"

namespace secrecy::operators {

    template <typename Share>
    void local_apply_perm_single_threaded(Vector<Share> &x, Vector<int> &permutation);

}


namespace secrecy::random {

    /**
     * Dishonest Majority Sharded Permutation
     *
     * A tuple of a local permutation and three random vectors subject to pi(A) = B + C.
     */
    template <typename T>
    class DMShardedPermutation : public ShardedPermutation {
        // the correlation type
        using dm_perm_t = std::tuple<Vector<int>, Vector<T>, Vector<T>, Vector<T>>;

        // the underlying data
        std::shared_ptr<std::tuple<Vector<int>, Vector<T>, Vector<T>, Vector<T>>> perm;

        // the type of the permutation (arithmetic or binary)
        secrecy::Encoding encoding;
        
        // CommonPRG for random generation
        bool has_common_prg;
        std::shared_ptr<CommonPRG> common_prg;
    
    public:

        /**
         * Constructor that takes the size of the permutation correlation (the minimal amount
         * of information) and defaults to binary encoding.
         * @param n - The size of the permutation correlation.
         */
        DMShardedPermutation(size_t n)
            : perm(std::make_shared<dm_perm_t>(n, n, n, n)), encoding(secrecy::Encoding::BShared), 
              common_prg(nullptr), has_common_prg(false) {}
        
        /**
         * Constructor that takes the size of the permutation correlation and an encoding type.
         * @param n - The size of the permutation correlation.
         * @param _encoding - The encoding type of the permutation correlation.
         */
        DMShardedPermutation(size_t n, secrecy::Encoding _encoding)
            : perm(std::make_shared<dm_perm_t>(n, n, n, n)), encoding(_encoding), 
              common_prg(nullptr), has_common_prg(false) {}

        /**
         * Constructor that takes an existing permutation correlation and an encoding type.
         * @param _perm - The existing permutation correlation.
         * @param _encoding - The encoding type of the permutation correlation.
         */
        DMShardedPermutation(dm_perm_t _perm, secrecy::Encoding _encoding)
            : perm(std::make_shared<dm_perm_t>(_perm)), encoding(_encoding), 
              common_prg(nullptr), has_common_prg(false) {}

        /**
         * getTuple - Expose the underlying data through a getter function.
         */
        std::shared_ptr<dm_perm_t> getTuple() {
            return perm;
        }

        /**
         * getEncoding - Expose the type of the correlation through a getter function.
         */
        secrecy::Encoding getEncoding() {
            return encoding;
        }

        /**
         * hasCommonPRG - Expose the existence of a CommonPRG through a getter function.
         */
        bool hasCommonPRG() {
            return has_common_prg;
        }

        /**
         * getCommonPRG - Expose the CommonPRG through a getter function.
         */
        std::shared_ptr<CommonPRG> getCommonPRG() {
            return common_prg;
        }

        /**
         * setCommonPRG - Set the CommonPRG.
         */
        void setCommonPRG(std::shared_ptr<CommonPRG> _common_prg) {
            common_prg = _common_prg;
            has_common_prg = true;
        }

        size_t size() {
            return std::get<0>(*perm).size();
        }

        std::shared_ptr<ShardedPermutation> clone() {
            // get the underlying tuple
            auto [pi, A, B, C] = *perm;
            
            // create new copies of each vector
            Vector<int> pi_copy(pi);
            Vector<T> A_copy(A);
            Vector<T> B_copy(B);
            Vector<T> C_copy(C);
            
            // create a new DMShardedPermutation with the copied data and CommonPRG
            return std::make_shared<DMShardedPermutation<T>>(
                std::make_tuple(std::move(pi_copy), std::move(A_copy), std::move(B_copy), std::move(C_copy)),
                encoding
            );
        }

    };

    template<typename T1, typename T2>
    std::shared_ptr<DMShardedPermutation<T2>> dm_perm_convert_type(std::shared_ptr<DMShardedPermutation<T1>> perm) {
        using vec_t2 = Vector<T2>;
    
        const auto& perm_tuple = *(perm->getTuple());
        const auto& [pi, A, B, C] = perm_tuple;
    
        Vector<int> pi_copy = pi;
    
        auto convert = [](const auto& v) {
            const auto& std_v = v.as_std_vector();
            std::vector<T2> result;
            result.reserve(std_v.size());
            std::transform(std_v.begin(), std_v.end(), std::back_inserter(result),
                           [](const T1& x) { return static_cast<T2>(x); });
            return vec_t2(result);
        };
    
        vec_t2 A2 = convert(A);
        vec_t2 B2 = convert(B);
        vec_t2 C2 = convert(C);
    
        return std::make_shared<DMShardedPermutation<T2>>(
            std::make_tuple(std::move(pi_copy), std::move(A2), std::move(B2), std::move(C2)),
            perm->getEncoding()
        );
    }

    template<typename T>
    std::shared_ptr<DMShardedPermutation<T>> dm_perm_convert_b2a(std::shared_ptr<DMShardedPermutation<T>> perm) {
        auto [pi, A, B, C] = *(perm->getTuple());

        // create BSharedVectors
        BSharedVector<T, EVector<T, 1>> B_binary(perm->size());
        BSharedVector<T, EVector<T, 1>> C_binary(perm->size());

        // P0's B matches up with P1's C, and vise versa
        if (secrecy::service::runTime->getPartyID() == 0) {
            B_binary.vector(0) = B;
            C_binary.vector(0) = C;
        } else {
            B_binary.vector(0) = C;
            C_binary.vector(0) = B;
        }

        // convert to ASharedVectors
#ifdef USE_LIBOTE
	ASharedVector<T, EVector<T, 1>> B_arithmetic = B_binary.b2a();
        ASharedVector<T, EVector<T, 1>> C_arithmetic = C_binary.b2a();
#else
	ASharedVector<T, EVector<T, 1>> B_arithmetic = B_binary.insecure_b2a();
	ASharedVector<T, EVector<T, 1>> C_arithmetic = C_binary.insecure_b2a();
#endif

        if (secrecy::service::runTime->getPartyID() == 0) {
            B = B_arithmetic.vector(0);
            C = C_arithmetic.vector(0);
        } else {
            B = C_arithmetic.vector(0);
            C = B_arithmetic.vector(0);
        }

        return std::make_shared<DMShardedPermutation<T>>(
            std::make_tuple(std::move(pi), std::move(A), std::move(B), std::move(C)),
            secrecy::Encoding::AShared
        );
    }

    /**
     * Dishonest Majority Sharded Permutation Generator
     * 
     * Currently only supports 2PC
     */
    template <typename T>
    class DMShardedPermutationGenerator : public ShardedPermutationGenerator {

        int rank;
        std::optional<Communicator*> comm;

    public:

        // permutation correlation type
        using dm_perm_t = std::tuple<Vector<int>, Vector<T>, Vector<T>, Vector<T>>;

        /**
        * Constructor for DMShardedPermutationGenerator.
        * @param rank The rank of the current party.
        * @param _comm An optional pointer to a Communicator instance for communication purposes.
        *              Defaults to std::nullopt if not provided.
        */
        DMShardedPermutationGenerator(int _rank, std::optional<Communicator*> _comm=std::nullopt)
            : ShardedPermutationGenerator(_rank, _comm), rank(_rank), comm(_comm) {}

        /**
         * getNext - Generate and return a DMShardedPermutation.
         * @param n - The size of the permutation.
         * @return The DMShardedPermutation.
         */
        virtual std::shared_ptr<ShardedPermutation> getNext(size_t n) {
            return std::make_shared<DMShardedPermutation<T>>(n);
        }
        
        /**
         * allocate - Allocate memory for many DMShardedPermutations so they can be passed to and generated by the runtime.
         * @param num_permutations - The number of permutations to allocate memory for.
         * @param n - The size of the permutations.
         * @return A vector of empty DMShardedPermutations.
         */
        std::vector<std::shared_ptr<ShardedPermutation>> allocate(size_t num_permutations, size_t size_permutation) {
            std::vector<std::shared_ptr<ShardedPermutation>> ret;
            for (int i = 0; i < num_permutations; ++i) {
                auto perm = std::make_shared<DMShardedPermutation<T>>(size_permutation);
                ret.push_back(perm);
            }

            return ret;
        }
        
        /**
         * generateBatch - Generate a batch of DMShardedPermutations, invoked by the runtime.
         * @param ret - A vector of DMShardedPermutations to fill.
         */
        void generateBatch(std::vector<std::shared_ptr<ShardedPermutation>>& ret) {
            // skip for now
        }

        void assertCorrelated(std::shared_ptr<DMShardedPermutation<T>> &perm) {
            if (! comm.has_value()) {
                if (getRank() == 0) {
                    std::cout << "Skipping Permutation check: communicator not defined\n";
                }
                return;
            }

            dm_perm_t perm_tuple = *(perm->getTuple());
            bool is_a_shared = perm->getEncoding() == secrecy::Encoding::AShared;
            
            // declare values
            auto n = perm->size();
            Vector<int> pi_0(n), pi_1(n);
            Vector<T> A_0(n), B_0(n), C_0(n);
            Vector<T> A_1(n), B_1(n), C_1(n);

            if (getRank() == 0) {
                auto [pi_0, A_1, B_1, C_0] = perm_tuple;

                comm.value()->receiveShares(A_0, 1, n);
                comm.value()->receiveShares(B_0, 1, n);
                
                comm.value()->receiveShares(pi_1, 1, n);
                comm.value()->receiveShares(C_1, 1, n);

                Vector<T> pi_A_0(n);
                Vector<T> pi_A_1(n);
                for (int i = 0; i < n; i++) {
                    pi_A_0[i] = A_0[i];
                    pi_A_1[i] = A_1[i];
                }

                // check correctness - 0
                secrecy::operators::local_apply_perm_single_threaded(pi_A_0, pi_0);
                Vector<T> B_plus_C_0(n);
                if (is_a_shared) {
                    B_plus_C_0 = B_0 + C_0;
                } else {
                    B_plus_C_0 = B_0 ^ C_0;
                }
                assert(pi_A_0.same_as(B_plus_C_0));

                // check correctness - 1
                secrecy::operators::local_apply_perm_single_threaded(pi_A_1, pi_1);
                Vector<T> B_plus_C_1(n);
                if (is_a_shared) {
                    B_plus_C_1 = B_1 + C_1;
                } else {
                    B_plus_C_1 = B_1 ^ C_1;
                }
                assert(pi_A_1.same_as(B_plus_C_1));
            } else {
                auto [pi_1, A_0, B_0, C_1] = perm_tuple;

                comm.value()->sendShares(A_0, 1, n);
                comm.value()->sendShares(B_0, 1, n);

                comm.value()->sendShares(pi_1, 1, n);
                comm.value()->sendShares(C_1, 1, n);
            }
        }

        /**
         * getComm - Expose the Communicator through a getter function.
         */
        Communicator* getComm() const {
            return comm.has_value() ? comm.value() : nullptr;
        }
    };
    

} // namespace secrecy::random

#endif // SECRECY_DM_SHARDED_PERMUTATION_GENERATOR_H
