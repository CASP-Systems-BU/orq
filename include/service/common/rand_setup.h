#pragma once

#include <sodium.h>
#include <unistd.h>

#include <set>
#include <typeindex>

#include "../../core/random/permutations/dm_dummy.h"
#include "../../mpc.h"

namespace secrecy::service {

/**
 * Utility function to setup a CommonPRG among a group.
 * @param rank - The absolute rank of the current party.
 * @param group - The group that shares the CommonPRG.
 * @param commonPRGManager - The CommonPRGManager to add the new CommonPRG to.
 */
void create_common_prg_among_group(
    int rank, std::set<int> group,
    std::shared_ptr<secrecy::random::CommonPRGManager> commonPRGManager) {
    // only members of the group should participate
    // if not in the group, just return
    if (!group.contains(rank)) {
        return;
    }

    unsigned char seed[crypto_aead_aes256gcm_KEYBYTES];

    // lowest rank party in the group determines the seed
    int lowestRank = *group.begin();  // this is deterministic as sets are sorted

    if (rank == lowestRank) {
        // generate a seed
        secrecy::random::AESPRGAlgorithm::aesKeyGen(seed);
        // convert unsigned char to uint8_t vector to be sent
        std::vector<int8_t> seed_bytes;
        for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
            seed_bytes.push_back((int8_t)seed[byte]);
        }
        secrecy::Vector<int8_t> seed_to_send(seed_bytes);

        // send to all parties
        for (int other_rank : group) {
            if (rank == other_rank) continue;
            int relative_rank = other_rank - rank;
            secrecy::Vector<int8_t> empty(crypto_aead_aes256gcm_KEYBYTES);
            secrecy::service::runTime->comm0()->exchangeShares(
                seed_to_send, empty, relative_rank, relative_rank, crypto_aead_aes256gcm_KEYBYTES);
        }
    } else {
        // all other parties receive
        secrecy::Vector<int8_t> remote(crypto_aead_aes256gcm_KEYBYTES);
        int relative_rank = lowestRank - rank;

        secrecy::Vector<int8_t> empty(crypto_aead_aes256gcm_KEYBYTES);
        secrecy::service::runTime->comm0()->exchangeShares(
            empty, remote, relative_rank, relative_rank, crypto_aead_aes256gcm_KEYBYTES);

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
    std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm = std::make_unique<secrecy::random::AESPRGAlgorithm>(seed_vec);
    auto commonPRG = std::make_shared<secrecy::random::CommonPRG>(std::move(prg_algorithm), rank);
    commonPRGManager->add(commonPRG, group);
}

// NOTE: maybe these setup subroutines should be a part of the protocol?
// Or, common functionality can be broken out into a function here, and protocol
// specific stuff moved into the protocol itself. ~ eli

// setup function for the CommonPRGs in replicated 3PC
// TODO: should use own communicator
void setup_3pc_common_prgs(int rank,
                           std::shared_ptr<secrecy::random::CommonPRGManager> commonPRGManager) {
    /*
        generate and exchange seeds
    */
    // generate a seed
    unsigned char local_key[crypto_aead_aes256gcm_KEYBYTES];
    secrecy::random::AESPRGAlgorithm::aesKeyGen(local_key);
    // convert unsigned char to uint8_t vector to be exchanged
    // TODO: this conversion can probably be done more easily
    std::vector<int8_t> local_key_bytes;
    for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
        local_key_bytes.push_back((int8_t)local_key[byte]);
    }
    // exchange keys
    secrecy::Vector<int8_t> local(local_key_bytes);
    secrecy::Vector<int8_t> remote(crypto_aead_aes256gcm_KEYBYTES);
    secrecy::service::runTime->comm0()->exchangeShares(local, remote, +1, -1,
                                                       crypto_aead_aes256gcm_KEYBYTES);
    // convert uint8_t vector back to unsigned char for key
    unsigned char remote_key[crypto_aead_aes256gcm_KEYBYTES];
    for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
        remote_key[byte] = (unsigned char)remote[byte];
    }

    /*
        create the CommonPRG objects
    */
    for (int j = 0; j < 3; j++) {
        if (j == rank) continue;

        int relative_rank = (3 + j - rank) % 3;

        unsigned char seed[crypto_aead_aes256gcm_KEYBYTES];
        if (relative_rank == 1) {
            // seed = local_key
            for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
                seed[byte] = local_key[byte];
            }
        } else if (relative_rank == 2) {
            // seed = remote_key
            for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
                seed[byte] = remote_key[byte];
            }
        }

        std::vector<unsigned char> seed_vec;
        for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
            seed_vec.push_back(seed[byte]);
        }

        std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm = std::make_unique<secrecy::random::AESPRGAlgorithm>(seed_vec);
        auto commonPRG = std::make_shared<secrecy::random::CommonPRG>(std::move(prg_algorithm), rank);
        commonPRGManager->add(commonPRG, relative_rank);
    }
}

