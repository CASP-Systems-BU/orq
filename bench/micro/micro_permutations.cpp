#include "orq.h"

// enforce header ordering
#include "core/random/permutations/permutation_manager.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();

    auto test_size = runTime->getArg<size_t>("test-size", "r", 1 << 20);
    auto num_permutations = runTime->getArg<size_t>("num-permutations", "p", 1);

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