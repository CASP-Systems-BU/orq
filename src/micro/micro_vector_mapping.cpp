#include "../../include/secrecy.h"
#include "../../include/benchmark/stopwatch.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// command
//mpirun -np 3 ./micro_vector_mapping 1 1 8192 $VECTOR_SIZES

#define REPEAT(n, expr) for(int i = 0; i < n; i++) {expr;}

#define NUM_REPETITIONS 1000

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

    int test_size = 1 << 20;
    if(argc >= 5){
        test_size = atoi(argv[4]);
    }

    secrecy::Vector<int> v(test_size);
    runTime->populateLocalRandom(v);
    secrecy::Vector<int> f(test_size);
    runTime->populateLocalRandom(f);
    f = (f % 3) == 0;

    single_cout(NUM_REPETITIONS << " repetitions each");

    stopwatch::timepoint("Start");

    REPEAT(NUM_REPETITIONS, v.simple_subset_reference(0, 1, test_size));
    stopwatch::timepoint("Simple Subset Reference - Full");

    REPEAT(NUM_REPETITIONS, v.simple_subset_reference(0, 1, test_size / 2));
    stopwatch::timepoint("SSR - Half");

    REPEAT(NUM_REPETITIONS, v.simple_subset_reference(0, 1, test_size / 2).simple_subset_reference(test_size / 5, 1, test_size / 3));
    stopwatch::timepoint("SSR - Composed");

    REPEAT(NUM_REPETITIONS, v.simple_subset_reference(0, 1, 99));
    stopwatch::timepoint("Simple Subset Reference - Small");

    REPEAT(NUM_REPETITIONS, v.slice(0, test_size));
    stopwatch::timepoint("Slice - Full");

    REPEAT(NUM_REPETITIONS, v.slice(0, test_size / 2));
    stopwatch::timepoint("Slice - Half");

    REPEAT(NUM_REPETITIONS, v.slice(0, test_size / 2).slice(test_size / 5, test_size / 3));
    stopwatch::timepoint("Slice - Composed");

    REPEAT(NUM_REPETITIONS, v.slice(0, 100));
    stopwatch::timepoint("Slice - Small");

    REPEAT(NUM_REPETITIONS, v.alternating_subset_reference(1, 0));
    stopwatch::timepoint("Alternating Subset Reference");

    REPEAT(NUM_REPETITIONS, v.reversed_alternating_subset_reference(1, 0));
    stopwatch::timepoint("Reversed Alternating Subset Reference");

    REPEAT(NUM_REPETITIONS, v.repeated_subset_reference(1));
    stopwatch::timepoint("Repeating Subset Reference");

    REPEAT(NUM_REPETITIONS, v.cyclic_subset_reference(1));
    stopwatch::timepoint("Cyclic Subset Reference");

    REPEAT(NUM_REPETITIONS, v.directed_subset_reference(-1));
    stopwatch::timepoint("Directed Subset Reference");

    REPEAT(NUM_REPETITIONS, v.included_reference(f));
    stopwatch::timepoint("Included Reference - Many");

    f.zero();

    REPEAT(NUM_REPETITIONS, v.included_reference(f));
    stopwatch::timepoint("Included Reference - None");
    

    stopwatch::done();


#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}
