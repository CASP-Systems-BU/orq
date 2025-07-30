#include "../../include/secrecy.h"
#include "../../include/benchmark/stopwatch.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace secrecy::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// command
//mpirun -np 3 ./micro_triples 1 1 8192 $ROWS

#define REPEAT 4
#define S__(x) #x
#define S_(x) S__(x)

int main(int argc, char** argv) {
    secrecy_init(argc, argv);
#ifndef MPC_PROTOCOL_BEAVER_TWO
    single_cout("Skipping test_correlated for non-2PC");
#else

    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    
    auto pID = runTime->getPartyID();
    int test_size = 1 << 16;
    if(argc >= 5){
        test_size = atoi(argv[4]);
    }

    using T = int32_t;

    BSharedVector<T> b1(test_size), b2(test_size);
    ASharedVector<T> a1(test_size), a2(test_size);

    stopwatch::timepoint("Start");

    auto y = b1 & b2;
    stopwatch::timepoint("and - no reserve");
    auto z = a1 * a1;
    stopwatch::timepoint("mult - no reserve");

    runTime->reserve_and_triples<T>(test_size);
    stopwatch::timepoint("ReserveAndTriples");

    runTime->reserve_mul_triples<T>(test_size);
    stopwatch::timepoint("ReserveMulTriples");

    y = b1 & b2;
    stopwatch::timepoint("and - with reserve");
    z = a1 * a1;
    stopwatch::timepoint("mult - with reserve");

    y = b1 & b2;
    stopwatch::timepoint("and - none left");

    runTime->reserve_and_triples<T>(test_size);
    stopwatch::timepoint("ReserveAndTriples Again");

    for (int i = 0; i < REPEAT; i++) {
        runTime->reserve_and_triples<T>(test_size);
    }
    stopwatch::timepoint("ReserveAndTriples " S_(REPEAT) "x");

    runTime->reserve_and_triples<T>(REPEAT * test_size);
    stopwatch::timepoint("Reserve " S_(REPEAT) "x AndTriples");

    for (int i = 0; i < 20; i++) {
        y = b1 & b2;
        stopwatch::timepoint("and - more reserved " + std::to_string(i));
    }

#endif

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}