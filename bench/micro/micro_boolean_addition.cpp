#include "orq.h"

using namespace orq::debug;
using namespace orq::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 128;
    if (argc >= 5) {
        test_size = atoi(argv[4]);
    }

    BSharedVector<int> a(test_size), b(test_size);

    // start timer
    struct timeval begin, end;
    long seconds, micro;
    double elapsed;
    gettimeofday(&begin, 0);

    BSharedVector<int> c_1 = orq::ripple_carry_adder(a, b);

    // stop timer
    gettimeofday(&end, 0);
    seconds = end.tv_sec - begin.tv_sec;
    micro = end.tv_usec - begin.tv_usec;
    elapsed = seconds + micro * 1e-6;
    if (pID == 0) {
        std::cout << "RCA_QUERY:\t\t\t" << test_size << "\t\telapsed\t\t" << elapsed << std::endl;
    }

    gettimeofday(&begin, 0);

    BSharedVector<int> c_2 = orq::parallel_prefix_adder(a, b);

    gettimeofday(&end, 0);
    seconds = end.tv_sec - begin.tv_sec;
    micro = end.tv_usec - begin.tv_usec;
    elapsed = seconds + micro * 1e-6;
    if (pID == 0) {
        std::cout << "CLA_QUERY:\t\t\t" << test_size << "\t\telapsed\t\t" << elapsed << std::endl;
    }

    return 0;
}