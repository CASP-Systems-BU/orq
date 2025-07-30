// Include secrecy first
#include "../include/secrecy.h"
// Then include other files if not already included
#include "../include/core/random/ole_generator.h"


// ../scripts/run_experiment.sh -p 3 -s same -c mpi -r 1 -T 1 test_correlated

using namespace secrecy;
using namespace secrecy::service;
using namespace secrecy::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

const size_t test_size = 1 << 12;

template <typename T, secrecy::random::Correlation C>
void checkCorrelation(std::string label) {
    auto L = std::numeric_limits<std::make_unsigned_t<T>>::digits;
    single_cout_nonl("Checking length " << test_size << " " << L << "-bit " << label << "... ");

    auto gen = runTime->rand0()->getCorrelation<T, C>();
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
        runTime->rand0()->getCorrelation<T, secrecy::random::Correlation::ShardedPermutation>();
    auto generator = dynamic_cast<DMShardedPermutationGenerator<T>*>(base_generator);

    // check for nullptr
    if (generator == nullptr) {
        throw std::runtime_error("Failed to get generator of type DMShardedPermutationGenerator");
    }

    secrecy::random::PermutationManager::get()->reserve(test_size, 2);

    // check individual permutation correlations
    auto result_a = secrecy::random::PermutationManager::get()->getNext<T>(
        test_size, secrecy::Encoding::AShared);
    auto result_b = secrecy::random::PermutationManager::get()->getNext<T>(
        test_size, secrecy::Encoding::BShared);

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
    auto [first, second] =
        secrecy::random::PermutationManager::get()->getNextPair<T, T>(test_size);
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

template <typename T>
void TestDummyAuthTriplesGenerator(const size_t& testSize) {
    // Party Information
    auto pID = secrecy::service::runTime->getPartyID();
    auto pNum = secrecy::service::runTime->getNumParties();
    auto othersCount = pNum - 1;

    // Helpers
    auto communicator = secrecy::service::runTime->comm0();
    auto randManager = secrecy::service::runTime->rand0();
    auto zeroSharingGenerator = randManager->zeroSharingGenerator;
    auto localPRG = randManager->localPRG;

    // Generating the key
    T partyKey;
    localPRG->getNext(partyKey);

    // Creating the dummy generator
    auto dummyGenerator = secrecy::random::DummyAuthTripleGenerator<T>(
        pNum, partyKey, pID, localPRG, zeroSharingGenerator, communicator);

    // Getting some dummy triples
    auto triple = dummyGenerator.getNext(testSize);
    dummyGenerator.assertCorrelated(triple);
}

template <typename T>
void TestDummyAuthRandomGenerator(const size_t& testSize) {
    // Party Information
    auto pID = secrecy::service::runTime->getPartyID();
    auto pNum = secrecy::service::runTime->getNumParties();
    auto othersCount = pNum - 1;

    // Helpers
    auto communicator = secrecy::service::runTime->comm0();
    auto randManager = secrecy::service::runTime->rand0();
    auto zeroSharingGenerator = randManager->zeroSharingGenerator;
    auto localPRG = randManager->localPRG;

    // Generating the key
    T partyKey;
    localPRG->getNext(partyKey);

    // Creating the dummy generator
    auto dummyGenerator = secrecy::random::DummyAuthRandomGenerator<T>(
        pNum, partyKey, pID, localPRG, zeroSharingGenerator, communicator);

    // Getting some dummy random numbers
    auto a = dummyGenerator.getNext(testSize);
    dummyGenerator.assertCorrelated(a);
}

int main(int argc, char** argv) {
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

#ifndef MPC_PROTOCOL_BEAVER_TWO
    single_cout("Skipping test_correlated for non-2PC");
#else
    checkCorrelation<int8_t, Correlation::OLE>("OLE");
    checkCorrelation<int32_t, Correlation::OLE>("OLE");
    checkCorrelation<int64_t, Correlation::OLE>("OLE");

    checkCorrelation<int8_t, Correlation::rOT>("rOT");
    checkCorrelation<int32_t, Correlation::rOT>("rOT");
    checkCorrelation<int64_t, Correlation::rOT>("rOT");

    checkCorrelation<int8_t, Correlation::BeaverMulTriple>("Beaver Triples");
    checkCorrelation<int32_t, Correlation::BeaverMulTriple>("Beaver Triples");
    checkCorrelation<int64_t, Correlation::BeaverMulTriple>("Beaver Triples");

    checkCorrelation<int8_t, Correlation::BeaverAndTriple>("Beaver AND Triples");
    checkCorrelation<int32_t, Correlation::BeaverAndTriple>("Beaver AND Triples");
    checkCorrelation<int64_t, Correlation::BeaverAndTriple>("Beaver AND Triples");

    // we only generate 128-bit permutation correlations and cut them down
    // so we only have a 128-bit generator object to run assertCorrelated
    test_permutation_correlations<__int128_t>(1000);
    single_cout("Permutation Correlations... OK");
#endif

    ///////////////////////////////////////////
    // Testing the DummyAuthTriplesGenerator //
    ///////////////////////////////////////////
    single_cout("Testing DummyAuthTriplesGenerator...");
    TestDummyAuthTriplesGenerator<int8_t>(test_size);
    single_cout("int8_t dummy authenticated triples generation: OK");

    TestDummyAuthTriplesGenerator<int16_t>(test_size);
    single_cout("int16_t dummy authenticated triples generation: OK");

    TestDummyAuthTriplesGenerator<int32_t>(test_size);
    single_cout("int32_t dummy authenticated triples generation: OK");

    TestDummyAuthTriplesGenerator<int64_t>(test_size);
    single_cout("int64_t dummy authenticated triples generation: OK");

    TestDummyAuthTriplesGenerator<__int128_t>(test_size);
    single_cout("__int128_t dummy authenticated triples generation: OK");

    single_cout("");

    ///////////////////////////////////////////
    // Testing the DummyAuthRandomGenerator ///
    ///////////////////////////////////////////
    single_cout("Testing DummyAuthRandomGenerator...");
    TestDummyAuthRandomGenerator<int8_t>(test_size);
    single_cout("int8_t dummy authenticated random generation: OK");

    TestDummyAuthRandomGenerator<int16_t>(test_size);
    single_cout("int16_t dummy authenticated random generation: OK");

    TestDummyAuthRandomGenerator<int32_t>(test_size);
    single_cout("int32_t dummy authenticated random generation: OK");

    TestDummyAuthRandomGenerator<int64_t>(test_size);
    single_cout("int64_t dummy authenticated random generation: OK");

    TestDummyAuthRandomGenerator<__int128_t>(test_size);
    single_cout("__int128_t dummy authenticated random generation: OK");

    runTime->malicious_check();

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
}
