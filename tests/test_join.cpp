#include <ranges>

#include "../include/secrecy.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace secrecy::aggregators;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#define MASK_VALUE (std::numeric_limits<int>::max())

using A = ASharedVector<int>;
using B = BSharedVector<int>;

#define RUN_ASSERTS
// #define PRINT_TABLES

#ifndef RUN_ASSERTS
#undef assert
#define assert(x) (true)
#endif

#ifdef PRINT_TABLES
#define print_table(...) print_table(__VA_ARGS__)
#else
#define print_table(...)
#endif

void test_multi_key() {
    auto pid = runTime->getPartyID();
    // clang-format off
    std::vector<secrecy::Vector<int>> c1 = {
        {1, 1, 2, 3, 4},
        {1, 2, 4, 6, 7},
        Vector<int>(5)};
    // clang-format on
    EncodedTable<int> t1 = secret_share(c1, {"[K1]", "[K2]", "[D]"});

    std::vector<secrecy::Vector<int>> c2 = {{1, 1, 1, 3, 4, 4, 1, 1, 4, 5},
                                            {2, 1, 1, 6, 7, 7, 1, 2, 7, 8},
                                            {0, 1, 1, 2, 3, 5, 8, 3, 1, 4},
                                            Vector<int>(10)};
    EncodedTable<int> t2 = secret_share(c2, {"[K1]", "[K2]", "[D2]", "C"});

    single_cout("Single-key join...");
    auto tj1 = t1.inner_join(t2, {"[K1]"},
                             {
                                 {"C", "C", count<A>},
                             });

    print_table(tj1.open_with_schema(), pid);

    single_cout("Double-key join...");
    auto tj2 = t1.inner_join(t2, {"[K1]", "[K2]"},
                             {
                                 {"C", "C", count<A>},
                             });

    tj2.sort({"[K1]", "[K2]"}, ASC);

    tj2.deleteColumns({"[D]", "[D2]"});

    auto T = tj2.open_with_schema();
    auto count_col = tj2.get_column(T, "C");

    print_table(tj2.open_with_schema(), pid);

    assert(count_col.same_as({3, 2, 1, 3}));
}

void test_valid() {
    auto pid = runTime->getPartyID();
    // clang-format off
    std::vector<secrecy::Vector<int>> pk_data = {
        {1, 2, 3, 4},
        {100, 200, 300, 400},
        {1, 0, 1, 0}};
    // clang-format on
    EncodedTable<int> P = secret_share(pk_data, {"[K]", "[D2]", "[_VALID]"});
    P.filter(P["[_VALID]"]);

    // clang-format off
    std::vector<secrecy::Vector<int>> fk_data = {
        { 1,  1,  1,  1,  2,  3,  3,  3,  3,  5,  5},
        {10, 20, 30, 32, 35, 40, 50, 60, 65, 70, 80},
        { 1,  1,  0,  1,  1,  1,  0,  0,  1,  1,  0},
    };
    // clang-format on
    EncodedTable<int> F = secret_share(fk_data, {"[K]", "[Data]", "[_VALID]"});
    F.filter(F["[_VALID]"]);

    auto J = P.inner_join(F, {"[K]"},
                          {
                              {"[D2]", "[D2]", copy<B>},
                          });

    J.deleteColumns({"[_VALID]"});

    single_cout("Join with valid bit...");
    auto T = J.open_with_schema();
    print_table(T, pid);
    auto data_col = J.get_column(T, "[Data]");

    // don't worry about stable sort here.
    assert(same_elements(data_col, {10, 20, 32, 40, 65}));
}

