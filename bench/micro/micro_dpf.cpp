#include "orq.h"
#include "profiling/memory.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

/*
 * Run microbenchmark for DPF.
 *
 * We specify the number of DPF points and their domain.
 * We run distributed key generation and (local)expansion.
 */

int main(int argc, char** argv) {
#ifdef USE_LIBOTE
    // Initialize orq runtime [executable - threads_num - p_factor - batch_size]
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 1 << 20;
    int domain = 1 << 5;
    if (argc >= 5) {
        test_size = atoi(argv[4]);
    }
    if (argc >= 6) {
        domain = 1 << atoi(argv[5]);
    }

    single_cout("Inputs: " << test_size << ", domain: " << domain);

    orq::random::DPF<int64_t> dpf(pID, 0, runTime->comm0());
    orq::Vector<int64_t> input(test_size);

    // start timer
    stopwatch::timepoint("Start");
    memory::mempoint("Start");

    dpf.keyGen(input, domain);

    stopwatch::timepoint("Distributed Key Generation");

    auto output = dpf.expand();

    stopwatch::timepoint("Expansion");
    memory::mempoint("Peak Memory");

    return 0;
#endif
}