// setup function for the CommonPRGs in Fantastic 4PC
void setup_4pc_common_prgs(int rank,
                           std::shared_ptr<secrecy::random::CommonPRGManager> commonPRGManager) {
    /*
        generate and exchange seeds
    */
    // generate a seed
    unsigned char local_key[crypto_aead_aes256gcm_KEYBYTES];
    secrecy::random::AESPRGAlgorithm::aesKeyGen(local_key);
    // convert unsigned char to uint8_t vector to be exchanged
    // TODO: this conversion can probably be done more easily
    std::vector<int8_t> local_key_bytes;
    for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
        local_key_bytes.push_back((int8_t)local_key[byte]);
    }

    /*
        create the CommonPRG objects
    */
    for (int j = 0; j < 4; j++) {
        if (j == rank) continue;

        // prepare to exchange keys
        secrecy::Vector<int8_t> local(local_key_bytes);
        secrecy::Vector<int8_t> remote(crypto_aead_aes256gcm_KEYBYTES);

        // this is just a way of identifying which parties the CommonPRG is shared with
        // unlike in 3PC, this is now the relative_rank of the party the CommonPRG is NOT shared
        // with
        int relative_rank = (4 + j - rank) % 4;

        unsigned char seed[crypto_aead_aes256gcm_KEYBYTES];

        // party for which relative_rank = 1 shares key with other 2 parties
        if (relative_rank == 1) {
            // seed = local_key
            // send to the two other parties, who have relative ranks of -1 and -2
            secrecy::service::runTime->comm0()->sendShares(local, -1,
                                                           crypto_aead_aes256gcm_KEYBYTES);
            secrecy::service::runTime->comm0()->sendShares(local, -2,
                                                           crypto_aead_aes256gcm_KEYBYTES);
            for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
                seed[byte] = local_key[byte];
            }
        } else if (relative_rank == 2) {
            // seed = remote_key
            // get from party with relative rank +1
            secrecy::service::runTime->comm0()->receiveShares(remote, +1,
                                                              crypto_aead_aes256gcm_KEYBYTES);
            for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
                seed[byte] = (unsigned char)remote[byte];
            }
        } else if (relative_rank == 3) {
            // seed = remote_key
            // get from party with relative rank +2
            secrecy::service::runTime->comm0()->receiveShares(remote, +2,
                                                              crypto_aead_aes256gcm_KEYBYTES);
            for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
                seed[byte] = (unsigned char)remote[byte];
            }
        }

        std::vector<unsigned char> seed_vec;
        for (int byte = 0; byte < crypto_aead_aes256gcm_KEYBYTES; byte++) {
            seed_vec.push_back(seed[byte]);
        }

        std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm = std::make_unique<secrecy::random::AESPRGAlgorithm>(seed_vec);
        auto commonPRG = std::make_shared<secrecy::random::CommonPRG>(std::move(prg_algorithm), rank);
        commonPRGManager->add(commonPRG, relative_rank);
    }
}

void setup_2pc_common_prgs(int rank,
                           std::shared_ptr<secrecy::random::CommonPRGManager> commonPRGManager) {
    auto common_prg = commonPRGManager->get({0, 1});

    // The commonprg held by both parties is also the _relative_ common prg with
    // its neighbor (used for zero sharing)
    commonPRGManager->add(common_prg, 1);
}

using cg_map_t = std::map<std::tuple<std::type_index, secrecy::random::Correlation>,
                          secrecy::random::CorrelationGenerator*>;

#ifdef MPC_PROTOCOL_BEAVER_TWO
template <typename T>
void setup_2pc_correlation_generators(cg_map_t& cg, int rank,
                                      std::shared_ptr<secrecy::random::CommonPRGManager> PRGm,
                                      secrecy::Communicator* comm, int thread) {
    using namespace secrecy::random;

    auto& Ti = __typeid(T);

#if defined USE_DUMMY_TRIPLES
    auto vg_a = new DummyOLE<T, secrecy::Encoding::AShared>(rank, PRGm, comm);
    auto vg_b = new DummyOLE<T, secrecy::Encoding::BShared>(rank, PRGm, comm);
    cg[{Ti, Correlation::BeaverMulTriple}] =
        new BeaverTripleGenerator<T, secrecy::Encoding::AShared>(vg_a);
    cg[{Ti, Correlation::BeaverAndTriple}] =
        new BeaverTripleGenerator<T, secrecy::Encoding::BShared>(vg_b);
#elif defined USE_ZERO_TRIPLES
    auto vg_a = new ZeroOLE<T, secrecy::Encoding::AShared>(rank, comm);
    auto vg_b = new ZeroOLE<T, secrecy::Encoding::BShared>(rank, comm);
    cg[{Ti, Correlation::BeaverMulTriple}] =
        new BeaverTripleGenerator<T, secrecy::Encoding::AShared>(vg_a);
    cg[{Ti, Correlation::BeaverAndTriple}] =
        new BeaverTripleGenerator<T, secrecy::Encoding::BShared>(vg_b);
#else
    // Use real generators - silent OLE and silent OT
    auto vg_a = new SilentOLE<T>(rank, comm, thread);
    auto vg_b = new SilentOT<T>(rank, comm, thread);
    auto pooled_ole = make_pooled<SilentOLE<T>>(rank, comm, thread);
    auto pooled_ot = make_pooled<SilentOT<T>>(rank, comm, thread);
    cg[{Ti, Correlation::BeaverMulTriple}] =
        new BeaverTripleGenerator<T, secrecy::Encoding::AShared>(pooled_ole, comm);
    cg[{Ti, Correlation::BeaverAndTriple}] =
        new BeaverTripleGenerator<T, secrecy::Encoding::BShared>(pooled_ot, comm);
#endif

    cg[{Ti, Correlation::OLE}] = vg_a;
    cg[{Ti, Correlation::rOT}] = vg_b;
}
#endif