void check_simple() {
    auto pid = runTime->getPartyID();

    single_cout("Simple correctness...");
    std::vector<secrecy::Vector<int>> c1 = {{1, 2}, {12345, 67890}};
    EncodedTable<int> t1 = secret_share(c1, {"[UID]", "[Zipcode]"});
    t1.tableName = "LOC";

    std::vector<secrecy::Vector<int>> c2 = {{2, 2}, {40, -60}};
    EncodedTable<int> t2 = secret_share(c2, {"[UID]", "[Amount]"});
    t2.tableName = "TXN";

    auto tc = t1.concatenate(t2);

    // Check sizes
    assert(tc.size() == t1.size() + t2.size());

    auto tj = t1.inner_join(
        t2, {"[UID]"},
        {
            {"[Zipcode]", "[Zipcode]", secrecy::aggregators::copy<BSharedVector<int>>},
        });
    // tj may trim
    assert(tc.size() >= tj.size());

    print_table(tj.open_with_schema());

    // Check schema
    auto js = tj.getSchema();
    assert(js.count("[UID]"));
    assert(js.count("[Zipcode]"));
    assert(js.count("[Amount]"));

    // Open and check values
    // clang-format off
    std::vector<secrecy::Vector<int>> truth = {
        {1, 1, MASK_VALUE, MASK_VALUE},
        {67890, 67890, MASK_VALUE, MASK_VALUE},
        {2, 2, MASK_VALUE, MASK_VALUE},
        {-60, 40, MASK_VALUE, MASK_VALUE},
    };
    // clang-format on

    auto open_join = tj.open();
    print_table(open_join, pid);
}

void test_normal_behavior() {
    auto pid = runTime->getPartyID();

    single_cout("Inner join...");

    std::vector<secrecy::Vector<int>> c1 = {{1, 2, 3, -5, 10},
                                            {12345, 67890, 11111, 98989, 22222},
                                            {18, 22, 82, -10, 0},
                                            {1111, 2222, 3333, 1040, 7777}};
    EncodedTable<int> t1 = secret_share(c1, {"[UID]", "[Zip]", "Age", "Amount"});
    t1.tableName = "User";

    // clang-format off
    std::vector<secrecy::Vector<int>> c2 = {
        {  1,   2,  3, 3,  2,  3, 3, 3, 3, 10,  3, 3, 3, 3, 4, 4, 4, 4, 4, -5, 10},
        {100, -30, 80, 9, 10, 18, 4, 5, 5,  7, 19, 8, 1, 2, 5, 6, 3, 3, 1,  8,  9},
        {  2,   8,  7, 1,  8,  3, 1, 0, 3,  4,  8, 1, 0, 1, 9, 0, 3, 9, 7,  8,  3}};
    // clang-format on
    EncodedTable<int> t2 = secret_share(c2, {"[UID]", "Amount", "[To]"});
    t2.tableName = "Txn";

    auto tc = t1.concatenate(t2);

    print_table(t1.open_with_schema(), pid);
    print_table(t2.open_with_schema(), pid);

    print_table(tc.open_with_schema(), pid);

    {
        auto tj = t1.inner_join(t2, {"[UID]"},
                                {
                                    {"[Zip]", "[Zip]", secrecy::aggregators::copy<B>},
                                });
        print_table(tj.open_with_schema(), pid);
    }

    {
        auto tj = t1.inner_join(t2, {"[UID]"},
                                {
                                    {"[Zip]", "[Zip]", secrecy::aggregators::copy<B>},
                                    {"Age", "Age", secrecy::aggregators::copy<A>},
                                });
        auto R = tj.open_with_schema();
        print_table(R, pid);

        {
            // Inner join tests
            auto zip_col = tj.get_column(R, "[Zip]");
            ASSERT_SAME(1, _COUNT(zip_col, 98989));
            ASSERT_SAME(1, _COUNT(zip_col, 12345));
            ASSERT_SAME(2, _COUNT(zip_col, 67890));
            ASSERT_SAME(10, _COUNT(zip_col, 11111));
            ASSERT_SAME(2, _COUNT(zip_col, 22222));

            auto uid_col = tj.get_column(R, "[UID]");
            ASSERT_SAME(10, _COUNT(uid_col, 3));  // many matches
            ASSERT_SAME(0, _COUNT(zip_col, 4));   // no match for UID 4
        }
    }

    t2.addColumns(std::vector<std::string>{"COUNT", "SUM_AMT"}, t2.size());

    single_cout("Aggregations...");

    {
        auto tj = t1.inner_join(t2, {"[UID]"},
                                {
                                    {"Amount", "COUNT", secrecy::aggregators::count<A>},
                                    {"Amount", "SUM_AMT", secrecy::aggregators::sum<A>},
                                    {"Age", "Age", secrecy::aggregators::copy<A>},
                                    {"[Zip]", "[Zip]", secrecy::aggregators::copy<B>},
                                });

        // tj.deleteColumns({"Age", "Amount", "SUM_AMT", "[To]"});

        auto R = tj.open_with_schema();
        print_table(R, pid);

        // Aggregation tests
        auto count_col = tj.get_column(R, "COUNT");
        ASSERT_CONTAINS(count_col, 1);
        ASSERT_CONTAINS(count_col, 2);
        ASSERT_CONTAINS(count_col, 10);

        auto uid_col = tj.get_column(R, "[UID]");
        ASSERT_CONTAINS(uid_col, -5);
        REFUTE_CONTAINS(uid_col, 4);

        auto sum_col = tj.get_column(R, "SUM_AMT");
        ASSERT_CONTAINS(sum_col, 8);
        ASSERT_CONTAINS(sum_col, 100);
        ASSERT_CONTAINS(sum_col, -20);
        ASSERT_CONTAINS(sum_col, 151);
        ASSERT_CONTAINS(sum_col, 16);
    }
}

