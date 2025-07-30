#pragma once

#include "rand_setup.h"

namespace secrecy::service {

template <typename ProtocolFactory, typename CommunicatorFactory>
void secrecy_runtime_init(int argc, char** argv, const int& partiesNum,
                          const std::vector<std::set<int>>& groups) {
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

    // Initialize the runtime
    runTime = std::make_unique<RunTime>(batch_size, threads_num);

    // Initialize Communicator Factory
    CommunicatorFactory communicatorFactory(argc, argv, partiesNum, threads_num);
    auto partyId = communicatorFactory.getPartyId();

    // Create the workers
    runTime->setup_workers(partyId);

    for (int i = 0; i < runTime->get_num_threads(); ++i) {
        // Attach their communicators
        runTime->workers[i].attach_comm(communicatorFactory.create());
    }

    // Start all communicator threads
    communicatorFactory.start();

    // Initialize the protocol factory
    ProtocolFactory protocolFactory(partyId, partiesNum);

    for (int i = 0; i < runTime->get_num_threads(); ++i) {
        // Random setup depends on communicator
        runTime->workers[i].attach_rand(setup_random_generation(partiesNum, partyId, groups, i));

        runTime->workers[i].init_proto<int8_t>(protocolFactory);
        runTime->workers[i].init_proto<int16_t>(protocolFactory);
        runTime->workers[i].init_proto<int32_t>(protocolFactory);
        runTime->workers[i].init_proto<int64_t>(protocolFactory);
        runTime->workers[i].init_proto<__int128_t>(protocolFactory);
    }
}

}  // namespace secrecy::service

namespace secrecy::service::mpi_service {
namespace fantastic_4pc {
    init_mpc_types(int, secrecy::Vector, std::vector, secrecy::EVector, 3);
    init_mpc_system(secrecy::MPICommunicator, secrecy::random::PRGAlgorithm,
                    secrecy::Fantastic_4PC, secrecy::Fantastic_4PC_Factory);
    init_mpc_functions(3);

    void secrecy_init(int argc, char** argv) {
        std::vector<std::set<int>> groups = {{0, 1, 2}, {1, 2, 3}, {2, 3, 0}, {3, 0, 1}};
        secrecy_runtime_init<ProtocolFactory, MPICommunicatorFactory>(
            argc, argv, Protocol_8::parties_num, groups);
    }
}  // namespace fantastic_4pc

namespace replicated_3pc {
    init_mpc_types(int, secrecy::Vector, std::vector, secrecy::EVector, 2);
    init_mpc_system(secrecy::MPICommunicator, secrecy::random::PRGAlgorithm,
                    secrecy::Replicated_3PC, secrecy::Replicated_3PC_Factory);
    init_mpc_functions(2);

    void secrecy_init(int argc, char** argv) {
        std::vector<std::set<int>> groups =
            secrecy::ProtocolBase::generateRandomnessGroups(3, 2, 1);
        secrecy_runtime_init<ProtocolFactory, MPICommunicatorFactory>(
            argc, argv, Protocol_8::parties_num, groups);
    }
}  // namespace replicated_3pc

#ifdef MPC_PROTOCOL_BEAVER_TWO
namespace beaver_2pc {
    init_mpc_types(int, secrecy::Vector, std::vector, secrecy::EVector, 1);
    init_mpc_system(secrecy::MPICommunicator, secrecy::random::PRGAlgorithm,
                    secrecy::Beaver_2PC, secrecy::Beaver_2PC_Factory);
    init_mpc_functions(1);

    void secrecy_init(int argc, char** argv) {
        std::vector<std::set<int>> groups =
            secrecy::ProtocolBase::generateRandomnessGroups(2, 2, 1);
        secrecy_runtime_init<ProtocolFactory, MPICommunicatorFactory>(
            argc, argv, Protocol_8::parties_num, groups);
    }
}  // namespace beaver_2pc
#endif
}  // namespace secrecy::service::mpi_service

namespace secrecy::service::nocopy_service {
namespace fantastic_4pc {
    init_mpc_types(int, secrecy::Vector, std::vector, secrecy::EVector, 3);
    init_mpc_system(secrecy::NoCopyCommunicator, secrecy::random::PRGAlgorithm,
                    secrecy::Fantastic_4PC, secrecy::Fantastic_4PC_Factory);
    init_mpc_functions(3);

    void secrecy_init(int argc, char** argv) {
        std::vector<std::set<int>> groups = {{0, 1, 2}, {1, 2, 3}, {2, 3, 0}, {3, 0, 1}};
        secrecy_runtime_init<ProtocolFactory, NoCopyCommunicatorFactory>(
            argc, argv, Protocol_8::parties_num, groups);
    }
}  // namespace fantastic_4pc

namespace replicated_3pc {
    init_mpc_types(int, secrecy::Vector, std::vector, secrecy::EVector, 2);
    init_mpc_system(secrecy::NoCopyCommunicator, secrecy::random::PRGAlgorithm,
                    secrecy::Replicated_3PC, secrecy::Replicated_3PC_Factory);
    init_mpc_functions(2);

    void secrecy_init(int argc, char** argv) {
        std::vector<std::set<int>> groups =
            secrecy::ProtocolBase::generateRandomnessGroups(3, 2, 1);
        secrecy_runtime_init<ProtocolFactory, NoCopyCommunicatorFactory>(
            argc, argv, Protocol_8::parties_num, groups);
    }
}  // namespace replicated_3pc
#ifdef MPC_PROTOCOL_BEAVER_TWO
namespace beaver_2pc {
    init_mpc_types(int, secrecy::Vector, std::vector, secrecy::EVector, 1);
    init_mpc_system(secrecy::NoCopyCommunicator, secrecy::random::PRGAlgorithm,
                    secrecy::Beaver_2PC, secrecy::Beaver_2PC_Factory);
    init_mpc_functions(1);

    void secrecy_init(int argc, char** argv) {
        std::vector<std::set<int>> groups =
            secrecy::ProtocolBase::generateRandomnessGroups(2, 2, 1);
        secrecy_runtime_init<ProtocolFactory, NoCopyCommunicatorFactory>(
            argc, argv, Protocol_8::parties_num, groups);
    }
}  // namespace beaver_2pc
#endif
}  // namespace secrecy::service::nocopy_service
