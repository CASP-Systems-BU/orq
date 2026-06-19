#include "orq.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#include <unistd.h>

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    auto test_size = runTime->getArg<size_t>("test-size", "r", 1 << 20);

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