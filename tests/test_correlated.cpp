#include "orq.h"

// Include other files if not already included
#include "core/math/primes.h"

// ../scripts/run_experiment.py -p 3 -s same -c mpi -r 1 -T 1 test_correlated

using namespace orq;
using namespace orq::service;
using namespace orq::random;
using namespace orq::math::primes;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

const size_t test_size = 1 << 12;

template <typename T, template <typename> class C>
void checkCorrelation(std::string label) {
    auto L = std::numeric_limits<std::make_unsigned_t<T>>::digits;
    single_cout_nonl("Checking length " << test_size << " " << L << "-bit " << label << "... ");

    auto gen = runTime->rand0()->template getCorrelation<T, C<T>>();
    auto corr = gen->getNext(test_size);

    gen->assertCorrelated(corr);

    single_cout("OK");
}

template <typename T>
void test_permutation_correlations(int test_size) {
#ifdef MPC_PROTOCOL_BEAVER_TWO
    // get the generator and interpret it as a dishonest-majority generator
    // otherwise it won't have the assertCorrelated function
    auto base_generator =
        runTime->rand0()->template getCorrelation<T, ShardedPermutationGenerator>();
    auto generator = dynamic_pointer_cast<DMShardedPermutationGenerator<T>>(base_generator);

    // check for nullptr
    if (generator == nullptr) {
        throw std::runtime_error("Failed to get generator of type DMShardedPermutationGenerator");
    }

    orq::random::PermutationManager::get()->reserve(test_size, 2);

    // check individual permutation correlations
    auto result_a =
        orq::random::PermutationManager::get()->getNext<T>(test_size, orq::Encoding::AShared);
    auto result_b =
        orq::random::PermutationManager::get()->getNext<T>(test_size, orq::Encoding::BShared);

    std::shared_ptr<DMShardedPermutation<T>> perm_corr_a =
        std::dynamic_pointer_cast<DMShardedPermutation<T>>(result_a);
    std::shared_ptr<DMShardedPermutation<T>> perm_corr_b =
        std::dynamic_pointer_cast<DMShardedPermutation<T>>(result_b);
    // check for nullptr
    if ((perm_corr_a == nullptr) || (perm_corr_b == nullptr)) {
        throw std::runtime_error("Failed to get permutation");
    }

    generator->assertCorrelated(perm_corr_a);
    generator->assertCorrelated(perm_corr_b);

    // check pairs of permutation correlations
    auto [first, second] = orq::random::PermutationManager::get()->getNextPair<T, T>(test_size);
    auto pair_first = std::dynamic_pointer_cast<DMShardedPermutation<T>>(first);
    auto pair_second = std::dynamic_pointer_cast<DMShardedPermutation<T>>(second);
    if ((pair_first == nullptr) || (pair_second == nullptr)) {
        throw std::runtime_error("Failed to get permutation in pair");
    }
    // make sure each permutation individually is correct
    generator->assertCorrelated(pair_first);
    generator->assertCorrelated(pair_second);
#endif
}

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pid = runTime->getPartyID();

#ifndef MPC_PROTOCOL_BEAVER_TWO
    single_cout("Skipping test_correlated for non-2PC");
#else
    checkCorrelation<int8_t, OLEGenerator>("OLE");
    checkCorrelation<int32_t, OLEGenerator>("OLE");
    checkCorrelation<int64_t, OLEGenerator>("OLE");
    checkCorrelation<__int128_t, OLEGenerator>("OLE");

    checkCorrelation<int8_t, OTGenerator>("rOT");
    checkCorrelation<int32_t, OTGenerator>("rOT");
    checkCorrelation<int64_t, OTGenerator>("rOT");
    checkCorrelation<__int128_t, OTGenerator>("rOT");

    checkCorrelation<int8_t, BeaverAndGenerator>("Beaver AND Triples");
    checkCorrelation<int32_t, BeaverAndGenerator>("Beaver AND Triples");
    checkCorrelation<int64_t, BeaverAndGenerator>("Beaver AND Triples");
    checkCorrelation<__int128_t, BeaverAndGenerator>("Beaver AND Triples");

    checkCorrelation<int8_t, BeaverMulGenerator>("Beaver Triples");
    checkCorrelation<int32_t, BeaverMulGenerator>("Beaver Triples");
    checkCorrelation<int64_t, BeaverMulGenerator>("Beaver Triples");
    checkCorrelation<__int128_t, BeaverMulGenerator>("Beaver Triples");

    // we only generate 128-bit permutation correlations and cut them down
    // so we only have a 128-bit generator object to run assertCorrelated
    test_permutation_correlations<__int128_t>(1000);
    single_cout("Permutation Correlations... OK");
#endif

    runTime->malicious_check();
}
