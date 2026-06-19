// Do not remove. This define tells `malicious_check` to not abort.
#define MAL_TEST_MODE

#include "orq.h"

using namespace orq::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

enum TestOutput { INVALID, PASS, FAIL };

/**
 * @brief Parties broadcast their checks, and then take AND of all received.
 * This ensure tests pass regardless of which party actually detected the
 * cheating.
 *
 */
TestOutput joint_malicious_check() {
    bool my_check = runTime->malicious_check();
    int r = true;

    for (int p = 1; p < runTime->getNumParties(); p++) {
        runTime->comm0()->sendShare(my_check, p);
    }
    for (int p = 1; p < runTime->getNumParties(); p++) {
        runTime->comm0()->receiveShare(r, p);
        my_check &= r;
    }

    // Allow us to run more tests.
    runTime->reset_malicious_state();
    return my_check ? PASS : FAIL;
}

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();

    if constexpr (!MALICIOUS_PROTOCOL) {
        single_cout("Malicious checks... skipped");
        return 0;
    }

    const int test_size = 1000;

    Vector<int> x(test_size), y(test_size);

    ASharedVector<int> a1 = secret_share_a(x, 0);
    ASharedVector<int> a2 = secret_share_a(y, 1);

    a1 *= a2;
    a1.open();
    assert(joint_malicious_check() == PASS);

    if (pID == 1) {
        // P1 cheats on one of its shares
        a1.vector(0)[test_size / 2] += 1;
    }

    // Call to `open()` will detect cheating
    a1.open();
    assert(joint_malicious_check() == FAIL);

    // Hashes should reset after a failed (non-abort) check. Should pass because
    // only local operations.
    auto c = a1 + a2;
    assert(joint_malicious_check() == PASS);

    // But open will catch it.
    c->open();
    assert(joint_malicious_check() == FAIL);

    // Check passes if we don't use manipulated data
    auto d = a2 * a2;
    assert(joint_malicious_check() == PASS);

    // But fails if we do
    auto e = a1 * a2;
    assert(joint_malicious_check() == FAIL);

    // Check boolean
    BSharedVector<int> b1 = secret_share_b(x, 0);
    BSharedVector<int> b2 = secret_share_b(y, 1);

    if (pID == 1) {
        // flip some bits
        b1.vector(0)[test_size / 3] ^= 0xffff;
    }

    auto f = b1 ^ b2;
    assert(joint_malicious_check() == PASS);

    auto g = b1 & b2;
    assert(joint_malicious_check() == FAIL);

    single_cout("Malicious primitives... OK");

    orq::random::PermutationManager::get()->reserve(test_size, 2);

    a2.shuffle();
    assert(joint_malicious_check() == PASS);

    // NOTE: this only works with P0 and P1. P2 / P3 have their shares overwritten.
    if (pID == 0) {
        a2.vector(0)[1] += 1;
    }

    a2.shuffle();
    a2.open();

    assert(joint_malicious_check() == FAIL);
    single_cout("Malicious arithmetic shuffle... OK");
}