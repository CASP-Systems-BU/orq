#include "orq.h"

using namespace orq::debug;
using namespace orq::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#define MAX_COLUMNS 8
#define MIN_ROW_EXPONENT 5
#define MAX_ROW_EXPONENT 20

int main(int argc, char** argv) {
     [executable - threads_num - p_factor -
    // batch_size]
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 1 << 20;
    if (argc >= 5) {
        test_size = atoi(argv[4]);
    }

    orq::Vector<int64_t> v(test_size);
    for (int i = 0; i < test_size; i++) {
        v[i] = i;
    }
    BSharedVector<int64_t> b = secret_share_b(v, 0);

    stopwatch::profile_init();

    // start timer
    stopwatch::timepoint("Start");
    orq::operators::quicksort(b);
    stopwatch::timepoint("Quicksort");
    stopwatch::profile_done();

    stopwatch::profile_init();
    orq::operators::radix_sort(b);
    stopwatch::timepoint("Radix Sort");
    stopwatch::profile_done();

    // thread_stopwatch::write(pID);

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    return 0;
}
