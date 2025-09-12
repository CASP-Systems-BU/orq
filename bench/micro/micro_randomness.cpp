#include "orq.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#include <unistd.h>
// command
// mpirun -np 3 ./micro_randomness 1 1 8192 $ROWS

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 1000000;
    if (argc >= 5) {
        test_size = atoi(argv[4]);
    }

    orq::Vector<int> local(test_size);
    orq::Vector<int> common(test_size);

    /*
        local randomness
    */

    // start timer
    stopwatch::timepoint("Start");

    runTime->populateLocalRandom(local);

    // stop timer
    stopwatch::timepoint("Local Randomness");

    /*
        common randomness
    */

    std::set<int> group = runTime->getGroups()[0];
    if (group.contains(pID)) {
        runTime->populateCommonRandom(common, group);
    }

    // stop timer
    stopwatch::timepoint("Common Randomness");

    return 0;
}