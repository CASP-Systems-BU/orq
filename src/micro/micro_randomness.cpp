#include "../../include/secrecy.h"
#include "../../include/benchmark/stopwatch.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;


#include <unistd.h>
// command
//mpirun -np 3 ./micro_randomness 1 1 8192 $ROWS


int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 1000000;
    if(argc >= 5){
        test_size = atoi(argv[4]);
    }
    
    secrecy::Vector<int> local(test_size);
    secrecy::Vector<int> common(test_size);

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

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}