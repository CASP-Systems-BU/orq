#include "orq.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// command
// mpirun -np 3 ./micro_randomness 1 1 8192 $ROWS

int main(int argc, char** argv) {
    orq_init(argc, argv);
#ifndef MPC_PROTOCOL_BEAVER_TWO
    single_cout("Skipping test_correlated for non-2PC");
#else

    auto pID = runTime->getPartyID();
    int test_size = 1000000;
    if (argc >= 5) {
        test_size = atoi(argv[4]);
    }

    // setup generators
    using OLEBase = orq::random::OLEGenerator<int32_t, orq::Encoding::AShared>;
    using ole_t = OLEBase::ole_t;

    auto comm = runTime->comm0();

    auto generator = make_pooled<GilboaOLE<int32_t> >(pID, comm, 0);
    auto btgen =
        std::make_shared<BeaverTripleGenerator<int32_t, orq::Encoding::AShared> >(generator, comm);

    stopwatch::timepoint("Start");

    /*
        generate without pooling
    */
    btgen->getNext(test_size);
    stopwatch::timepoint("Without Pooling");

    /*
        generate with pooling
    */

    // generation phase
    runTime->reserve_mul_triples<int32_t>(test_size);
    stopwatch::timepoint("Pooling Generation Phase");

    auto batch = btgen->getNext(test_size);
    stopwatch::timepoint("Pooling Retrieval Phase");

#endif

    return 0;
}