// general function for setting up random generators
auto setup_random_generation(int num_parties, int rank, std::vector<std::set<int>> groups,
                             int thread) {
    // create the local (private) PRG
    // TODO make this a PRGAlgorithm
    auto localPRG = std::make_shared<secrecy::random::CommonPRG>();

    // create the CommonPRGManager
    auto commonPRGManager = std::make_shared<secrecy::random::CommonPRGManager>(num_parties);

    // add to the CommonPRGManager for each group
    for (std::set<int> group : groups) {
        create_common_prg_among_group(rank, group, commonPRGManager);
    }

    // Add the everyone group
    std::set<int> _everyone;
    for (int i = 0; i < num_parties; i++) {
        _everyone.insert(i);
    }
    create_common_prg_among_group(rank, _everyone, commonPRGManager);

    cg_map_t cg;

    // create the relative CommonPRG objects
    if (num_parties == 2) {
#ifdef MPC_PROTOCOL_BEAVER_TWO
        setup_2pc_common_prgs(rank, commonPRGManager);

        auto comm = runTime->workers[thread].getCommunicator();
        setup_2pc_correlation_generators<int8_t>(cg, rank, commonPRGManager, comm, thread);
        setup_2pc_correlation_generators<int16_t>(cg, rank, commonPRGManager, comm, thread);
        setup_2pc_correlation_generators<int32_t>(cg, rank, commonPRGManager, comm, thread);
        setup_2pc_correlation_generators<int64_t>(cg, rank, commonPRGManager, comm, thread);
        setup_2pc_correlation_generators<__int128_t>(cg, rank, commonPRGManager, comm, thread);

        // permutation generator
#if defined USE_DUMMY_TRIPLES || defined USE_ZERO_TRIPLES
        auto sharded_generator =
            new DMDummyGenerator<__int128_t>(rank, thread, commonPRGManager, comm);
#else
        // Use real permutation generator
        auto sharded_generator =
            new DMPermutationCorrelationGenerator<__int128_t>(rank, thread, commonPRGManager, comm);
#endif
        cg[{__typeid(int8_t), Correlation::ShardedPermutation}] = sharded_generator;
        cg[{__typeid(int16_t), Correlation::ShardedPermutation}] = sharded_generator;
        cg[{__typeid(int32_t), Correlation::ShardedPermutation}] = sharded_generator;
        cg[{__typeid(int64_t), Correlation::ShardedPermutation}] = sharded_generator;
        cg[{__typeid(__int128_t), Correlation::ShardedPermutation}] = sharded_generator;
#endif

    } else if (num_parties == 3) {
        setup_3pc_common_prgs(rank, commonPRGManager);
    } else if (num_parties == 4) {
        setup_4pc_common_prgs(rank, commonPRGManager);
    }

    if ((num_parties == 3) || (num_parties == 4)) {
        std::shared_ptr<secrecy::random::CommonPRGManager> commonPRGManager_ptr(commonPRGManager);
        auto sharded_generator =
            new HMShardedPermutationGenerator(rank, commonPRGManager_ptr, groups);
        cg[{__typeid(int8_t), Correlation::ShardedPermutation}] =
            sharded_generator;
        cg[{__typeid(int16_t), Correlation::ShardedPermutation}] =
            sharded_generator;
        cg[{__typeid(int32_t), Correlation::ShardedPermutation}] =
            sharded_generator;
        cg[{__typeid(int64_t), Correlation::ShardedPermutation}] =
            sharded_generator;
        cg[{__typeid(__int128_t), Correlation::ShardedPermutation}] =
            sharded_generator;
    }

    // create the zero sharing generator
    auto zeroSharingGenerator = std::make_shared<secrecy::random::ZeroSharingGenerator>(
        num_parties, commonPRGManager, rank);

    // create the randomness manager to access all generators
    return std::make_unique<secrecy::random::RandomnessManager>(localPRG, commonPRGManager,
                                                                zeroSharingGenerator, cg);
}

}  // namespace secrecy::service
