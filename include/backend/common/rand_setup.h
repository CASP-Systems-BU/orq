#pragma once

#include <sodium.h>
#include <unistd.h>

#include <algorithm>
#include <set>
#include <typeindex>

#ifdef USE_LIBOTE
#include "backend/common/libote_io.h"
#endif

#include "core/random/permutations/dm_dummy.h"
#include "core/random/prg/random_generator.h"

namespace orq::service {

/**
 * Utility function to setup a CommonPRG among a group of semihonest parties.
 * @param rank The absolute rank of the current party.
 * @param group The group that shares the CommonPRG.
 */
std::shared_ptr<CommonPRG> create_common_prg_among_group(int rank, std::set<int> group) {
    assert(!MALICIOUS_PROTOCOL);
    // only members of the group should participate
    // if not in the group, just return
    if (!group.contains(rank)) {
        return {};
    }

    unsigned char seed[crypto_aead_aes256gcm_KEYBYTES];

    // lowest rank party in the group determines the seed
    int lowestRank = *group.begin();  // this is deterministic as sets are sorted

    if (rank == lowestRank) {
        // generate a seed
        orq::random::AESPRGAlgorithm::aesKeyGen(seed);
        // convert unsigned char to uint8_t vector to be sent
        std::vector<int8_t> seed_bytes;
        for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
            seed_bytes.push_back((int8_t)seed[byte]);
        }
        orq::Vector<int8_t> seed_to_send(seed_bytes);

        // send to all parties
        for (int other_rank : group) {
            if (rank == other_rank) continue;
            int relative_rank = other_rank - rank;
            orq::Vector<int8_t> empty(crypto_aead_aes256gcm_KEYBYTES);
            orq::service::runTime->comm0()->exchangeShares(seed_to_send, empty, relative_rank,
                                                           relative_rank);
        }
    } else {
        // all other parties receive
        orq::Vector<int8_t> remote(crypto_aead_aes256gcm_KEYBYTES);
        int relative_rank = lowestRank - rank;

        orq::Vector<int8_t> empty(crypto_aead_aes256gcm_KEYBYTES);
        orq::service::runTime->comm0()->exchangeShares(empty, remote, relative_rank, relative_rank);

        // convert uint8_t vector back to unsigned char for key
        for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
            seed[byte] = (unsigned char)remote[byte];
        }
    }

    std::vector<unsigned char> seed_vec;
    for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
        seed_vec.push_back(seed[byte]);
    }

    // by now, all parties agree on a seed
    std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm =
        std::make_unique<orq::random::AESPRGAlgorithm>(seed_vec);

    return std::make_shared<orq::random::CommonPRG>(std::move(prg_algorithm), rank);
}

#ifdef MPC_PROTOCOL_BEAVER_TWO

/**
 * @brief Type for arithmetic OLE generators. We can either use quadratic-bandwidth Gilboa, or the
 * CRT-based optimization. Use the flag in debug.h to change between these.
 *
 * @tparam T
 */
template <typename T>
using ole_generator_t = std::conditional_t<USE_GILBOA_CRT, GilboaCRT<T>, GilboaOLE<T>>;

using OLEmodP_t = orq::random::OLEmodPrimeInterface<uint8_t>;

/**
 * @brief Setup correlation generators for 2PC
 *
 * @tparam T
 * @param rank of this node
 * @param PRGm CommonPRGManager
 * @param comm communicator
 * @param thread thread index
 * @return Typed correlations for type T
 */
template <typename T>
orq::random::TypedCorrelations_t<T> setup_2pc_correlations(
    int rank, std::shared_ptr<orq::random::CommonPRGManager> PRGm, orq::Communicator* comm,
    std::shared_ptr<OLEmodP_t> mod_ptr, int thread) {
    using namespace orq::random;

#if defined USE_DUMMY_TRIPLES
    auto vg_a = std::make_shared<DummyOLE<T>>(rank, PRGm, comm);
    auto vg_b = std::make_shared<DummyOT<T>>(rank, PRGm, comm);
    auto btg_a = std::make_shared<BeaverTripleGenerator<T, orq::Encoding::AShared>>(vg_a);
    auto btg_b = std::make_shared<BeaverTripleGenerator<T, orq::Encoding::BShared>>(vg_b);
    return std::make_tuple(vg_b, vg_a, btg_a, btg_b, nullptr);
#elif defined USE_ZERO_TRIPLES
    auto vg_a = std::make_shared<ZeroOLE<T>>(rank, comm);
    auto vg_b = std::make_shared<ZeroOLE<T>>(rank, comm);
    auto btg_a = std::make_shared<BeaverTripleGenerator<T, orq::Encoding::AShared>>(vg_a);
    auto btg_b = std::make_shared<BeaverTripleGenerator<T, orq::Encoding::BShared>>(vg_b);
    return std::make_tuple(vg_b, vg_a, btg_a, btg_b, nullptr);
#else

#if defined(USE_LIBOTE) && defined(USE_SECURE_JOIN)
    std::shared_ptr<orq::random::OPRF> oprf;
    if constexpr (std::is_same_v<T, __int128_t>) {
        oprf = std::make_shared<orq::random::OPRF>(rank, thread, comm->host_prefix);
    }
#else
    auto oprf = nullptr;
#endif

    std::shared_ptr<ole_generator_t<T>> vg_a;

    if constexpr (USE_GILBOA_CRT) {
        assert(mod_ptr != nullptr);
        // All Gilboa CRT instantiations use the same underlying small Gilboa
        vg_a = std::make_shared<ole_generator_t<T>>(mod_ptr, PRGm);
    } else {
        // GILBOA_CRT is false, so we're using the regular (full-width) Gilboa
        vg_a = std::make_shared<ole_generator_t<T>>(rank, PRGm, comm, thread);
    }

    // Use real generators - gilboa OLE and silent OT
    auto vg_b = std::make_shared<SilentOT<T>>(rank, PRGm, comm, thread);
    // pooled variants
    auto pooled_ole = make_pooled<ole_generator_t<T>>(vg_a);
    auto pooled_ot = make_pooled<SilentOT<T>>(vg_b);
    auto btg_a =
        std::make_shared<BeaverTripleGenerator<T, orq::Encoding::AShared>>(pooled_ole, comm);
    auto btg_b =
        std::make_shared<BeaverTripleGenerator<T, orq::Encoding::BShared>>(pooled_ot, comm);
    return std::make_tuple(vg_b, vg_a, btg_a, btg_b, oprf);
#endif
}
#endif