void test_outer() {
    auto pid = runTime->getPartyID();

    std::vector<secrecy::Vector<int>> c1 = {
        {111, 222, 222, 333, 444, 444, 999},
        {100, 200, 201, 300, 400, 402, 900},
        {-1, -2, -2, -3, -4, -7, -5},
    };
    EncodedTable<int> t1 = secret_share(c1, {"[K]", "[D]", "A"});

    std::vector<secrecy::Vector<int>> c2 = {
        {222, 999, 666, 333},
        {1, 2, 3, 4},
    };
    EncodedTable<int> t2 = secret_share(c2, {"[K]", "D2"});

    single_cout("Left Outer...");

    auto tL = t2.left_outer_join(t1, {"[K]"});

    {
        auto opened = tL.open_with_schema();
        print_table(opened, pid);

        assert(tL.get_column(opened, "[K]").same_as({222, 222, 333, 666, 999}));
        assert(tL.get_column(opened, "D2").same_as({0, 0, 0, 3, 0}));
        assert(tL.get_column(opened, "[D]").same_as({200, 201, 300, 0, 900}));
        assert(tL.get_column(opened, "A").same_as({-2, -2, -3, 0, -5}));
    }

    single_cout("Right Outer...");
    auto tR = t2.right_outer_join(t1, {"[K]"}, {{"D2", "D2", copy<A>}});

    {
        auto opened = tR.open_with_schema();
        print_table(opened, pid);

        assert(tR.get_column(opened, "[K]").same_as({111, 222, 222, 333, 444, 444, 999}));
        assert(tR.get_column(opened, "D2").same_as({0, 1, 1, 4, 0, 0, 2}));
        assert(tR.get_column(opened, "[D]").same_as({100, 200, 201, 300, 400, 402, 900}));
    }

    single_cout("Full Outer...");
    auto tF = t2.full_outer_join(t1, {"[K]"});
    {
        auto opened = tF.open_with_schema();
        assert(tF.get_column(opened, "[K]").size() == t1.size() + t2.size());
    }
}

void test_semi_anti_join() {
    auto pid = runTime->getPartyID();

    std::vector<secrecy::Vector<int>> c1 = {
        {111, 222, 222, 333, 444, 444, 999},
        {100, 200, 201, 300, 400, 402, 900},
        {-1, -2, -2, -3, -4, -7, -5},
    };
    EncodedTable<int> t1 = secret_share(c1, {"[K]", "[D]", "A"});

    std::vector<secrecy::Vector<int>> c2 = {
        {222, 999, 666, 333},
        {1, 2, 3, 4},
    };
    EncodedTable<int> t2 = secret_share(c2, {"[K]", "D2"});

    single_cout("Semijoin...");

    auto tsj = t1.semi_join(t2, {"[K]"});

    {
        auto opened = tsj.open_with_schema();
        print_table(opened, pid);

        assert(tsj.getColumnNames() == t1.getColumnNames());
        assert(tsj.get_column(opened, "[D]").same_as({200, 201, 300, 900}));
        assert(tsj.get_column(opened, "A").same_as({-2, -2, -3, -5}));
    }

    single_cout("Antijoin... ");

    auto taj = t1.anti_join(t2, {"[K]"});
    {
        auto opened = taj.open_with_schema();
        print_table(opened, pid);

        assert(taj.getColumnNames() == t1.getColumnNames());
        assert(taj.get_column(opened, "[D]").same_as({100, 400, 402}));
        assert(taj.get_column(opened, "A").same_as({-1, -4, -7}));
    }

    // semi join + anti join is the same as the left table (t1)
    single_cout("Semi and anti complements...");
    {
        auto tb = tsj.concatenate(taj);
        tb.deleteColumns({ENC_TABLE_JOIN_ID});
        tb.sort({"[K]"});
        auto opened = tb.open_with_schema();
        print_table(opened, pid);

        assert(tb.getColumnNames() == t1.getColumnNames());
        // don't worry about stable sort
        assert(same_elements(tb.get_column(opened, "[K]"), c1[0]));
        assert(same_elements(tb.get_column(opened, "[D]"), c1[1]));
        assert(same_elements(tb.get_column(opened, "A"), c1[2]));
    }
}

