#include "../../include/secrecy.h"
#include <numeric>

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;
using namespace secrecy::debug;
using namespace secrecy::service;

int main(int argc, char** argv) {
    secrecy_init(argc, argv);
    auto pID = secrecy::service::runTime->getPartyID();

    // Trying Vector operations
    Vector<int64_t> a(257); // = {0, 10, 2, 2, 3, 3, 1, 9, 0, 3};
    Vector<int64_t> b(257); // = {0, 1, 5, 2, 3, 7, 1, 2, 11, 3};
    Vector<int64_t> c1 = a * b;
    Vector<int64_t> c2 = a > b;

    ASharedVector<int64_t> a_ = secret_share_a(a, 0);
    ASharedVector<int64_t> b_ = secret_share_a(b, 0);
    ASharedVector<int64_t> c1_ = a_ * b_;

    auto c1_opened = c1_.open();
    
    if (pID == 0) std::cout << "-------------------MULT-------------------" << std::endl;
    // secrecy::debug::print(c1, pID);
    // secrecy::debug::print(c1_opened, pID);


    BSharedVector<int64_t> a__ = secret_share_b(a, 0);
    BSharedVector<int64_t> b__ = secret_share_b(b, 0);
    BSharedVector<int64_t> c2__ = a__ > b__;

    auto c2__opened = c2__.open();

    if (pID == 0) std::cout << "-----------------GREATER-----------------" << std::endl;
    // secrecy::debug::print(c2, pID);
    // secrecy::debug::print(c2__opened, pID);

    runTime->malicious_check();

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif

    return 0;
}
