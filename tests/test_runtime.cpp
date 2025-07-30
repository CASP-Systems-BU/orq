#include "../include/secrecy.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace secrecy::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

void test_setup(int max_threads) {
    int batch_size = 8192;

    // create runtime objects with variable number of threads and make sure they successfully create
    // if this test hangs, there is a deadlock in setup
    for (int num_threads = 1; num_threads < max_threads; num_threads <<= 1) {
        RunTime rt(batch_size, num_threads);
        rt.setup_workers(0);
    }
}

void test_modify_parallel(int test_size) {
    secrecy::Vector<int> v(test_size);
    std::iota(v.begin(), v.end(), 0);

    BSharedVector<int> b = secret_share_b(v, 0);

    // masking calls runTime->modify_parallel
    b.mask(1);

    auto opened = b.open();

    v.mask(1);
    
    assert(opened.same_as(v));
}

void test_parallel_generation(size_t test_size) {
    int pID = secrecy::service::runTime->getPartyID();

    /*
        common randomness
    */
    secrecy::Vector<int> common(test_size);
    stopwatch::get_elapsed();
    std::set<int> group = secrecy::service::runTime->getPartySet();

    secrecy::service::runTime->rand0()->commonPRGManager->get(group)->getNext(common);
    auto unthreaded_time = stopwatch::get_elapsed();    

    secrecy::service::runTime->populateCommonRandom(common, group);
    auto threaded_time = stopwatch::get_elapsed();

    if (pID == 0) {
        assert(threaded_time < unthreaded_time);
    }
}

int main(int argc, char **argv) {
    secrecy_init(argc, argv);

    test_setup(32);
    single_cout("Runtime Setup...OK");

    test_modify_parallel(1000000);
    single_cout("Modify Parallel...OK");
    
    auto T = secrecy::service::runTime->get_num_threads();
    if (T > 1) {
        test_parallel_generation(T * (1 << 24));
        single_cout("Parallel Generation...OK");
    } else {
        single_cout("Parallel Generation...SKIPPED");
    }

    // Create test vector
    secrecy::Vector<int> vec_1 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}