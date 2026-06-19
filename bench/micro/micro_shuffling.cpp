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
    auto num_columns = runTime->getArg<int>("num-columns", "nc", 2);

    orq::Vector<int> v(test_size);
    for (int i = 0; i < test_size; i++) {
        v[i] = i;
    }
    BSharedVector<int> b = secret_share_b(v, 0);

    // start timer
    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    b.shuffle();

    // stop timer
    stopwatch::timepoint("Shuffle");
    stopwatch::profile_done();

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    return 0;
}