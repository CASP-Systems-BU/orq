#include <iostream>
#include <type_traits>

#include "orq.h"
// explicit include for testing functionality
#include "core/random/permutations/permutation_manager.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

const size_t test_size_1 = 1 << 12;
const size_t test_size_2 = 1 << 11;
const size_t test_size_3 = 1 << 10;

template <typename Generator>
void test_pooled() {
    auto pID = runTime->getPartyID();
    auto comm = runTime->comm0();

    auto pooled = make_pooled<Generator>(pID, comm, 0);

    // reserve a batch and check that it's been reserved
    pooled->reserve(test_size_1);
    pooled->reserve(test_size_2);
    assert(pooled->size() == test_size_1 + test_size_2);

    // get the batch and check that the correlation is correct
    auto batch = pooled->getNext(test_size_3);
    assert(pooled->size() == test_size_1 + test_size_2 - test_size_3);
    auto batch_2 = pooled->getNext(test_size_1 + test_size_2 - test_size_3);
    assert(pooled->size() == 0);

    pooled->assertCorrelated(batch);
}

#ifdef MPC_PROTOCOL_BEAVER_TWO
template <typename Generator, typename T, orq::Encoding E>
void test_pooled_triples() {
    auto pID = runTime->getPartyID();
    auto comm = runTime->comm0();

    auto pooled = make_pooled<Generator>(pID, comm, 0);

    auto btgen = std::make_shared<BeaverTripleGenerator<T, E>>(pooled, comm);

    btgen->reserve(test_size_1);

    auto batch = btgen->getNext(test_size_3);
    btgen->assertCorrelated(batch);

    runTime->reserve_mul_triples<int32_t>(test_size_1);
    runTime->reserve_and_triples<int32_t>(test_size_1);
}
#endif

void test_pooled_permutations(size_t num_permutations, size_t permutation_size) {
    auto pID = runTime->getPartyID();
    auto commonPRGManager = std::shared_ptr<CommonPRGManager>(runTime->rand0()->commonPRGManager);
    auto groups = runTime->getGroups();

    // start timer
    stopwatch::get_elapsed();

    auto generator =
        runTime->rand0()->getCorrelation<int32_t, orq::random::Correlation::ShardedPermutation>();
    for (int i = 0; i < num_permutations; i++) {
        generator->getNext(permutation_size);
    }

    // stop timer
    auto unthreaded_time = stopwatch::get_elapsed();

    auto manager = PermutationManager::get();
    manager->reserve(permutation_size, num_permutations);

    // stop timer
    auto threaded_time = stopwatch::get_elapsed();

    if (pID == 0) {
        assert(threaded_time < unthreaded_time);
    }

    auto permutation = manager->getNext<int32_t>(permutation_size);
}

int main(int argc, char** argv) {
    orq_init(argc, argv);

#ifndef MPC_PROTOCOL_BEAVER_TWO
    if (orq::service::runTime->get_num_threads() > 1) {
        test_pooled_permutations(10, 100000);
        single_cout("Parallel Permutations...OK");
    } else {
        single_cout("Parallel Permutations...SKIPPED");
    }
#else
    test_pooled<GilboaOLE<int8_t>>();
    test_pooled<GilboaOLE<int32_t>>();
    test_pooled<GilboaOLE<int64_t>>();
    single_cout("Pooled Gilboa OLE...OK");

    test_pooled<SilentOT<int8_t>>();
    test_pooled<SilentOT<int32_t>>();
    test_pooled<SilentOT<int64_t>>();
    single_cout("Pooled Silent OT...OK");

    test_pooled_triples<GilboaOLE<int8_t>, int8_t, orq::Encoding::AShared>();
    test_pooled_triples<GilboaOLE<int32_t>, int32_t, orq::Encoding::AShared>();
    test_pooled_triples<GilboaOLE<int64_t>, int64_t, orq::Encoding::AShared>();

    test_pooled_triples<SilentOT<int8_t>, int8_t, orq::Encoding::BShared>();
    test_pooled_triples<SilentOT<int32_t>, int32_t, orq::Encoding::BShared>();
    test_pooled_triples<SilentOT<int64_t>, int64_t, orq::Encoding::BShared>();
    single_cout("Pooled Beaver Triples...OK");
#endif

    // Tear down communication

    return 0;
}
