#include "../include/secrecy.h"

using namespace secrecy::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

/**
 * @brief Parties broadcast their checks, and then take AND of all received.
 * This ensure tests pass regardless of which party actually detected the
 * cheating.
 * 
 */
bool joint_malicious_check() {
    bool my_check = runTime->malicious_check(false);
    int r = true;

    for (int p = 1; p < runTime->getNumParties(); p++) {
        runTime->comm0()->sendShare(my_check, p);
    }
    for (int p = 1; p < runTime->getNumParties(); p++) {
        runTime->comm0()->receiveShare(r, p);
        my_check &= r;
    }
   
    return my_check;
}

int main(int argc, char ** argv) {
    secrecy_init(argc, argv);

#ifdef MALICIOUS_PROTOCOL
    auto pid = runTime->getPartyID();

    const int test_size = 1000;

    Vector<int> x(test_size), y(test_size);

    ASharedVector<int> a1 = secret_share_a(x, 0);
    ASharedVector<int> a2 = secret_share_a(y, 1);

    if (pid == 1) {
        // P1 cheats on one of its shares
        a1.vector(0)[test_size / 2] += 1;
    }

    // Call to `open()` will detect cheating
    a1.open();
    assert(! joint_malicious_check());

    // Hashes should reset after a failed (non-abort) check. Should pass because
    // only local operations.
    auto c = a1 + a2;
    assert(joint_malicious_check());

    // But open will catch it.
    c->open();
    assert(! joint_malicious_check());

    // Check passes if we don't use manipulated data
    auto d = a2 * a2;
    assert(joint_malicious_check());

    // But fails if we do
    auto e = a1 * a2;
    assert(! joint_malicious_check());

    // Check boolean

    BSharedVector<int> b1 = secret_share_b(x, 0);
    BSharedVector<int> b2 = secret_share_b(y, 1);

    if (pid == 1) {
        // flip some bits
        b1.vector(0)[test_size / 3] ^= 0xffff;
    }

    auto f = b1 ^ b2;
    assert(joint_malicious_check());

    auto g = b1 & b2;
    assert(! joint_malicious_check());

    single_cout("Malicious checks... OK");
#else
    single_cout("Malicious checks... skipped");
#endif

    MPI_Finalize();
}