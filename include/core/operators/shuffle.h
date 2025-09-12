#pragma once

#include "core/random/permutations/dm_dummy.h"
#include "profiling/stopwatch.h"
#ifdef USE_LIBOTE
#include "core/random/permutations/dm_permcorr.h"
#endif
#include "core/random/permutations/permutation_manager.h"
using namespace orq::benchmarking;

using namespace orq::random;

#define random_buffer_size 65536

namespace orq::operators {

#if defined(MPC_PROTOCOL_PLAINTEXT_ONE) || defined(MPC_PROTOCOL_DUMMY_ZERO)
#define INSTRUMENT_APPLYPERM
#endif

template <typename E>
using AElementwisePermutation = ASharedVector<int, orq::EVector<int, E::replicationNumber>>;
template <typename E>
using BElementwisePermutation = BSharedVector<int, orq::EVector<int, E::replicationNumber>>;

#ifdef INSTRUMENT_APPLYPERM
/**
 * @brief Counts oblivious permutation applications for instrumentation.
 *
 * @tparam T Share data type.
 * @param size Size of the permutation being applied.
 */
template <typename T>
void count_oblivious_apply_perm(uint64_t size) {
    using P = orq::Plaintext_1PC<T, T, orq::Vector<T>, EVector<T, 1>>;
    P *proto;
    if constexpr (std::is_same_v<T, int8_t>) {
        proto = (P *)runTime->worker0->proto_8.get();
    } else if constexpr (std::is_same_v<T, int16_t>) {
        proto = (P *)runTime->worker0->proto_16.get();
    } else if constexpr (std::is_same_v<T, int32_t>) {
        proto = (P *)runTime->worker0->proto_32.get();
    } else if constexpr (std::is_same_v<T, int64_t>) {
        proto = (P *)runTime->worker0->proto_64.get();
    } else if constexpr (std::is_same_v<T, __int128_t>) {
        proto = (P *)runTime->worker0->proto_128.get();
    }
    proto->op_counter["applyperm"] += size;
    proto->round_counter["applyperm"] += 1;
}
#endif

/**
 * @brief Generate a pseudorandom local permutation using Fisher-Yates shuffle.
 *
 * Creates a vector {0, 1, ..., size-1}, permutes it, and returns it.
 *
 * @param size The size of the permutation.
 * @param generator The common PRG object used as the pseudorandomness source.
 * @return The permutation as a vector of destination indices.
 */
std::vector<int> gen_perm(size_t size, std::shared_ptr<random::CommonPRG> generator) {
    std::vector<int> permutation(size);
    for (size_t i = 0; i < size; i++) {
        permutation[i] = i;
    }

    // we don't know a priori how many random elements we will need,
    //  so we don't want to overshoot by too much
    // however, we also get speed benefits from generating randomness in batches
    orq::Vector<uint32_t> random(random_buffer_size);
    generator->getNext(random);

    int bits = std::bit_width(size);
    int mask = (1 << bits) - 1;

    int rand_index = 0;
    for (size_t i = 0; i < size; i++) {
        // get the location to put the ith element
        // generate random int in range [0, n-i-1]
        uint32_t rand = size;
        while (rand > size - i - 1) {
            rand = random[rand_index] & mask;
            rand_index++;
            if (rand_index == random_buffer_size) {
                // generate new randomness and reset the index
                generator->getNext(random);
                rand_index = 0;
            }
        }
        int location = i + rand;

        // swap
        int temp = permutation[i];
        permutation[i] = permutation[location];
        permutation[location] = temp;
    }

    return permutation;
}

//////////////////////////////////
// Local Apply Perm + overloads //
//////////////////////////////////

/**
 * @brief Apply a local permutation to a vector.
 *
 * @tparam T Data type of vector elements.
 * @param x The vector to permute (modified in place).
 * @param permutation The permutation to apply.
 */
template <typename T>
void local_apply_perm(Vector<T> &x, const LocalPermutation &permutation) {
    static auto new_x = std::make_unique<Vector<T>>(x.size());
    new_x->resize(x.size());

    auto perm_func = [&](const size_t batch_start, const size_t batch_end) {
        for (size_t i = batch_start; i < batch_end; i++) {
            (*new_x)[permutation[i]] = x[i];
        }
    };

    auto copy_func = [&](const size_t batch_start, const size_t batch_end) {
        for (size_t i = batch_start; i < batch_end; i++) {
            x[i] = (*new_x)[i];
        }
    };

    orq::service::runTime->execute_parallel_unsafe(x.size(), perm_func);
    // copy back in.
    orq::service::runTime->execute_parallel_unsafe(x.size(), copy_func);
}

/**
 * @brief Overload for both vector and permutation represented as ORQ Vector.
 *
 * @tparam T Data type of vector elements.
 * @param x The vector to permute (modified in place).
 * @param permutation The permutation to apply as ORQ Vector.
 */
template <typename T>
void local_apply_perm(Vector<T> &x, Vector<int> &permutation) {
    local_apply_perm(x, permutation.as_std_vector());
}

/**
 * @brief Single-threaded version of local_apply_perm.
 *
 * Avoids reentering the runtime within a thread.
 *
 * @tparam T Data type of vector elements.
 * @param x The vector to permute (modified in place).
 * @param permutation The permutation to apply.
 */
template <typename T>
void local_apply_perm_single_threaded(Vector<T> &x, const LocalPermutation &permutation) {
    auto new_x = std::make_shared<Vector<T>>(x.size());

    // Apply permutation: new_x[permutation[i]] = x[i]
    for (size_t i = 0; i < x.size(); i++) {
        (*new_x)[permutation[i]] = x[i];
    }

    // Copy back
    for (size_t i = 0; i < x.size(); i++) {
        x[i] = (*new_x)[i];
    }
}

/**
 * @brief Single-threaded overload for both vector and permutation represented as ORQ Vector
 *
 * @tparam T Data type of vector elements.
 * @param x The vector to permute (modified in place).
 * @param permutation The permutation to apply.
 */
template <typename T>
void local_apply_perm_single_threaded(Vector<T> &x, Vector<int> &permutation) {
    local_apply_perm_single_threaded(x, permutation.as_std_vector());
}

/**
 * Catch-all overload for arbitrary vector types and permutation as ORQ Vector
 *
 * @tparam T Data type of vector elements.
 * @tparam R Replication number.
 * @tparam P Permutation type.
 * @param x The vector to permute (modified in place).
 * @param permutation The permutation to apply.
 */
template <typename T, int R, typename P>
void local_apply_perm(EVector<T, R> &x, P &permutation) {
    auto p = [&]() {
        if constexpr (std::is_same_v<P, LocalPermutation>) {
            return permutation;
        } else {
            // Vector<int>
            return permutation.as_std_vector();
        }
    }();

    for (int r = 0; r < R; r++) {
        local_apply_perm(x(r), p);
    }
}

/**
 * Overload when vector is an ElementwisePermutation.
 *
 * @tparam EVector Share container type.
 * @param x The vector to permute (modified in place).
 * @param permutation The permutation to apply.
 */
template <typename EVector>
void local_apply_perm(ElementwisePermutation<EVector> &x, Vector<int> &permutation) {
    local_apply_perm(x.sharedVector, permutation);
}

/**
 * Overload for permutation a SharedVector. Permutation type P can be
 * a LocalPermutation or Vector<int>.
 *
 * @tparam S Share data type.
 * @tparam E Share container type.
 * @tparam P Permutation type.
 * @param x The vector to permute (modified in place).
 * @param permutation The permutation to apply.
 */
template <typename S, typename E, typename P>
void local_apply_perm(SharedVector<S, E> &x, P &permutation) {
    local_apply_perm(x.vector, permutation);
}

//////////////////////////////////////////
// Local Apply Inverse Perm + overloads //
//////////////////////////////////////////

/**
 * @brief Apply the inverse of a local permutation to a vector.
 *
 * NOTE: this can also be implemented by just applying `permutation` as
 * a new mapping. However, this is currently less efficient.
 *
 * @tparam T Data type of vector elements.
 * @tparam S Permutation data type.
 * @param x The vector to permute.
 * @param permutation The permutation whose inverse to apply.
 */
template <typename T, typename S = int>
void local_apply_inverse_perm(Vector<T> &x, const std::vector<S> &permutation) {
    static auto new_x = std::make_unique<Vector<T>>(x.size());
    new_x->resize(x.size());

    auto perm_func = [&](const size_t batch_start, const size_t batch_end) {
        for (size_t i = batch_start; i < batch_end; i++) {
            (*new_x)[i] = x[permutation[i]];
        }
    };

    auto copy_func = [&](const size_t batch_start, const size_t batch_end) {
        for (size_t i = batch_start; i < batch_end; i++) {
            x[i] = (*new_x)[i];
        }
    };

    orq::service::runTime->execute_parallel_unsafe(x.size(), perm_func);
    // copy back in.
    orq::service::runTime->execute_parallel_unsafe(x.size(), copy_func);
}

/**
 * @brief Overload when both are ORQ Vectors.
 *
 * @tparam T Data type of vector elements.
 * @tparam S Permutation data type.
 * @param x The vector to permute.
 * @param permutation The permutation whose inverse to apply.
 */
template <typename T, typename S = int>
void local_apply_inverse_perm(Vector<T> &x, const Vector<S> &permutation) {
    local_apply_inverse_perm(x, permutation.as_std_vector());
}

/**
 * @brief Overload when we are permutation an EVector.
 *
 * @tparam T Data type of vector elements.
 * @tparam R Replication number.
 * @tparam S Permutation data type.
 * @param x The vector to permute.
 * @param permutation The permutation whose inverse to apply.
 */
template <typename T, int R, typename S = int>
void local_apply_inverse_perm(EVector<T, R> &x, const std::vector<S> &permutation) {
    for (int r = 0; r < R; r++) {
        local_apply_inverse_perm(x(r), permutation);
    }
}

/**
 * Overload when vector is a SharedVector.
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @tparam S Permutation data type.
 * @param x The vector to permute.
 * @param permutation The permutation whose inverse to apply.
 */
template <typename Share, typename EVector, typename S = int>
void local_apply_inverse_perm(SharedVector<Share, EVector> &x, const std::vector<S> &permutation) {
    local_apply_inverse_perm(x.vector, permutation);
}

/**
 * Overload when vector is a SharedVector and permutation is a ORQ Vector
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @param x The vector to permute.
 * @param permutation The permutation whose inverse to apply.
 */
template <typename Share, typename EVector>
void local_apply_inverse_perm(SharedVector<Share, EVector> &x, Vector<int> &permutation) {
    local_apply_inverse_perm(x, permutation.as_std_vector());
}

/**
 * Overload when the vector is an ElementwisePermutation and permutation is
 * a ORQ Vector
 *
 * @tparam EVector Share container type.
 * @param x The vector to permute.
 * @param permutation The permutation whose inverse to apply.
 */
template <typename EVector>
void local_apply_inverse_perm(ElementwisePermutation<EVector> &x, Vector<int> &permutation) {
    local_apply_inverse_perm(x.sharedVector, permutation);
}

/**
 * @brief Obliviously apply a sharded secret-shared permutation (Honest Majority).
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @param x The secret-shared vector to permute.
 * @param permutation The permutation to apply.
 */
template <typename Share, typename EVector>
void hm_oblivious_apply_sharded_perm(SharedVector<Share, EVector> &x,
                                     std::shared_ptr<HMShardedPermutation> &permutation) {
    bool binary = x.encoding;

    int pID = orq::service::runTime->getPartyID();
    auto groups = orq::service::runTime->getGroups();

    // apply permutations and reshare
    for (std::set<int> group : groups) {
        if (group.contains(pID)) {
            // apply permutation
            local_apply_perm(x, (*permutation->getPermMap())[group]);
        }

        // reshare
        x.vector.materialize_inplace();
        orq::service::runTime->reshare(x.vector, group, binary);
    }
}

/**
 * @brief Obliviously apply a sharded secret-shared permutation (Honest Majority).
 *
 * This overloaded function just passes the ElementwisePermutation's underlying SharedVector.
 *
 * @param x The ElementwisePermutation to permute.
 * @param permutation The permutation to apply.
 */
template <typename EVector>
void hm_oblivious_apply_sharded_perm(ElementwisePermutation<EVector> &x,
                                     std::shared_ptr<HMShardedPermutation> &permutation) {
    hm_oblivious_apply_sharded_perm(x.sharedVector, permutation);
}

/**
 * Obliviously apply the inverse of a sharded secret-shared permutation.
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @param x The secret-shared vector to permute.
 * @param permutation The permutation whose inverse to apply.
 */
template <typename Share, typename EVector>
void hm_oblivious_apply_inverse_sharded_perm(SharedVector<Share, EVector> &x,
                                             std::shared_ptr<HMShardedPermutation> &permutation) {
    bool binary = x.encoding;

    int pID = orq::service::runTime->getPartyID();
    auto groups = orq::service::runTime->getGroups();
    std::reverse(groups.begin(), groups.end());

    // apply inverse permutations in reverse order and reshare
    for (std::set<int> group : groups) {
        if (group.contains(pID)) {
            // apply inverse permutation
            local_apply_inverse_perm(x, (*permutation->getPermMap())[group]);
        }

        // reshare
        x.vector.materialize_inplace();
        orq::service::runTime->reshare(x.vector, group, binary);
    }
}

/**
 * @brief Obliviously apply the inverse of a sharded secret-shared permutation (Honest Majority).
 *
 * @tparam EVector Share container type.
 * @param x The ElementwisePermutation to permute.
 * @param permutation The permutation whose inverse to apply.
 */
template <typename EVector>
void hm_oblivious_apply_inverse_sharded_perm(ElementwisePermutation<EVector> &x,
                                             std::shared_ptr<HMShardedPermutation> &permutation) {
    hm_oblivious_apply_inverse_sharded_perm(x.sharedVector, permutation);
}

/**
 * @brief Apply a permutation correlation to a secret-shared vector (2PC only).
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @param x The secret-shared vector to permute and share.
 * @param perm The permutation correlation.
 * @param send_party The index of the party acting as the sender.
 */
template <typename Share, typename EVector>
void permute_and_share(SharedVector<Share, EVector> &x,
                       std::shared_ptr<DMShardedPermutation<Share>> &perm, int send_party) {
    size_t n = x.size();
    int rank = runTime->getPartyID();
    bool sender = (rank == send_party);
    bool receiver = !sender;
    orq::Encoding encoding = perm->getEncoding();

    auto [pi, A, B, C] = *(perm->getTuple());

    if (receiver) {
        // receiver blinds their share
        // delta = [x]_r - A
        if (encoding == orq::Encoding::BShared) {
            x.vector(0) ^= A;
        } else {
            x.vector(0) -= A;
        }

        // x might have an access pattern applied.
        x.vector.materialize_inplace();

        // receiver sends delta to sender
        runTime->comm0()->sendShares(x.vector(0), 1, n);

        // receiver sets share equal to B
        // [x]_r = B
        x.vector(0) = B;
    }

    if (sender) {
        // sender receives delta from receiver
        Vector<Share> delta(n);
        runTime->comm0()->receiveShares(delta, 1, n);

        // sender permutes delta under pi
        local_apply_perm(delta, pi);

        // C' = pi(delta) + C
        if (encoding == orq::Encoding::BShared) {
            delta ^= C;
        } else {
            delta += C;
        }

        // output C'
        // [x]_s = pi([x]_s) + C'
        local_apply_perm(x, pi);
        if (encoding == orq::Encoding::BShared) {
            x.vector(0) ^= delta;
        } else {
            x.vector(0) += delta;
        }
    }
}

/**
 * @brief Apply the inverse of a permutation correlation to a secret-shared vector (2PC only).
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @param x The secret-shared vector to permute and share.
 * @param perm The permutation correlation whose inverse to apply.
 * @param send_party The index of the party acting as the sender.
 */
template <typename Share, typename EVector>
void permute_and_share_inverse(SharedVector<Share, EVector> &x,
                               std::shared_ptr<DMShardedPermutation<Share>> &perm, int send_party) {
    size_t n = x.size();
    int rank = runTime->getPartyID();
    bool sender = (rank == send_party);
    bool receiver = !sender;
    orq::Encoding encoding = perm->getEncoding();

    auto [pi, A, B, C] = *(perm->getTuple());

    if (receiver) {
        // receiver blinds their share
        // delta = [x]_r - B
        if (encoding == orq::Encoding::BShared) {
            x.vector(0) ^= B;
        } else {
            x.vector(0) -= B;
        }

        // x might have an access pattern applied.
        x.vector.materialize_inplace();

        // receiver sends delta to sender
        runTime->comm0()->sendShares(x.vector(0), 1, n);

        // receiver sets share equal to A
        // [x]_r = A
        x.vector(0) = A;
    }

    if (sender) {
        // sender receives delta from receiver
        Vector<Share> delta(n);
        runTime->comm0()->receiveShares(delta, 1, n);

        // delta' = delta - C
        if (encoding == orq::Encoding::BShared) {
            delta ^= C;
        } else {
            delta -= C;
        }

        // sender permutes delta' under pi inverse
        local_apply_inverse_perm(delta, pi);

        // output delta prime under pi inverse
        // [x]_s = pi^-1([x]_s) + delta'
        local_apply_inverse_perm(x, pi);
        if (encoding == orq::Encoding::BShared) {
            x.vector(0) ^= delta;
        } else {
            x.vector(0) += delta;
        }
    }
}

/**
 * @brief Obliviously apply a sharded secret-shared permutation (Dishonest Majority).
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @param x The secret-shared vector to permute.
 * @param permutation The permutation to apply.
 */
template <typename Share, typename EVector>
void dm_oblivious_apply_sharded_perm(SharedVector<Share, EVector> &x,
                                     std::shared_ptr<DMShardedPermutation<Share>> &permutation) {
    permute_and_share(x, permutation, 0);
    permute_and_share(x, permutation, 1);
}

/**
 * @brief Obliviously apply the inverse of a sharded secret-shared permutation
 *      (Dishonest Majority).
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @param x The secret-shared vector to permute.
 * @param permutation The permutation whose inverse to apply.
 */
template <typename Share, typename EVector>
void dm_oblivious_apply_inverse_sharded_perm(
    SharedVector<Share, EVector> &x, std::shared_ptr<DMShardedPermutation<Share>> &permutation) {
    permute_and_share_inverse(x, permutation, 1);
    permute_and_share_inverse(x, permutation, 0);
}

/**
 * @brief Protocol agnostic function to apply sharded permutations.
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @param x The secret-shared vector to permute.
 * @param perm The permutation to apply.
 */
template <typename Share, typename EVector>
void oblivious_apply_sharded_perm(SharedVector<Share, EVector> &x,
                                  std::shared_ptr<ShardedPermutation> &perm) {
#ifdef INSTRUMENT_APPLYPERM
    count_oblivious_apply_perm<Share>(x.size());
#endif

    if (auto hm_perm = std::dynamic_pointer_cast<HMShardedPermutation>(perm)) {
        // honest majority
        hm_oblivious_apply_sharded_perm(x, hm_perm);
    } else if (auto dm_perm = std::dynamic_pointer_cast<DMShardedPermutation<Share>>(perm)) {
        // dishonest majority
        dm_oblivious_apply_sharded_perm(x, dm_perm);
    } else {
#ifndef MPC_PROTOCOL_DUMMY_ZERO
        // Handle the case where the type is neither HMShardedPermutation nor
        // DMShardedPermutation
        throw std::runtime_error("Unsupported ShardedPermutation type");
#endif
    }
}

/**
 * @brief Protocol agnostic function to apply sharded permutations.
 *
 * @tparam EVector Share container type.
 * @param x The secret-shared vector to permute.
 * @param permutation The permutation to apply.
 *
 *  This overloaded function just passes the ElementwisePermutation's underlying
 * SharedVector.
 */
template <typename EVector>
void oblivious_apply_sharded_perm(ElementwisePermutation<EVector> &x,
                                  std::shared_ptr<ShardedPermutation> &permutation) {
    oblivious_apply_sharded_perm(x.sharedVector, permutation);
}

/**
 * @brief Protocol agnostic function to apply sharded permutations.
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @param x The secret-shared vector to permute.
 * @param perm The permutation whose inverse to apply.
 */
template <typename Share, typename EVector>
void oblivious_apply_inverse_sharded_perm(SharedVector<Share, EVector> &x,
                                          std::shared_ptr<ShardedPermutation> &perm) {
#ifdef INSTRUMENT_APPLYPERM
    count_oblivious_apply_perm<Share>(x.size());
#endif
    if (auto hm_perm = std::dynamic_pointer_cast<HMShardedPermutation>(perm)) {
        // honest majority
        hm_oblivious_apply_inverse_sharded_perm(x, hm_perm);
    } else if (auto dm_perm = std::dynamic_pointer_cast<DMShardedPermutation<Share>>(perm)) {
        // dishonest majority
        dm_oblivious_apply_inverse_sharded_perm(x, dm_perm);
    } else {
#ifndef MPC_PROTOCOL_DUMMY_ZERO
        // Handle the case where the type is neither HMShardedPermutation nor
        // DMShardedPermutation
        throw std::runtime_error("Unsupported ShardedPermutation type");
#endif
    }
}

/**
 * @brief Protocol agnostic function to apply inversesharded permutations.
 *
 * @tparam EVector Share container type.
 * @param x The secret-shared vector to permute.
 * @param permutation The permutation whose inverse to apply.
 */
template <typename EVector>
void oblivious_apply_inverse_sharded_perm(ElementwisePermutation<EVector> &x,
                                          std::shared_ptr<ShardedPermutation> &permutation) {
    oblivious_apply_inverse_sharded_perm(x.sharedVector, permutation);
}

/**
 * @brief Obliviously apply an elementwise secret-shared permutation.
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @tparam EVectorPerm Permutation type.
 * @param x The secret-shared vector to permute.
 * @param perm The permutation to apply.
 */
template <typename Share, typename EVector, typename EVectorPerm>
void oblivious_apply_elementwise_perm(SharedVector<Share, EVector> &x,
                                      ElementwisePermutation<EVectorPerm> &perm) {
    // make a deep copy of the permutation
    ElementwisePermutation<EVectorPerm> permutation(perm);

    // generate a pair of random sharded permutations
    auto [pi_1, pi_2] = PermutationManager::get()->getNextPair<Share, int>(x.size(), x.encoding,
                                                                           perm.getEncoding());

    // shuffle both the vector and the permutation according to pi
    oblivious_apply_sharded_perm(x, pi_1);
    oblivious_apply_sharded_perm(permutation, pi_2);

    // open pi(perm)
    Vector<int> pi_perm = permutation.open();

    // locally apply pi(perm) to pi(x)
    local_apply_perm(x, pi_perm);
}

/**
 * @brief Compose two elementwise secret-shared permutations.
 *
 * @tparam EVector Share container type.
 * @param sigma The first permutation to be applied in the composition.
 * @param rho The second permutation to be applied in the composition.
 * @return An elementwise secret-sharing of the composed permutation.
 */
template <typename EVector>
ElementwisePermutation<EVector> compose_permutations(ElementwisePermutation<EVector> &sigma,
                                                     ElementwisePermutation<EVector> &rho) {
    assert(sigma.getEncoding() == rho.getEncoding());

    // generate a random sharded permutation
    std::shared_ptr<ShardedPermutation> pi =
        PermutationManager::get()->getNext<int32_t>(sigma.size(), sigma.getEncoding());

    // apply the random permutation pi to sigma
    // sigma' := pi(sigma) = sigma * pi_inverse (observation 2.4)
    oblivious_apply_sharded_perm(sigma, pi);

    // open pi(sigma) = sigma * pi_inverse
    Vector<int> pi_sigma = sigma.open();

    // apply inverse(pi(sigma)) to rho
    // rho' := inverse(pi(sigma))(rho) = (pi * sigma_inverse)(rho) = rho * sigma
    // * pi_inverse
    local_apply_inverse_perm(rho, pi_sigma);

    // apply the inverse random permutation pi to pi(sigma(rho))
    // rho'' := pi_inverse(rho) = rho' * pi = rho * sigma * pi_inverse * pi =
    // rho * sigma
    oblivious_apply_inverse_sharded_perm(rho, pi);

    return rho;
}

/**
 * @brief Obliviously shuffle a secret-shared vector.
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @param x The vector to shuffle.
 */
template <typename Share, typename EVector>
static void shuffle(SharedVector<Share, EVector> &x) {
    std::shared_ptr<ShardedPermutation> sharded_perm =
        PermutationManager::get()->getNext<Share>(x.size(), x.encoding);
    oblivious_apply_sharded_perm(x, sharded_perm);
}

/**
 * @brief Obliviously shuffle a table of secret-shared vectors.
 *
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 * @param _data_a List of pointers to all arithmetic columns.
 * @param _data_b List of pointers to all binary columns.
 * @param size Size of the vectors to shuffle.
 */
template <typename Share, typename EVector>
static void shuffle(std::vector<ASharedVector<Share, EVector> *> _data_a,
                    std::vector<BSharedVector<Share, EVector> *> _data_b, size_t size) {
    // generate a random sharded permutation and use it to generate a random
    // elementwise permutation
    std::shared_ptr<ShardedPermutation> sharded_perm =
        PermutationManager::get()->getNext<int32_t>(size, orq::Encoding::BShared);
    ElementwisePermutation<EVector> permutation(size, orq::Encoding::BShared);
    oblivious_apply_sharded_perm(permutation, sharded_perm);

    // apply the shared permutation to all arithmetic columns
    for (ASharedVector<Share, EVector> *a_column : _data_a) {
        oblivious_apply_elementwise_perm(*a_column, permutation);
    }
    // apply the shared permutation to all binary columns
    for (BSharedVector<Share, EVector> *b_column : _data_b) {
        oblivious_apply_elementwise_perm(*b_column, permutation);
    }
}
}  // namespace orq::operators
