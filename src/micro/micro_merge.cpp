#include "../../include/secrecy.h"
#include "../../include/benchmark/stopwatch.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;


#include <unistd.h>


int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 128;
    if(argc >= 5){
        test_size = atoi(argv[4]);
    }

    secrecy::Vector<int> v(test_size);
    for (int i = 0; i < test_size; i++) {
        v[i] = i;
    }
    // secret share and shuffle the vector
    BSharedVector<int> b = secret_share_b(v, 0);
    b.shuffle();

    // sort each half of the list
    secrecy::Vector<int> shuffled = b.open();
    std::vector<int> first_half(test_size / 2);
    std::vector<int> second_half(test_size / 2);
    for (int i = 0; i < test_size / 2; i++) {
        first_half[i] = shuffled[i];
        second_half[i] = shuffled[test_size/2 + i];
    }
    std::sort(first_half.begin(), first_half.end());
    std::sort(second_half.begin(), second_half.end());
    // recombine and share
    secrecy::Vector<int> vec_to_merge(test_size);
    for (int i = 0; i < test_size / 2; i++) {
        vec_to_merge[i] = first_half[i];
        vec_to_merge[test_size/2 + i] = second_half[i];
    }
    BSharedVector<int> b2 = secret_share_b(vec_to_merge, 0);

    // start timer
    stopwatch::timepoint("Start");

    secrecy::operators::odd_even_merge(b2);

    // stop timer
    stopwatch::timepoint("Merge");


#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}