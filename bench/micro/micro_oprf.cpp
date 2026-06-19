#include "orq.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
#ifdef USE_LIBOTE
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    auto test_size = runTime->getArg<size_t>("test-size", "r", 1 << 20);
    auto num_threads = runTime->get_num_threads();

    // Create input vector for OPRF evaluation
    orq::Vector<__int128_t> input(test_size * num_threads);
    for (int i = 0; i < test_size; i++) {
        input[i] = i;
    }
    orq::Vector<__int128_t> output(test_size * num_threads);

    // start timer
    stopwatch::timepoint("Start");

    // Parallel OPRF evaluation using the new runtime function
    bool is_sender = (pID == 0);
    runTime->evaluate_oprf(input, output, is_sender);

    stopwatch::timepoint("OPRF (Role 0)");

    runTime->evaluate_oprf(input, output, !is_sender);

    stopwatch::timepoint("OPRF (Role 1)");

    return 0;
#endif
}
