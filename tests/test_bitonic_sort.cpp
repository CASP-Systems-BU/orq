#include "../include/secrecy.h"

#include <algorithm>

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    // Initialize Secrecy runtime
    secrecy_init(argc, argv);
    // The party's unique id
    auto pID = runTime->getPartyID();

    // **************************************** //
    //             Test swapping                //
    // **************************************** //

    // Input plaintext data for testing secure comparison
    secrecy::Vector<int> data_a = {111, -14, 0, -156, INT_MAX, 18};
    secrecy::Vector<int> data_b = {98,  -4,  0, 2847, INT_MIN, -4491};
    // Input bits to test secure swapping (1 means that the respective elements in data_a and data_b will be swapped)
    secrecy::Vector<int> bits = {0, 1, 1, 1, 1, 0};

    // Secret-share original vectors using boolean sharing
    BSharedVector<int> v1 = secret_share_b(data_a, 0);
    BSharedVector<int> v2 = secret_share_b(data_b, 0);
    // Secret-share original bits using boolean sharing
    BSharedVector<int> vb = secret_share_b(bits, 0);

    single_cout_nonl("Swapping... ");

    // Apply elementwise Secure swap according to the given bits
    secrecy::operators::swap(v1, v2, vb);
    // Compare swapped vectors with ground truth
    auto v1_open = v1.open();
    auto v2_open = v2.open();
    assert( v1_open.same_as({111, -4,  0, 2847, INT_MIN, 18}) &&
            v2_open.same_as({98,  -14, 0, -156, INT_MAX, -4491}) );

    single_cout("OK");

    // **************************************** //
    //                Test sort                 //
    // **************************************** //

    // Input plaintext data for testing secure sort
    secrecy::Vector<int> unordered = {0, 5, 4, 7, -1 , -2, 8, 1};

    // Secret-share original vector using boolean sharing
    BSharedVector<int> v = secret_share_b(unordered, 0);

    single_cout_nonl("Sort asc... ");

    // Sort vector in ASC order
    secrecy::operators::bitonic_sort(v);
    // Compare sorted vector with ground truth
    auto asc_open = v.open();
    assert(asc_open.same_as({-2, -1, 0, 1, 4, 5, 7, 8}));

    single_cout("OK");

    single_cout_nonl("Sort desc... ");

    // Sort vector in DESC order
    secrecy::operators::bitonic_sort(v, SortOrder::DESC);
    // Compare sorted vector with ground truth
    auto desc_open = v.open();
    assert(desc_open.same_as({8, 7, 5, 4, 1, 0, -1, -2}));

    single_cout("OK");

    single_cout_nonl("Sort with INT_MAX / 2... ");
    {
        secrecy::Vector<int> x = {0, INT_MAX / 2, -1, 42, 87, 100, -444, -13};
        BSharedVector<int> v = secret_share_b(x, 0);

        secrecy::operators::bitonic_sort(v);
        auto open = v.open();
        assert(std::is_sorted(std::begin(open), std::end(open)));
    }
    single_cout("OK");

    single_cout_nonl("Sort with INT_MAX / 2 + 1... ");
    {
        secrecy::Vector<int> x = {0, INT_MAX / 2 + 1, -1, 42, 87, 100, -444, -13};
        BSharedVector<int> v = secret_share_b(x, 0);

        secrecy::operators::bitonic_sort(v);
        auto open = v.open();

        assert(std::is_sorted(std::begin(open), std::end(open)));
    }
    single_cout("OK");

    // **************************************** //
    //             Test table sort              //
    // **************************************** //

    // // // WARNING // // //
    // These tests are pretty unreliable, because they rely on the order of
    // columns being returned by the table library. This is dependent on the
    // iterator order of the map, which should be consistent but may be
    // arbitrary. This is only a temporary solution until we implement a
    // plaintext table type.

    std::vector<secrecy::Vector<int>> columns = {{111,      111,    0,      111,     INT_MAX,   5,      5,     5},
                                                 {-14,      -4,     0,      -14,     INT_MIN,   13,     13,    13},
                                                 {INT_MIN,  0,      6423,   -11233,  INT_MIN,   7,      7,     7}};

    // Sort on a single column
    single_cout_nonl("Table sort on one column...");
    {
        EncodedTable<int> t = secret_share(columns, {"[A]", "[B]", "[C]"});
        t.sort({"[A]"}, ASC, secrecy::SortingProtocol::BITONICSORT);
        auto open = t.open_with_schema();

        auto col_a = t.get_column(open, "[A]");

        // Just check first column. TODO: check other columns
        assert(std::is_sorted(std::begin(col_a), std::end(col_a)));
    }
    single_cout("OK")


    // Input plaintext table to test secure sort on multiple columns:
    //
    // | col1   |   col2    |   col3 |
    // -------------------------------
    // 111      | -14       | INT_MIN
    // 111      | -4        | 0
    // 0        | 0         | 6423
    // 111      | -14       | -11233
    // INT_MAX  | INT_MIN   | INT_MIN
    // 5        | 13        | 7
    // 5        | 13        | 7
    // 5        | 13        | 7
    single_cout_nonl("Table sort on multiple columns... ")
    EncodedTable<int> t = secret_share(columns, {"[col_1]", "[col_2]", "[col_3]"});

    // Sort table on all columns in DESC->ASC->DESC order
    t.sort({{"[col_1]", DESC}, {"[col_2]", ASC}, {"[col_3]", DESC}}, secrecy::SortingProtocol::BITONICSORT);

    // Compare sorted table with ground truth:
    //
    // | col1   |   col2    |   col3 |
    // -------------------------------
    // INT_MAX  | INT_MIN   | INT_MIN
    // 111      | -14       | -11233
    // 111      | -14       | INT_MIN
    // 111      | -4        | 0
    // 5        | 13        | 7
    // 5        | 13        | 7
    // 5        | 13        | 7
    // 0        | 0         | 6423
    DataTable<int> truth = {
        {INT_MAX,    111,    111,        111,    5,      5,      5,      0},
        {INT_MIN,    -14,    -14,        -4,     13,     13,     13,     0},
        {INT_MIN,    -11233, INT_MIN,    0,      7,      7,      7,      6423}};

    // print_table(t.open_with_schema(), pID);

    DataTable<int> t_open = t.open();
    for (int i=0; i<truth.size(); i++) {
        assert(t_open[i].same_as(truth[i]));
    }

    single_cout("OK");

    single_cout_nonl("Table sort with some AShared columns... ");

    // Repeat test on table with A-shared columns
    t = secret_share(columns, {"[col_1]", "col_2", "[col_3]"});
    // Sort table on two columns in DESC->ASC order
    t.sort({{"[col_1]", DESC}, {"[col_3]", ASC}}, secrecy::SortingProtocol::BITONICSORT);

    // Compare sorted table with ground truth:
    //
    // | col1   |   col2    |   col3 |
    // -------------------------------
    // INT_MAX  | INT_MIN   | INT_MIN
    // 111      | -14       | INT_MIN
    // 111      | -14       | -11233
    // 111      | -4        | 0
    // 5        | 13        | 7
    // 5        | 13        | 7
    // 5        | 13        | 7
    // 0        | 0         | 6423
    truth = {
        {INT_MAX,    111,    111,        111,    5,      5,      5,      0},
        {INT_MIN,    INT_MIN, -11233,    0,      7,      7,      7,      6423},
        {INT_MIN,    -14,    -14,        -4,     13,     13,     13,     0},
    };

    t_open = t.open();
    for (int i=0; i<truth.size(); i++) {
        assert(t_open[i].same_as(truth[i]));
    }

    single_cout("OK");

    // Tear down communication
#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}