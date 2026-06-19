#pragma once

#include <optional>

#include "core/math/util.h"
#include "core/random/prg/committed_seeds_queue.h"
#include "rand_setup.h"
#include "setting.h"

/**
 * @brief The default batch size for normal batching, if not specified by the user.
 *
 * From 2024-Nov experiments on AWS, -12 looks to be a good default.
 */
#define DEFAULT_BATCH_SIZE -12

namespace orq::service {

void usage(char** argv, int pid) {
    if (pid == 0) {
        std::cout << "[ ORQ ]\n";
        std::cout << "Usage for " << argv[0] << ":\n";
        std::cout << "  -b / -batch        " << "\t"
                  << "batch size. negative value divide work into that many batches." << "\n";
        std::cout << "  -n / -comm-threads " << "\t" << "communication threads (nocopy only)"
                  << "\n";
        std::cout << "  -r / -test-size    " << "\t" << "input size for test scripts" << "\n";
        std::cout << "  -s / -setting      " << "\t" << "setting (lan/wan/same)" << "\n";
        std::cout << "  -t / -threads      " << "\t" << "number of worker threads" << "\n";
        std::cout << "  -x / -host-prefix  " << "\t" << "host prefix" << "\n";
        std::cout << "  -l / -latency      " << "\t" << "network latency (ms)" << "\n";
        std::cout << "  -w / -bandwidth    " << "\t" << "network bandwidth (Gbps)" << "\n";
        std::cout << "  -h / -help         " << "\t" << "this help message" << "\n";

        std::cout << "Specific executables may define their own additional arguments.\n";
        std::cout << "\n";
        std::cout << "Compilation target: " << STRINGIFY(COMPILED_MPC_PROTOCOL_NAMESPACE) << "\n";
    }
    exit(0);
}

template <typename ProtocolFactory, typename CommunicatorFactory>
void orq_runtime_init(int argc, char** argv, std::optional<int> partiesNum,
                      std::vector<std::set<int>> groups = {}) {
    oc::CLP cmd(argc, argv);

    auto file_arg = register_cli<std::string>(cmd, "file-args", "f");

    if (!file_arg.empty()) {
        std::ifstream file(file_arg);

        if (!file.is_open()) {
            throw std::runtime_error("Unable to open file: " + file_arg);
        }

        ////////////////////////////////////////////////////////////////////////
        // WARNING!
        // The following code is a bit gnarly, because we need to convert to a
        // const char **. Be very careful making changes.

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::istringstream iss(buffer.str());

        std::vector<std::string> storage;
        std::string arg;
        // CLP skips argv[0], since this is normally the executable, so we pre-populate it.
        std::vector<const char*> argv_additional = {""};

        // These loops need to be separated so we prevent reallocation issues with
        // `storage`.
        while (iss >> arg) {
            storage.push_back(arg);
        }

        for (auto& s : storage) {
            argv_additional.push_back(s.c_str());
        }

        // Make all args inside the file available.
        cmd.parse(argv_additional.size(), const_cast<char**>(argv_additional.data()));
    }

    auto threads_num = register_cli<int>(cmd, "threads", "t", 1);
    auto batch_size = register_cli<int>(cmd, "batch", "b", DEFAULT_BATCH_SIZE);
    auto setting = parse_setting(register_cli<std::string>(cmd, "setting", "s", "same"));
    auto comm_threads = register_cli<int>(cmd, "comm-threads", "n", -1);
    auto host_prefix = register_cli<std::string>(cmd, "host-prefix", "x", "node");

    auto latency = register_cli<double>(cmd, "latency", "l", 0);
    auto bandwidth =
        register_cli<double>(cmd, "bandwidth", "w", std::numeric_limits<double>::infinity());

    // Initialize the runtime
    runTime = std::make_unique<RunTime>(batch_size, threads_num, cmd);

    bool output_host_prefix_warning = false;
    if (setting == Setting::SAME) {
        if (cmd.hasValue("host-prefix") || cmd.hasValue("x")) {
            output_host_prefix_warning = true;
        }
        host_prefix = "localhost";
    } else {
        // node0, machine0, etc.
        host_prefix += "0";
    }

    // Initialize Communicator Factory
    CommunicatorFactory communicatorFactory({
        .argc = argc,
        .argv = argv,
        .numThreads = threads_num,
        .latency = latency,
        .bandwidth = bandwidth,
        .host_prefix = host_prefix,
        .setting = setting,
    });

    auto partyId = communicatorFactory.getPartyId();

    // Create the workers
    runTime->setup_workers(partyId);

    if (cmd.isSet("help") || cmd.isSet("h")) {
        usage(argv, partyId);
    }

    if (partyId == 0 && output_host_prefix_warning) {
        std::cout << "WARNING: ignoring -host-prefix argument for setting = same\n";
    }

    for (auto& w : runTime->workers) {
        // Attach their communicators
        w.attach_comm(communicatorFactory.create());
    }

    // Start all communicator threads
    communicatorFactory.start();

    // If not specified, set partiesNum from communicator
    if (!partiesNum.has_value()) {
        partiesNum = communicatorFactory.getNumParties();
    }

    // Initialize the protocol factory
    ProtocolFactory protocolFactory(partyId, partiesNum.value());

    for (auto& w : runTime->workers) {
        // Random setup depends on communicator
        w.attach_rand(setup_random_generation(communicatorFactory.getNumParties(), partyId, groups,
                                              w.getId()));

        w.init_proto<int8_t>(protocolFactory);
        w.init_proto<int16_t>(protocolFactory);
        w.init_proto<int32_t>(protocolFactory);
        w.init_proto<int64_t>(protocolFactory);
        w.init_proto<__int128_t>(protocolFactory);
    }
}

}  // namespace orq::service

