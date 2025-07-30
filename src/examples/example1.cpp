/**
 * @brief A minimal example in which 3 parties compute the sum of a few numbers.
 * 
 * This program is a Hello World app for Secrecy. It is designed to work
 * specifically for three computing parties.
 */

#include "../../include/secrecy.h"
#if PROTOCOL_NUM != REPLICATED3
#error "This program only supports PROTOCOL_NUM == REPLICATED3"
#endif

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;
using namespace secrecy::debug;

int main(int argc, char **argv) {
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();

    // Each party has two data points.
    secrecy::Vector<int> data(2);
    
    // Each party loads its data into the vector. Here the data is hard-coded.
    // In practice, each party wants to keep its data confidential, so you could
    // read the data from a file instead.
    if (pID == 0) {
        // These are party zero's confidential values.
        data[0] = 100;
        data[1] = 10;
    } else if (pID == 1) {
        // These are party one's confidential values.
        data[0] = 200;
        data[1] = 20;
    } else {
        // These are party two's confidential values.
        data[0] = 300;
        data[1] = 30;
    }
    
    // Each party creates arithmetic shares of its vector and sends to the
    // other parties.
    ASharedVector<int> x0 = secret_share_a(data, 0);
    ASharedVector<int> x1 = secret_share_a(data, 1);
    ASharedVector<int> x2 = secret_share_a(data, 2);

    // Compute the sum on the shares.
    auto y = x0 + x1 + x2;

    // Opening reveals the result of the computation.
    auto z = y->open();
    print(z);
    
#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}
