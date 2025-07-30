#include "../../include/secrecy.h"
#include "../../include/benchmark/stopwatch.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace secrecy::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// command
//mpirun -np 3 ./micro_oprf 1 1 8192 $ROWS


int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 1 << 20;
    if (argc >= 5){
        test_size = atoi(argv[4]);
    }

    auto manager = PermutationManager::get();
    
    // start timer
    stopwatch::timepoint("Start");

    //secrecy::random::OPRF oprf(runTime->getPartyID(), 0);
    //oprf.evaluate<int>(test_size);

    stopwatch::timepoint("OPRF Evaluation");

    //manager->reserve<int32_t>(8, test_size);

    //stopwatch::timepoint("Parallel OPRF Evaluation");

    auto generator = runTime->randomnessManagers[0]->getCorrelation<int64_t, secrecy::random::Correlation::ShardedPermutation>();
    auto result = generator->getNext(test_size, secrecy::Encoding::BShared);

    stopwatch::timepoint("PermCorr");

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}