namespace orq::service::mpi_service {
namespace fantastic_4pc {
    init_mpc_types(int, orq::Vector, std::vector, orq::EVector, 3);
    init_mpc_system(orq::MPICommunicator, orq::Fantastic_4PC, orq::Fantastic_4PC_Factory);
    init_mpc_functions(3);

    void orq_init(int argc, char** argv) {
        std::vector<std::set<int>> groups = ProtocolBase::generateRandomnessGroups(4, 3, 1);

        math::setup_NTL_4pc_hash_check();

        orq_runtime_init<ProtocolFactory, MPICommunicatorFactory>(argc, argv,
                                                                  Protocol_8::parties_num, groups);
    }
}  // namespace fantastic_4pc

namespace replicated_3pc {
    init_mpc_types(int, orq::Vector, std::vector, orq::EVector, 2);
    init_mpc_system(orq::MPICommunicator, orq::Replicated_3PC, orq::Replicated_3PC_Factory);
    init_mpc_functions(2);

    void orq_init(int argc, char** argv) {
        auto groups = orq::ProtocolBase::generateRandomnessGroups(3, 2, 1);
        orq_runtime_init<ProtocolFactory, MPICommunicatorFactory>(argc, argv,
                                                                  Protocol_8::parties_num, groups);
    }
}  // namespace replicated_3pc

#ifdef MPC_PROTOCOL_BEAVER_TWO
namespace beaver_2pc {
    init_mpc_types(int, orq::Vector, std::vector, orq::EVector, 1);
    init_mpc_system(orq::MPICommunicator, orq::Beaver_2PC, orq::Beaver_2PC_Factory);
    init_mpc_functions(1);

    void orq_init(int argc, char** argv) {
        auto groups = orq::ProtocolBase::generateRandomnessGroups(2, 2, 1);
        orq_runtime_init<ProtocolFactory, MPICommunicatorFactory>(argc, argv,
                                                                  Protocol_8::parties_num, groups);
    }
}  // namespace beaver_2pc
#endif

}  // namespace orq::service::mpi_service

namespace orq::service::nocopy_service {
namespace fantastic_4pc {
    init_mpc_types(int, orq::Vector, std::vector, orq::EVector, 3);
    init_mpc_system(orq::NoCopyCommunicator, orq::Fantastic_4PC, orq::Fantastic_4PC_Factory);
    init_mpc_functions(3);

    void orq_init(int argc, char** argv) {
        // This has to include the everyone-group
        std::vector<std::set<int>> groups = ProtocolBase::generateRandomnessGroups(4, 3, 1);

        math::setup_NTL_4pc_hash_check();

        orq_runtime_init<ProtocolFactory, NoCopyCommunicatorFactory>(
            argc, argv, Protocol_8::parties_num, groups);
    }
}  // namespace fantastic_4pc

namespace replicated_3pc {
    init_mpc_types(int, orq::Vector, std::vector, orq::EVector, 2);
    init_mpc_system(orq::NoCopyCommunicator, orq::Replicated_3PC, orq::Replicated_3PC_Factory);
    init_mpc_functions(2);

    void orq_init(int argc, char** argv) {
        auto groups = orq::ProtocolBase::generateRandomnessGroups(3, 2, 1);
        orq_runtime_init<ProtocolFactory, NoCopyCommunicatorFactory>(
            argc, argv, Protocol_8::parties_num, groups);
    }
}  // namespace replicated_3pc
#ifdef MPC_PROTOCOL_BEAVER_TWO
namespace beaver_2pc {
    init_mpc_types(int, orq::Vector, std::vector, orq::EVector, 1);
    init_mpc_system(orq::NoCopyCommunicator, orq::Beaver_2PC, orq::Beaver_2PC_Factory);
    init_mpc_functions(1);

    void orq_init(int argc, char** argv) {
        auto groups = orq::ProtocolBase::generateRandomnessGroups(2, 2, 1);
        orq_runtime_init<ProtocolFactory, NoCopyCommunicatorFactory>(
            argc, argv, Protocol_8::parties_num, groups);
    }
}  // namespace beaver_2pc
#endif

}  // namespace orq::service::nocopy_service
