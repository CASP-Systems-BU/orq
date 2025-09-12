/**
 * @file ex1_secret_sharing.cpp
 * @brief Example 1: Secret Sharing
 *
 * To run this example, use the `run_experiment` script as detailed in the README.
 *
 * ```
 * ../scripts/run_experiment.sh ... ex1_secret_sharing
 * ```
 *
 * This example also shows the bare minimum required to run an ORQ program. It works with any
 * protocol.
 */

#include "orq.h"

// Tell ORQ to use the selected protocol & communicator
// Use the run_experiments script or CMake options to select. See the README for
// more details.
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char **argv) {
    // This call must be the first line of all ORQ programs.
    orq_init(argc, argv);

    // Sometimes it is useful to know which party you are
    auto pID = runTime->getPartyID();

    // A plaintext Vector of 10 elements (assume this is P0's private data)
    orq::Vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Party 0 shares their secret data to all other parties using arithmetic (additive) sharing
    ASharedVector<int> secret_a = secret_share_a(data, 0);

    // ...and boolean sharing
    BSharedVector<int> secret_b = secret_share_b(data, 0);

    // Only P0 prints this.
    single_cout("AShares:");

    // All parties print their a secret share: random!
    // These will add up to `data`
    // Note: for replicated sharing schemes (3PC, 4PC), this is just one of the party's shares.
    all_cout("A " << container2str(secret_a.asEVector()(0)));

    single_cout("BShares:");

    // These will xor to `data`
    all_cout("B " << container2str(secret_b.asEVector()(0)));

    // This gives back a Vector
    auto reconstruct = secret_a.open();

    single_cout("Plaintext again:");
    single_cout(container2str(reconstruct));
}