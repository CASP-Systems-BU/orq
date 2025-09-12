/**
 * @file ex2_operators.cpp
 * @brief Example 2: Secret-Shared Operations
 *
 * To run this example, use the `run_experiment` script as detailed in the README.
 *
 * ```
 * ../scripts/run_experiment.sh ... ex2_operators
 * ```
 *
 * This example demonstrates ORQ's vectorized primitives for secret-shared computation.
 */

#include "orq.h"

// Tell ORQ to use the selected protocol & communicator
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char **argv) {
    orq_init(argc, argv);
    auto pID = orq::service::runTime->getPartyID();

    Vector<int> a(10), b(10);

    // Party 0 creates some secret data.
    if (pID == 0) {
        runTime->populateLocalRandom(a);
        runTime->populateLocalRandom(b);
        // Keep the values small and positive
        a = (a % 40) + 40;
        b = (b % 15) + 15;
    }

    single_cout("Plaintext inputs:");
    single_cout("a = " << container2str(a));
    single_cout("b = " << container2str(b));

    ///////////////////////////////////////
    // Operations over arithmetic shares //

    // P0 is the data owner.
    ASharedVector<int> x = secret_share_a(a, 0);
    ASharedVector<int> y = secret_share_a(b, 0);

    // ASharedVectors support addition, subtraction, multiplication
    ASharedVector<int> add = x + y;
    ASharedVector<int> mul = y * add;
    // As well as compound assignment
    mul *= x;
    // and public-constant division
    ASharedVector<int> d = mul / 10;

    auto mul_open = mul.open();
    auto d_open = d.open();

    // Don't put calls to open inside `single_cout`: open() is an interactive functionality, but
    // single_cout is a macro which only runs for party 0.
    single_cout("mul: " << container2str(mul_open));
    single_cout("div: " << container2str(d_open));

    ////////////////////////////////////
    // Operations over boolean shares //

    // Note: need to use different secret sharing function for different type.
    BSharedVector<int> u = secret_share_b(a, 0);
    BSharedVector<int> v = secret_share_b(b, 0);

    // BSharedVectors support the standard bitwise operators
    BSharedVector<int> p = u & v;
    // Using `auto` produces unique_ptr<BSharedVector>
    auto q = p ^ (~u);

    // We also support some complex circuits

    // Comparison (<, >, <=, >=, ==, !=)
    BSharedVector<int> lt = u < v;
    // Effectively multiply by 2
    lt <<= 1;

    // Boolean addition (either ripple-carry or parallel-prefix)
    auto diff = u - q;

    // Private division (using a non-restoring divison circuit)
    auto div = u / v;

    auto lt_open = lt.open();
    auto u_open = u.open();
    auto q_open = q->open();
    auto diff_open = diff->open();
    auto div_open = div->open();

    single_cout("2lt: " << container2str(lt_open));
    single_cout("u:   " << container2str(u_open));
    single_cout("q:   " << container2str(q_open));
    single_cout("u-q: " << container2str(diff_open));
    single_cout("u/v: " << container2str(div_open));
}