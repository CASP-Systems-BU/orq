#pragma once

#include <set>

#include "backend/common/runtime.h"
#include "mpc.h"

namespace orq::service::null_service {

namespace {
    template <typename PRG = PRGAlgorithm, typename PermGen = HMShardedPermutationGenerator>
    auto setup_random_generation() {
        auto commonPRGManager = std::make_shared<CommonPRGManager>(1);

        // Setup dummy common PRG. Use seed of all zeros.
        auto local_key = std::vector<unsigned char>(crypto_aead_aes256gcm_KEYBYTES);
        orq::random::AESPRGAlgorithm::aesKeyGen(local_key);
        std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm =
            std::make_unique<orq::random::AESPRGAlgorithm>(local_key);
        auto commonPRG = std::make_shared<orq::random::CommonPRG>(std::move(prg_algorithm), 0);

        // This local PRG applies to relative party 0 (ourself) as well as the group of one party
        // (also ourself).
        commonPRGManager->add(commonPRG, 0);
        commonPRGManager->add(commonPRG, std::set<int>({0}));

        std::vector<std::set<int>> groups;
        groups.push_back({0});

        // setup the Permutation Generator
        // For confidential mode, this is not used
        auto sharded_generator = std::make_shared<PermGen>(0, commonPRGManager, groups);

        // This will always return 0.
        auto zeroSharingGenerator = std::make_shared<ZeroSharingGenerator>(1, commonPRGManager);

        return std::make_unique<RandomnessManager>(commonPRGManager, zeroSharingGenerator,
                                                   CorrRegistry_t{}, nullptr, sharded_generator);
    }
}  // namespace

namespace plaintext_1pc {
    init_mpc_types(int, orq::Vector, std::vector, orq::EVector, 1);
    init_mpc_system(orq::NullCommunicator, orq::Plaintext_1PC, orq::Plaintext_1PC_Factory);
    init_mpc_functions(1);

    void orq_init(int argc, char** argv) {
        oc::CLP cmd(argc, argv);

        auto threads_num = register_cli<int>(cmd, "threads", "t", 1);
        auto batch_size = register_cli<int>(cmd, "batch", "b", DEFAULT_BATCH_SIZE);

        runTime = std::make_unique<RunTime>(batch_size, threads_num, cmd);

        orq::benchmarking::stopwatch::partyID = 0;
        runTime->setup_workers(0);

        ProtocolFactory protocolFactory(0, 1);

        if (cmd.isSet("help") || cmd.isSet("h")) {
            usage(argv, 0);
        }

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
    init_mpc_system(orq::NullCommunicator, orq::Dummy_0PC, orq::Dummy_0PC_Factory);
    init_mpc_functions(1);

    void orq_init(int argc, char** argv) {
        // We don't use cmd for Dummy 0PC, but ORQ programs might.
        oc::CLP cmd(argc, argv);

        // Set Batch Size / Number of Threads
        int threads_num = 1;
        int batch_size = -1;

        runTime = std::make_unique<RunTime>(batch_size, threads_num, cmd);

        orq::benchmarking::stopwatch::partyID = 0;
        runTime->setup_workers(0);

        ProtocolFactory protocolFactory(0, 1);

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
