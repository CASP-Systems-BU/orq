/**
 * @file test_securejoin.cpp
 * @brief Test that SecureJoin is properly installed and basic functionality works.
 *
 */

#include "orq.h"

// This test only runs on 2pc
#ifdef MPC_PROTOCOL_BEAVER_TWO

#include "coproto/Socket/AsioSocket.h"
#include "secure-join/Prf/AltModPrfProto.h"

using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// tests that the OPRF correctly evaluates the PRF
// evaluate both OPRF and plaintext PRF and check for equality
void test_oprf_correctness(int test_size) {
    int rank = runTime->getPartyID();

    // create an OPRF object
    // TODO: this should be created at setup and accessible through the runtime
    orq::random::OPRF oprf(rank, 0);

    // generate a key to use for both plaintext and secret-shared evaluations
    OPRF::key_t key = oprf.keyGen();

    // generate an input vector
    Vector<__int128_t> inputs(test_size);
    for (int i = 0; i < test_size; i++) {
        inputs[i] = i;
    }

    Vector<__int128_t> outputs_plaintext =
        oprf.evaluate_plaintext<__int128_t>(inputs.as_std_vector(), key);
    Vector<__int128_t> outputs_plaintext_backup =
        oprf.evaluate_plaintext<__int128_t>(inputs.as_std_vector(), key);

    // check that plaintext evaluation is deterministic
    // consecutive runs with identical input should produce identical output
    for (int i = 0; i < outputs_plaintext.size(); i++) {
        assert(outputs_plaintext[i] == outputs_plaintext_backup[i]);
    }

    BSharedVector<__int128_t> result_0(inputs.size());
    BSharedVector<__int128_t> result_1(inputs.size());

    if (rank == 0) {
        result_0.vector(0) = oprf.evaluate_sender<__int128_t>(key, inputs.size());
        result_1.vector(0) = oprf.evaluate_receiver<__int128_t>(inputs);
    } else {
        result_0.vector(0) = oprf.evaluate_receiver<__int128_t>(inputs);
        result_1.vector(0) = oprf.evaluate_sender<__int128_t>(key, inputs.size());
    }

    Vector<__int128_t> opened_0 = result_0.open();
    Vector<__int128_t> opened_1 = result_1.open();

    // check that the opened secret shared output equals the plaintext output
    // check values computed with P0's key
    if (rank == 0) {
        for (int i = 0; i < opened_0.size(); i++) {
            assert(opened_0[i] == outputs_plaintext[i]);
        }
    }
    // check values computed with P1's key
    if (rank == 1) {
        for (int i = 0; i < opened_1.size(); i++) {
            assert(opened_1[i] == outputs_plaintext[i]);
        }
    }
}

void test_permutation_correlations() {
    auto pID = runTime->getPartyID();
    auto commonPRGManager = runTime->rand0()->commonPRGManager;
    auto comm = runTime->comm0();
    auto sharded_generator =
        new DMPermutationCorrelationGenerator<__int128_t>(pID, 0, commonPRGManager, comm);

    auto perm = sharded_generator->getNext(1000);
    std::shared_ptr<DMShardedPermutation<__int128_t>> dm_perm =
        std::dynamic_pointer_cast<DMShardedPermutation<__int128_t>>(perm);

    sharded_generator->assertCorrelated(dm_perm);
}

int main(int argc, char** argv) {
    orq_init(argc, argv);

    auto pID = runTime->getPartyID();

    test_oprf_correctness(1000);
    single_cout("OPRF Correctness...OK");

    test_permutation_correlations();
    single_cout("Permutation Correlations...OK");
}
#else  // MPC_PROTOCOL_BEAVER_TWO

// Dummy test for non-2pc

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    single_cout("secureJoin tests only for 2PC");
}

#endif  // MPC_PROTOCOL_BEAVER_TWO
