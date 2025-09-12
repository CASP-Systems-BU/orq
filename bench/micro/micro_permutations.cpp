#include "orq.h"

// enforce header ordering
#include "core/random/permutations/permutation_manager.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// command
// mpirun -np 3 ./micro_permutations 1 1 8192 $ROWS

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 1000000;
    int num_permutations = 1;
    if (argc >= 5) {
        test_size = atoi(argv[4]);
    }
    if (argc >= 6) {
        num_permutations = atoi(argv[5]);
    }

    auto manager = PermutationManager::get();

    // start timer
    stopwatch::timepoint("Start");

    manager->getNext<int64_t>(test_size);

    // stop timer
    stopwatch::timepoint("Single Thread - 1 Permutation");

    for (int i = 0; i < num_permutations; i++) {
        manager->getNext<int64_t>(test_size);
    }

    // stop timer
    stopwatch::timepoint("Single Thread - N Permutation");
    manager->reserve(test_size, num_permutations);

    // stop timer
    stopwatch::timepoint("Multi Thread - N Permutations");

    return 0;
}