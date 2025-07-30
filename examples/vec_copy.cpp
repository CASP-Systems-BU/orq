#include "../include/secrecy.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    // Initialize Secrecy runtime
    secrecy_init(argc, argv);
    // The party's unique id
    auto pID = runTime->getPartyID();

    // Input data
    secrecy::Vector<int> data_a = {-4, 124, 41984};
    secrecy::Vector<int> data_b = {4, -124, -41984};

    // Secret-share original vectors using arithmetic sharing
    ASharedVector<int> a = secret_share_a(data_a, 0);
    ASharedVector<int> b = secret_share_a(data_b, 0);

    std::vector<ASharedVector<int>> v = {a};          // v[0] now points to a's data elements
    v.push_back(b);                                   // v[1] now points to b's data elements
    ASharedVector<int> c(3);                     // c is an A-shared vector with 3 elements initialized to 0
    c = a;                                            // c is now a copy of a (copy assignment)
    ASharedVector<int> d = b;                         // d points to b's data elements (constructor call)
    // Apply elementwise secure addition
    ASharedVector<int> a_plus_b = v[0] + v[1];
    b = v[0] = a_plus_b;                              // a, b, v[0], v[1] are now equal to a_plus_b

    // Open vectors and compare with true result
    secrecy::Vector<int> true_a_plus_b = {0, 0, 0},
                         a_open = a.open(),
                         b_open = b.open(),
                         c_open = c.open(),
                         v0_open = v[0].open(),
                         v1_open = v[1].open();
    assert(a_open.same_as(b_open) &&            // a must be equal to b
           v0_open.same_as(v1_open) &&          // v[0] must be equal to v[1]
           a_open.same_as(v0_open)  &&          // All of the above must be equal...
           v0_open.same_as(true_a_plus_b));     // ...and also equal to the true result
    assert(c_open.same_as(data_a));             // c must be equal to data_a (copy of the original a)

    // Print a and c contents to stdout
    if (pID==0) {
        print(a_open);  // 0 0 0
        print(c_open);  // -4, 124, 41984
    }

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif

    return 0;
}