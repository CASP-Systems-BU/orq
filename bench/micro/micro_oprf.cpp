#include "orq.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// command
// mpirun -np 3 ./micro_oprf 1 1 8192 $ROWS

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 1 << 20;
    if (argc >= 5) {
        test_size = atoi(argv[4]);
    }

    auto manager = PermutationManager::get();

    // start timer
    stopwatch::timepoint("Start");

    // orq::random::OPRF oprf(runTime->getPartyID(), 0);
    // oprf.evaluate<int>(test_size);

    stopwatch::timepoint("OPRF Evaluation");

    // manager->reserve<int32_t>(8, test_size);

    // stopwatch::timepoint("Parallel OPRF Evaluation");

    auto generator =
        runTime->rand0()->getCorrelation<int64_t, orq::random::Correlation::ShardedPermutation>();
    auto result = generator->getNext(test_size);

    stopwatch::timepoint("PermCorr");

    return 0;
}