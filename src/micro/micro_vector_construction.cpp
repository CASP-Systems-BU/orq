#include "../../include/secrecy.h"
#include "../../include/benchmark/stopwatch.h"

#include <span>

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// command
//mpirun -np 3 ./micro_vector_construction 1 1 8192 $VECTOR_SIZES

#define REPEAT(n, expr) for(int i = 0; i < n; i++) {expr;}

int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();

    if (pID != 0) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        MPI_Finalize();
#endif
        return 0;
    }

    int test_size = 1 << 24;
    if(argc >= 5){
        test_size = atoi(argv[4]);
    }

    std::vector<int> v1 = std::vector<int>(test_size, 0);
    std::iota(v1.begin(), v1.end(), 0);

    std::vector<int> v2 = std::vector<int>(test_size, 0);
    std::iota(v2.begin(), v2.end(), 0);

    std::span<int> s1 = std::span<int>(v2);

    stopwatch::timepoint("Start");

    secrecy::Vector<int> V1 = secrecy::Vector<int>(std::move(v1));
    stopwatch::timepoint("Move constructor");

    secrecy::Vector<int> V2 = secrecy::Vector<int>(v2);
    stopwatch::timepoint("Copy constructor");

    secrecy::Vector<int> V3 = secrecy::Vector<int>(test_size, 5);
    stopwatch::timepoint("Repeated value constructor");

    secrecy::Vector<int> V4 = secrecy::Vector<int>(s1);
    stopwatch::timepoint("Range copy constructor (using span)");

    stopwatch::done();


#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}
