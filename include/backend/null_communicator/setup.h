#pragma once

#include <set>

#include "backend/common/runtime.h"
#include "mpc.h"

// #define FIXED_SEED {171, 207, 132, 64}

namespace orq::service::null_service {

namespace {
    template <typename PRG = PRGAlgorithm, typename PermGen = HMShardedPermutationGenerator>
    auto setup_random_generation() {
#if defined(USE_SEEDED_PRG) && defined(FIXED_SEED)
        std::vector<unsigned char> seed = FIXED_SEED;
        auto localPRG = std::make_shared<CommonPRG>(seed);
#else
        // use the regular localPRG, or generate a fresh seed
        auto localPRG = std::make_shared<CommonPRG>();
#endif

        auto commonPRGManager = std::make_shared<CommonPRGManager>(1);

        // correlation generator
        std::map<std::tuple<std::type_index, Correlation>, orq::random::CorrelationGenerator*> cg;

        // Setup dummy common PRG. Use seed of all zeros.
        auto zero_seed = std::vector<unsigned char>(crypto_aead_aes256gcm_KEYBYTES);
        std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm =
            std::make_unique<orq::random::AESPRGAlgorithm>(zero_seed);
        auto commonPRG = std::make_shared<orq::random::CommonPRG>(std::move(prg_algorithm), 0);

        // This fake common PRG applies to relative party 0 (ourself) as well as the
        // group of one party (also ourself).
        commonPRGManager->add(commonPRG, 0);
        commonPRGManager->add(commonPRG, std::set<int>({0}));

        std::vector<std::set<int>> groups;
        groups.push_back({0});

        // setup the Permutation Generator
        std::shared_ptr<CommonPRGManager> commonPRGManager_ptr(commonPRGManager);
        auto sharded_generator = new PermGen(0, commonPRGManager_ptr, groups);
        cg[{__typeid(int8_t), Correlation::ShardedPermutation}] = sharded_generator;
        cg[{__typeid(int16_t), Correlation::ShardedPermutation}] = sharded_generator;
        cg[{__typeid(int32_t), Correlation::ShardedPermutation}] = sharded_generator;
        cg[{__typeid(int64_t), Correlation::ShardedPermutation}] = sharded_generator;
        cg[{__typeid(__int128_t), Correlation::ShardedPermutation}] = sharded_generator;

        // This will always return 0.
        auto zeroSharingGenerator = std::make_shared<ZeroSharingGenerator>(1, commonPRGManager);

        return std::make_unique<RandomnessManager>(localPRG, commonPRGManager, zeroSharingGenerator,
                                                   cg);
    }
}  // namespace

namespace plaintext_1pc {

    init_mpc_types(int, orq::Vector, std::vector, orq::EVector, 1);
    init_mpc_system(orq::NullCommunicator, orq::random::PRGAlgorithm, orq::Plaintext_1PC,
                    orq::Plaintext_1PC_Factory);
    init_mpc_functions(1);

    static void orq_init(int argc, char** argv) {
        // Set Batch Size / Number of Threads
        int threads_num = 1;
        int p_factor = 1;
        int batch_size = DEFAULT_BATCH_SIZE;

        if (argc >= 2) {
            threads_num = atoi(argv[1]);
        }

        if (argc >= 3) {
            p_factor = atoi(argv[2]);
        }

        if (argc >= 4) {
            batch_size = atoi(argv[3]);
        }

        runTime = std::make_unique<RunTime>(batch_size, threads_num);

        orq::benchmarking::stopwatch::partyID = 0;
        runTime->setup_workers(0);

        ProtocolFactory protocolFactory(0, 0);

        for (int i = 0; i < runTime->get_num_threads(); ++i) {
            // create a null communicator
            runTime->workers[i].attach(std::make_unique<orq::NullCommunicator>(),
                                       setup_random_generation());

            runTime->workers[i].init_proto<int8_t>(protocolFactory);
            runTime->workers[i].init_proto<int16_t>(protocolFactory);
            runTime->workers[i].init_proto<int32_t>(protocolFactory);
            runTime->workers[i].init_proto<int64_t>(protocolFactory);
            runTime->workers[i].init_proto<__int128_t>(protocolFactory);
        }
    }
}  // namespace plaintext_1pc

namespace dummy_0pc {

    init_mpc_types(int, orq::Vector, std::vector, orq::EVector, 1);
    init_mpc_system(orq::NullCommunicator, orq::random::ZeroRandomGenerator, orq::Dummy_0PC,
                    orq::Dummy_0PC_Factory);
    init_mpc_functions(1);

    static void orq_init(int argc, char** argv) {
        // Set Batch Size / Number of Threads
        int threads_num = 1;
        int batch_size = -1;

        runTime = std::make_unique<RunTime>(batch_size, threads_num);

        orq::benchmarking::stopwatch::partyID = 0;
        runTime->setup_workers(0);

        ProtocolFactory protocolFactory(0, 0);

        for (int i = 0; i < runTime->get_num_threads(); ++i) {
            // create a null communicator
            // no randomness generation for dummy
            runTime->workers[i].attach(
                std::make_unique<orq::NullCommunicator>(),
                setup_random_generation<ZeroRandomGenerator, ZeroPermutationGenerator>());

            runTime->workers[i].init_proto<int8_t>(protocolFactory);
            runTime->workers[i].init_proto<int16_t>(protocolFactory);
            runTime->workers[i].init_proto<int32_t>(protocolFactory);
            runTime->workers[i].init_proto<int64_t>(protocolFactory);
            runTime->workers[i].init_proto<__int128_t>(protocolFactory);
        }
    }
}  // namespace dummy_0pc

}  // namespace orq::service::null_service