/**
 * @brief General randomness-generation setup
 *
 * @param num_parties
 * @param rank
 * @param groups
 * @param thread
 * @return auto
 */
auto setup_random_generation(int num_parties, int rank, std::vector<std::set<int>> groups,
                             int thread) {
    auto r = sodium_init();
    assert(r != -1);

    // Ensure the global Boost.Asio io_context is initialised once with the
    // number of worker threads that the runtime was configured with.
    int num_threads = orq::service::runTime->getArg<int>("threads", "t", 1);
    int num_libote_threads = orq::service::runTime->getArg<int>("comm-threads", "n", num_threads);
#ifdef USE_LIBOTE
    orq::libote_io::libOTeContext::getContext(num_libote_threads);
#endif

#ifdef MPC_PROTOCOL_FANTASTIC_FOUR
    auto rel_mode = orq::random::CommonPRGManager::RelativeRankMode::EXCLUDED;
#else
    auto rel_mode = orq::random::CommonPRGManager::RelativeRankMode::INCLUDED;
#endif

    auto commonPRGManager =
        std::make_shared<orq::random::CommonPRGManager>(num_parties, rank, rel_mode);

    // Only malicious protocols need committed seed queues
    std::unique_ptr<random::CommittedSeedsQueue<>> csq;
    if constexpr (MALICIOUS_PROTOCOL) {
        csq = std::make_unique<random::CommittedSeedsQueue<>>(
            runTime->comm0(), commonPRGManager->get({}), rank, num_parties);

        csq->repopulateQueue();
    }

    // Add to the CommonPRGManager for each group
    // For semihonest protocols, just generate a random seed and send it around
    for (std::set<int> group : groups) {
        auto c = MALICIOUS_PROTOCOL ? csq->nextPRGforGroup(group)
                                    : create_common_prg_among_group(rank, group);
        commonPRGManager->add(c, group);
    }

    CorrRegistry_t registry;
    std::shared_ptr<orq::random::ShardedPermutationGenerator> sharded_perm_gen;

    // Beaver / Replicated
    // create the relative CommonPRG objects
    if (num_parties == 2) {
#ifdef MPC_PROTOCOL_BEAVER_TWO
        std::shared_ptr<OLEmodP_t> modPptr = nullptr;
        auto comm = runTime->workers[thread].getCommunicator();

#ifdef USE_LIBOTE
        if constexpr (USE_GILBOA_CRT) {
            if (thread == 0) {
                single_cout("[ORQ] NOTE: Using Gilboa CRT");
            }

            // Make a single GilboaModPrime per thread. We'll pass this into the GilboaCRT for each
            // datatype. Different data types cannot be running at the same time, so this won't
            // cause any contention.
            auto GmP = new orq::random::GilboaModPrime(rank, commonPRGManager, comm, thread);
            modPptr = std::shared_ptr<OLEmodP_t>(GmP);
        }
#endif

        auto g8 = setup_2pc_correlations<int8_t>(rank, commonPRGManager, comm, modPptr, thread);
        auto g16 = setup_2pc_correlations<int16_t>(rank, commonPRGManager, comm, modPptr, thread);
        auto g32 = setup_2pc_correlations<int32_t>(rank, commonPRGManager, comm, modPptr, thread);
        auto g64 = setup_2pc_correlations<int64_t>(rank, commonPRGManager, comm, modPptr, thread);
        auto g128 =
            setup_2pc_correlations<__int128_t>(rank, commonPRGManager, comm, modPptr, thread);

        registry = std::make_tuple(std::move(g8), std::move(g16), std::move(g32), std::move(g64),
                                   std::move(g128));

        // permutation generator
#if defined USE_DUMMY_TRIPLES || defined USE_ZERO_TRIPLES || !defined USE_SECURE_JOIN
        sharded_perm_gen = std::make_shared<orq::random::DMDummyGenerator<__int128_t>>(
            rank, thread, commonPRGManager, comm);
#else
        // Use real permutation generator
        sharded_perm_gen =
            std::make_shared<orq::random::DMPermutationCorrelationGenerator<__int128_t>>(
                rank, thread, commonPRGManager, comm);
#endif
#endif
    }

    if ((num_parties == 3) || (num_parties == 4)) {
        sharded_perm_gen = std::make_shared<orq::random::HMShardedPermutationGenerator>(
            rank, commonPRGManager, groups);
    }

    // create the zero sharing generator
    auto zeroSharingGenerator =
        std::make_shared<orq::random::ZeroSharingGenerator>(num_parties, commonPRGManager, rank);

    // create the randomness manager to access all generators
    return std::make_unique<orq::random::RandomnessManager>(commonPRGManager, zeroSharingGenerator,
                                                            std::move(registry), std::move(csq),
                                                            sharded_perm_gen);
}

}  // namespace orq::service
