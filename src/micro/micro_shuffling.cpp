#include "../../include/secrecy.h"
#include "../../include/benchmark/stopwatch.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;


#include <unistd.h>
// command
//mpirun -np 3 ./micro_shuffling 1 1 8192 $ROWS


int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 128;
    int num_columns = 2;
    if (argc >= 5){
        test_size = atoi(argv[4]);
    }
    if (argc >= 6) {
        num_columns = atoi(argv[5]);
    }

    secrecy::Vector<int> v(test_size);
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

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}