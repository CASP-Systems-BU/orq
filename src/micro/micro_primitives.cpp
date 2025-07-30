#include "../../include/secrecy.h"

#include <iomanip>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace std::chrono;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#if DEFAULT_BITWIDTH == 64
using T = int64_t;
#else
using T = int32_t;
#endif

int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 1 << 20;
    if(argc >= 5){
        test_size = atoi(argv[4]);
    }
    
    BSharedVector<T> a(test_size), b(test_size);
    ASharedVector<T> x(test_size), y(test_size);

    single_cout("Vector " << test_size << " x " << std::numeric_limits<std::make_unsigned_t<T>>::digits << "b");

    stopwatch::timepoint("Start");

    runTime->reserve_mul_triples<T>(test_size);
    stopwatch::timepoint("Reserve MUL");

    // estimate for now
    size_t and_triples = test_size * 14;

    runTime->reserve_and_triples<T>(and_triples);
    stopwatch::timepoint("Reserve AND");
    
    auto c = a & b;
    stopwatch::timepoint("AND");

    auto z = x * y;
    stopwatch::timepoint("MULT");
    
    auto d = a == b;
    stopwatch::timepoint("EQ");

    auto e = a > b;
    stopwatch::timepoint("GR");
    
    auto f = a + b;
    stopwatch::timepoint("RCA");

    auto g = rca_compare(a, b);
    stopwatch::timepoint("RCA<");

    auto h = x.dot_product(y, 8);
    stopwatch::timepoint("Dot Product");

    runTime->print_statistics();
    runTime->print_communicator_statistics();


    MPI_Finalize();

    return 0;
}

#pragma GCC diagnostic pop
