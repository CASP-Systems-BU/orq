#include "../../include/secrecy.h"
#include "../../include/benchmark/stopwatch.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace secrecy::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// command
//mpirun -np 3 ./micro_randomness 1 1 8192 $ROWS


int main(int argc, char** argv) {
    secrecy_init(argc, argv);
#ifndef MPC_PROTOCOL_BEAVER_TWO
    single_cout("Skipping test_correlated for non-2PC");
#else

    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    
    auto pID = runTime->getPartyID();
    int test_size = 1000000;
    if(argc >= 5){
        test_size = atoi(argv[4]);
    }

    // setup generators
    using OLEBase = secrecy::random::OLEGenerator<int32_t, secrecy::Encoding::AShared>;
    using ole_t = OLEBase::ole_t;
    
    auto comm = runTime->comm0();

    auto generator = make_pooled<SilentOLE<int32_t>>(pID, comm, 0);
    auto btgen = std::make_shared<BeaverTripleGenerator<int32_t, secrecy::Encoding::AShared>>(generator, comm);

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

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}