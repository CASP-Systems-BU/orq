/**
 * @file ex0_template.cpp
 * @brief Example 0: An empty file.
 *
 * Use this as a template for creating new ORQ programs.
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

    // Your code here!
}