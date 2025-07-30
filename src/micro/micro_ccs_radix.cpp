#include "../../include/secrecy.h"

using namespace secrecy::debug;
using namespace secrecy::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#define MAX_COLUMNS 8
#define MIN_ROW_EXPONENT 5
#define MAX_ROW_EXPONENT 20

int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor -
    // batch_size]
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 1 << 20;
    if (argc >= 5) {
        test_size = atoi(argv[4]);
    }

    secrecy::Vector<int> v(test_size);
    for (int i = 0; i < test_size; i++) {
        v[i] = i;
    }
    BSharedVector<int> b = secret_share_b(v, 0);

    // our radix sort
    stopwatch::timepoint("Start");
    secrecy::operators::radix_sort(b);
    stopwatch::timepoint("Ours");

    // AHI+22 radix sort
    secrecy::operators::radix_sort_ccs(b, 32, true);
    stopwatch::timepoint("AHI+22");

    runTime->print_statistics();


#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}