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

    orq::Vector<int> v(test_size);
    for (int i = 0; i < test_size; i++) {
        v[i] = i;
    }
    BSharedVector<int> b = secret_share_b(v, 0);

    // our radix sort
    stopwatch::timepoint("Start");
    orq::operators::radix_sort(b);
    stopwatch::timepoint("Ours");

    // AHI+22 radix sort
    orq::operators::radix_sort_ccs(b, 32, true);
    stopwatch::timepoint("AHI+22");

    runTime->print_statistics();

    return 0;
}