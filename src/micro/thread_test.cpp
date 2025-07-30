/* thread_test
 *
 * This test allows for easy benchmarking of *individual* operations without
 * having to recompile. This is useful for automated data collection.
 * 
 * Specify which op on the command line, like:
 * 
 *  thread_test 1 1 8192 AND
 * 
 * Other operations can be easily added below.
 */
#include "../../include/secrecy.h"

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
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();

    // 8M elements
    const size_t test_size = 1 << 23;

    std::string op = "AND";

    if(argc >= 5){
        op = argv[4];
    }

    BSharedVector<T> a(test_size), b(test_size);

    const int bw = std::numeric_limits<std::make_unsigned_t<T>>::digits;

    single_cout("Test " << test_size << " x " << bw << " " << op << "\n");

    stopwatch::timepoint("Start");
    
    if (op == "AND") {
        auto c = a & b;
    } else if (op == "EQ") {
        auto c = a == b;
    } else if (op == "GR") {
        auto c = a > b; 
    } else if (op == "RCA") {
        auto c = a + b;
    } else if (op == "QS") {
        secrecy::operators::quicksort(a);
    } else if (op == "RS") {
        secrecy::operators::radix_sort(a);
    } else {
        std::cerr << "Unknown operator!\n";
        exit(-1);
    }

    stopwatch::timepoint("Exec " + op);

    runTime->print_statistics();


    MPI_Finalize();
    // thread_stopwatch::write(pID);
}