void test_unique() {
    single_cout("Unique key join...");
    single_cout("  single key column");
    {
        std::vector<secrecy::Vector<int>> d1 = {
            {1, 2, 4, 6, 7, 9, 10, 11, 13, 15},
            {99, 88, 77, 66, 55, 44, 33, 22, 11, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        };

        // clang-format off
        std::vector<secrecy::Vector<int>> d2 = {
            {  2,   3,   4,   5,   6,   9,  12,  14},
            {111, 222, 333, 444, 555, 888, 777, 999},
        };
        // clang-format on

        EncodedTable<int> t1 = secret_share(d1, {"[K]", "A", "[Z]"});
        EncodedTable<int> t2 = secret_share(d2, {"[K]", "B"});

        auto tj = t1.uu_join(t2, {"[K]"});
        assert(tj.size() == std::min(t1.size(), t2.size()));

        auto cols = tj.getColumnNames();
        ASSERT_CONTAINS(cols, "[K]");
        REFUTE_CONTAINS(cols, "A");
        REFUTE_CONTAINS(cols, "[Z]");
        ASSERT_CONTAINS(cols, "B");

        auto op = tj.open_with_schema();
        print_table(op, runTime->getPartyID());

        assert(tj.get_column(op, "[K]").same_as({2, 4, 6, 9}));
        assert(tj.get_column(op, "B").same_as({111, 333, 555, 888}));
    }

    single_cout("  compound key");
    std::vector<secrecy::Vector<int>> d1 = {
        {0, 0, 0, 1, 1, 2, 2, 2, 2, 3, 4, 5, 5},
        {0, 1, 2, 0, 1, 0, 1, 2, 3, 3, 4, 5, 6},
        {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3},
        {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3},
    };

    std::vector<secrecy::Vector<int>> d2 = {
        {0, 1, 1, 1, 1, 2, 2, 3, 4, 4, 4, 4, 5},
        {1, 5, 4, 3, 2, 0, 2, 4, 1, 2, 3, 4, 5},
        {99, 88, 77, 66, 55, 44, 33, 22, 11, 0, -11, -22, -33},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };

    EncodedTable<int> t1 = secret_share(d1, {"[K1]", "[K2]", "A", "Q"});
    EncodedTable<int> t2 = secret_share(d2, {"[K1]", "[K2]", "B", "W"});

    auto tj = t1.uu_join(t2, {"[K1]", "[K2]"}, {{"A", "A", copy<A>}});
    assert(tj.size() == std::min(t1.size(), t2.size()));

    auto cols = tj.getColumnNames();
    ASSERT_CONTAINS(cols, "[K1]");
    ASSERT_CONTAINS(cols, "[K2]");
    ASSERT_CONTAINS(cols, "A");
    REFUTE_CONTAINS(cols, "Q");
    ASSERT_CONTAINS(cols, "B");
    ASSERT_CONTAINS(cols, "W");

    auto op = tj.open_with_schema();
    print_table(op, runTime->getPartyID());

    assert(tj.get_column(op, "[K1]").same_as({0, 2, 2, 4, 5}));
    assert(tj.get_column(op, "[K2]").same_as({1, 0, 2, 4, 5}));
    assert(tj.get_column(op, "B").same_as({99, 44, 33, -22, -33}));
    assert(tj.get_column(op, "W").same_as({1, 1, 1, 1, 1}));

    single_cout("  copy data");
    assert(tj.get_column(op, "A").same_as({8, 4, 2, -1, -2}));
}

int main(int argc, char** argv) {
    secrecy_init(argc, argv);

    auto pid = runTime->getPartyID();

    check_simple();
    test_normal_behavior();
    test_multi_key();
    test_valid();

    test_multi_key();

    test_outer();

    test_semi_anti_join();

    test_unique();

    // TODO: Add tests here
#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}
