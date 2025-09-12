#include "orq.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

void test_setup(int max_threads) {
    int batch_size = 8192;

    // create runtime objects with variable number of threads and make sure they successfully create
    // if this test hangs, there is a deadlock in setup
    for (int num_threads = 1; num_threads < max_threads; num_threads <<= 1) {
        RunTime rt(batch_size, num_threads, true);
        rt.setup_workers(0);
    }
}

void test_modify_parallel(int test_size) {
    orq::Vector<int> v(test_size);
    std::iota(v.begin(), v.end(), 0);

    BSharedVector<int> b = secret_share_b(v, 0);

    // masking calls runTime->modify_parallel
    b.mask(1);

    auto opened = b.open();

    v.mask(1);

    assert(opened.same_as(v));
}

void test_parallel_generation(size_t test_size) {
    int pID = orq::service::runTime->getPartyID();

    /*
        common randomness
    */
    orq::Vector<int> common(test_size);
    stopwatch::get_elapsed();
    std::set<int> group = orq::service::runTime->getPartySet();

    orq::service::runTime->rand0()->commonPRGManager->get(group)->getNext(common);
    auto unthreaded_time = stopwatch::get_elapsed();

    orq::service::runTime->populateCommonRandom(common, group);
    auto threaded_time = stopwatch::get_elapsed();

    if (pID == 0) {
        assert(threaded_time < unthreaded_time);
    }
}

int main(int argc, char **argv) {
    orq_init(argc, argv);

    test_setup(32);
    single_cout("Runtime Setup...OK");

    test_modify_parallel(1000000);
    single_cout("Modify Parallel...OK");

    auto T = orq::service::runTime->get_num_threads();
    if (T > 1) {
        test_parallel_generation(T * (1 << 24));
        single_cout("Parallel Generation...OK");
    } else {
        single_cout("Parallel Generation...SKIPPED");
    }

    // Create test vector
    orq::Vector<int> vec_1 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

    return 0;
}