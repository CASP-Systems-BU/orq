#include <algorithm>
#include <numeric>
#include <random>

#include "orq.h"
#include "util.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::aggregators;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// #define PRINT

#ifdef PRINT
#define SIZE (1 << 3)
#else
#define SIZE (1 << 14)
#endif

int pid;
int correct;

using namespace orq::aggregators;

void test_minimal_agg_example() {
    const size_t test_size = 8;

    std::vector<int> _data(test_size);
    std::iota(_data.begin(), _data.end(), 1);
    std::shuffle(_data.begin(), _data.end(), std::default_random_engine{});

    orq::Vector data(_data);
    orq::Vector<int> group({0, 0, 0, 0, 0, 1, 1, 1});

    BSharedVector<int> db = secret_share_b(data, 0);
    BSharedVector<int> gb = secret_share_b(group, 0);
    BSharedVector<int> rb(test_size);

    ASharedVector<int> da = secret_share_a(data, 0);
    ASharedVector<int> ra(test_size);

    std::vector<BSharedVector<int>> keys = {gb};
    aggregate(keys,
              {
                  {db, rb, max},
              },
              {
                  {da, ra, sum},
              });

    auto rbo = rb.open();
    auto rao = ra.open();
    // print(data, pid);
    // print(rao, pid);
    // print(rbo, pid);
}

template <typename S>
void test_vector_aggregation(const size_t test_size = 32) {
    single_cout("Testing " << std::numeric_limits<std::make_unsigned_t<S>>::digits
                           << "-bit vector aggregation...");
    std::vector<S> data(test_size);
    // TODO: random values
    std::iota(data.begin(), data.end(), 1);

    std::shuffle(data.begin(), data.end(), std::default_random_engine{});

    const S t_max = std::numeric_limits<S>::max();
    const S t_min = std::numeric_limits<S>::min();

    S g1_sum = 0;
    S g0_sum = 0;

    S g0_max = t_min;
    S g0_min = t_max;

    // This might overflow. That's ok.
    const size_t GROUP_ONE_SIZE = test_size - 1;  // * 4 / 7;

    // generate vectors, compute expected test outputs
    orq::Vector<S> group(test_size);
    for (int i = 0; i < test_size; i++) {
        if (i < GROUP_ONE_SIZE) {
            group[i] = 1;
            g1_sum += data[i];
        } else {
            g0_sum += data[i];
            g0_max = std::max(g0_max, data[i]);
            g0_min = std::min(g0_min, data[i]);
        }
    }

    orq::Vector<S> _dv(data);
    orq::Vector<S> _gv(group);

    ASharedVector<S> da = secret_share_a(_dv, 0);
    ASharedVector<S> ga = secret_share_a(_gv, 0);
    ASharedVector<S> ra1(test_size), ra2(test_size), ra3(test_size), rar(test_size);

    BSharedVector<S> db = secret_share_b(_dv, 0);
    BSharedVector<S> gb = secret_share_b(_gv, 0);
    BSharedVector<S> rb1(test_size), rb2(test_size), rb3(test_size), rbr(test_size);

    // Check operations

    // not reversed: results at top of group
    std::vector<BSharedVector<S>> keys = {gb};
    aggregate(keys,
              {
                  {db, rb1, max},
                  {db, rb2, min},
                  {db, rb3, copy},
              },
              {
                  {da, ra1, sum},
                  {da, ra2, count},
                  {da, ra3, copy},
              });

    // Check a few reversed: results at bottom of group
    aggregate(keys,
              {
                  {db, rbr, max},
              },
              {
                  {da, rar, sum},
              },
              Direction::Reverse);

    auto sum_ = ra1.open();
    auto sum_rev = rar.open();
    auto count_ = ra2.open();

    auto max_ = rb1.open();
    auto max_rev = rbr.open();
    auto min_ = rb2.open();

#ifdef PRINT
    single_cout_nonl("group: ");
    print(_gv, pid);

    single_cout_nonl("data:  ");
    print(_dv, pid);

    auto v = rb1.open();
    single_cout_nonl("max:   ");
    print(v, pid);

    v = rbr.open();
    single_cout_nonl("maxrev:");
    print(v, pid);

    v = rb2.open();
    single_cout_nonl("min:   ");
    print(v, pid);

    v = rb3.open();
    single_cout_nonl("id b:  ") print(v, pid);

    single_cout("----");
    single_cout_nonl("sum:   ");
    print(sum_, pid);

    v = rar.open();
    single_cout_nonl("sumrev: ");
    print(v, pid);

    v = ra2.open();
    single_cout_nonl("count: ");
    print(v, pid);

    v = ra3.open();
    single_cout_nonl("id a: ");
    print(v, pid);
#endif

    if (GROUP_ONE_SIZE) {
        ASSERT_SAME(sum_[0], g1_sum);
        ASSERT_SAME(sum_rev[GROUP_ONE_SIZE - 1], g1_sum);
    }
    ASSERT_SAME(sum_[GROUP_ONE_SIZE], g0_sum);
    ASSERT_SAME(sum_rev[test_size - 1], g0_sum);

    if (test_size <= t_max) {
        if (GROUP_ONE_SIZE) {
            ASSERT_SAME(count_[0], GROUP_ONE_SIZE);
        }
        ASSERT_SAME(count_[GROUP_ONE_SIZE], (test_size - GROUP_ONE_SIZE));
    } else {
        single_cout("Skipping count because type too small " << "for vector of length " << test_size
                                                             << ".");
    }

    ASSERT_SAME(max_[GROUP_ONE_SIZE], g0_max);
    ASSERT_SAME(max_rev[test_size - 1], g0_max);
    ASSERT_SAME(min_[GROUP_ONE_SIZE], g0_min);
}

template <typename S>
void test_distinct(const int test_size = 32) {
    single_cout("Testing " << std::numeric_limits<std::make_unsigned_t<S>>::digits
                           << "-bit distinct...");
    orq::Vector<S> data(test_size);
    orq::Vector<S> truth(test_size);

    S last = -1;

    // fill with some nice repeating pattern
    for (int i = 0; i < test_size;) {
        int j;
        for (j = 0; j <= i; j++) {
            if (i + j >= test_size) {
                break;
            }
            data[i + j] = i;
        }

        // Handle possible overflows for small data types
        if ((S)i != last) {
            truth[i] = 1;
        }
        last = i;

        i += j;
    }

    BSharedVector<S> db = secret_share_b(data, 0);
    BSharedVector<S> out(test_size);
    std::vector<BSharedVector<S>> keys = {db};
    orq::operators::distinct(keys, out);

    auto distinct = out.open();

    assert(truth.same_as(distinct));
}

template <typename S>
void test_multi_distinct() {
    single_cout("Testing " << std::numeric_limits<std::make_unsigned_t<S>>::digits
                           << "-bit multi-key distinct...");
    auto pid = runTime->getPartyID();
    std::vector<orq::Vector<S>> c1 = {{0, 0, 0, 1, 1, 1, 2}, {0, 0, 1, 1, 1, 0, 2}, Vector<int>(7)};
    EncodedTable<S> t1 = secret_share(c1, {"[K1]", "[K2]", "[D]"});

    t1.distinct({"[K1]"}, "[D]");
    auto v = ((BSharedVector<S>*)t1["[D]"].contents.get())->open();
    assert(v.same_as({1, 0, 0, 1, 0, 0, 1}));

    t1.distinct({"[K2]"}, "[D]");
    v = ((BSharedVector<S>*)t1["[D]"].contents.get())->open();
    assert(v.same_as({1, 0, 1, 0, 0, 1, 1}));

    t1.distinct({"[K1]", "[K2]"}, "[D]");
    v = ((BSharedVector<S>*)t1["[D]"].contents.get())->open();
    assert(v.same_as({1, 0, 1, 1, 0, 1, 1}));
}

// TODO: auto generate tables and test results as above.
template <typename S>
void test_table_operators() {
    single_cout("Testing " << std::numeric_limits<std::make_unsigned_t<S>>::digits
                           << "-bit tables...");

    // streaming ops and division currently only work for int32.
    const bool do_int32_operations = std::is_same<S, int>::value;

    const std::vector<std::string> schema = {
        "[SEL]",      "[DATA]",          "DATA",         "SUM",
        "SUM_R",      "[MAX]",           "[MAX_R]",      "[MIN]",
        "[MIN_R]",    "TUMBLING_WINDOW", "[GAP_WINDOW]", "[THRESHOLD_WINDOW]",
        "[DISTINCT]", "COUNT",           "COUNT_R",      "[SUM]",
        "[DIV]",
    };

#define NUM_ROWS 8

    std::vector<orq::Vector<S>> columns = {
        {1, 1, 1, 1, 0, 0, 0, 0}, {1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 4, 5, 6, 7, 8},
        Vector<S>(NUM_ROWS),      Vector<S>(NUM_ROWS),      Vector<S>(NUM_ROWS),
        Vector<S>(NUM_ROWS),      Vector<S>(NUM_ROWS),      Vector<S>(NUM_ROWS),
        Vector<S>(NUM_ROWS),      Vector<S>(NUM_ROWS),      Vector<S>(NUM_ROWS),
        Vector<S>(NUM_ROWS),      Vector<S>(NUM_ROWS),      Vector<S>(NUM_ROWS),
        Vector<S>(NUM_ROWS),      Vector<S>(NUM_ROWS),
    };
    EncodedTable<S> t = secret_share(columns, schema);

    t.sort({"[DATA]"}, DESC);

    using A = ASharedVector<S>;
    using B = BSharedVector<S>;

    t.aggregate({"[SEL]"},
                {
                    {"DATA", "SUM", orq::aggregators::sum<A>},
                    {"[DATA]", "[MAX]", orq::aggregators::max<B>},
                    {"[DATA]", "[MIN]", orq::aggregators::min<B>},
                    {"DATA", "COUNT", orq::aggregators::count<A>},
                },
                {.reverse = false, .mark_valid = false});

#ifdef PRINT
    print_table(t.open_with_schema(), pid);
#endif

    // NOTE: once aggregations condense their results, this reverse aggregation
    // will need to be performed on (a copy of) the original table, since `t`
    // will no longer contain the full data set.

    t.aggregate({"[SEL]"},
                {
                    {"DATA", "SUM_R", orq::aggregators::sum<A>},
                    {"[DATA]", "[MAX_R]", orq::aggregators::max<B>},
                    {"[DATA]", "[MIN_R]", orq::aggregators::min<B>},
                    {"DATA", "COUNT_R", orq::aggregators::count<A>},
                },
                {.reverse = true, .mark_valid = false});

    t.distinct({"[SEL]"}, "[DISTINCT]");

    // streaming operators only work for int, and non-2PC
    if constexpr (do_int32_operations) {
        t.tumbling_window("DATA", 2, "TUMBLING_WINDOW");
        t.gap_session_window({"[SEL]"}, "DATA", "[DATA]", "[GAP_WINDOW]", 2);
        t.threshold_session_window({"[SEL]"}, "[DATA]", "[DATA]", "[THRESHOLD_WINDOW]", 3, true,
                                   false);
    }

    t.convert_a2b("SUM_R", "[SUM]");
    t["[DIV]"] = t["[SUM]"] / t["[DATA]"];
    auto R = t.open_with_schema();

#ifdef PRINT
    print_table(R, pid);
#endif

    // Count for each group is 4, so we should have 2 occurrences
    auto count_col = t.get_column(R, "COUNT");
    auto count_r_col = t.get_column(R, "COUNT_R");
    ASSERT_SAME(2, _COUNT(count_col, 4));
    ASSERT_SAME(2, _COUNT(count_r_col, 4));

    // Sum for the two groups is 10 and 26
    auto sum_col = t.get_column(R, "SUM");
    auto sum_r_col = t.get_column(R, "SUM_R");
    ASSERT_SAME(1, _COUNT(sum_col, 10));
    ASSERT_SAME(1, _COUNT(sum_col, 26));
    ASSERT_SAME(1, _COUNT(sum_r_col, 10));
    ASSERT_SAME(1, _COUNT(sum_r_col, 26));

    // min and max: depending on aggregation direction, may have different
    // numbers of each value. just check inequality.
    auto min_col = t.get_column(R, "[MIN]");
    assert(_COUNT(min_col, 1) >= 1);
    assert(_COUNT(min_col, 5) >= 1);
    min_col = t.get_column(R, "[MIN_R]");
    assert(_COUNT(min_col, 1) >= 1);
    assert(_COUNT(min_col, 5) >= 1);

    auto max_col = t.get_column(R, "[MAX]");
    assert(_COUNT(max_col, 4) >= 1);
    assert(_COUNT(max_col, 8) >= 1);
    max_col = t.get_column(R, "[MAX_R]");
    assert(_COUNT(max_col, 4) >= 1);
    assert(_COUNT(max_col, 8) >= 1);

    if constexpr (do_int32_operations) {
        // Other time series operators: gap, threshold, tumbling
        auto gap_col = t.get_column(R, "[GAP_WINDOW]");
        ASSERT_SAME(4, _COUNT(gap_col, 1));
        ASSERT_SAME(4, _COUNT(gap_col, -1));
        auto thresh_col = t.get_column(R, "[THRESHOLD_WINDOW]");
        ASSERT_SAME(1, _COUNT(thresh_col, 4));
        ASSERT_SAME(7, _COUNT(thresh_col, -1));
        auto tumbl_col = t.get_column(R, "TUMBLING_WINDOW");
        ASSERT_SAME(2, _COUNT(tumbl_col, 1));
        ASSERT_SAME(2, _COUNT(tumbl_col, 2));
        ASSERT_SAME(2, _COUNT(tumbl_col, 3));

        auto div_col = t.get_column(R, "[DIV]");
        auto data_col = t.get_column(R, "[DATA]");
        assert(div_col.same_as(sum_r_col / data_col));
    }

    t["SUM"].zero();

    ///////////////////
    // Test mark-valid
    EncodedTable<S> t2 = secret_share(columns, schema);
    t2.aggregate({"[SEL]"}, {
                                {"DATA", "SUM", sum<A>},
                            });

    auto R2 = t2.open_with_schema();

#ifdef PRINT
    print_table(R2, pid);
#endif

    ASSERT_SAME(2, R2.first[0].size());
    auto sum_col2 = t2.get_column(R2, "SUM");
    // For some reason, 8-bit table returns (correct) result in opposite order,
    // so can't just check vector equal.
    ASSERT_CONTAINS(sum_col2, 26);
    ASSERT_CONTAINS(sum_col2, 10);

    ////////////
    // ...and in the opposite direction
    EncodedTable<S> t3 = secret_share(columns, schema);
    t3.aggregate({"[SEL]"},
                 {
                     {"DATA", "SUM_R", orq::aggregators::sum<A>},
                 },
                 {.reverse = true});

    auto Rrev = t3.open_with_schema();

#ifdef PRINT
    print_table(Rrev, pid);
#endif

    ASSERT_SAME(2, Rrev.first[0].size());
    auto sum_col_rev = t3.get_column(Rrev, "SUM_R");
    // For some reason, 8-bit table returns (correct) result in opposite order,
    // so can't just check vector equal.
    ASSERT_CONTAINS(sum_col_rev, 26);
    ASSERT_CONTAINS(sum_col_rev, 10);
}

/**
 * @brief Special test to confirm that MAX aggregation is properly incrementally
 * computed (i.e., result is monotonic). We crucially rely on this fact for the
 * implementation of the threshold session window operator.
 *
 * This was previously broken due to a incorrect argument to an access pattern;
 * it's a gnarly bug and even the correct output is a bit tricky to follow, so
 * I think this is a good test to keep.
 *
 */
void test_max_monotonic() {
    const int VEC_LENGTH = 1 << 8;
    const int NUM_TRIALS = 8;

    single_cout("Testing max aggregation incremental computation (" + std::to_string(NUM_TRIALS) +
                " trials)...");

    Vector<int> x_(VEC_LENGTH);
    BSharedVector<int> y(VEC_LENGTH);

    std::vector<BSharedVector<int>> empty_keys = {};

    for (int _t = 0; _t < NUM_TRIALS; _t++) {
        runTime->populateLocalRandom(x_);

        // std::iota(x_.begin(), x_.end(), 1);
        // std::shuffle(x_.begin(), x_.end(), std::default_random_engine{});

        BSharedVector<int> x = secret_share_b(x_, 0);

        aggregate(empty_keys, {{x, y, orq::aggregators::max}}, {}, Direction::Reverse);

        auto o = y.open();

        assert(std::is_sorted(std::begin(o), std::end(o)));
    }
}

void test_prefix_sum() {
    const int LENGTH = 1 << 4;

    auto pid = runTime->getPartyID();

    single_cout("Testing prefix sum...");

    // Plaintext operation
    Vector<int> x(LENGTH), y(LENGTH), yrev(LENGTH), z(LENGTH);

    // Only party 0 checks plaintext
    if (pid == 0) {
        runTime->populateLocalRandom(x);
        x %= 10;

        y = x;

        // Call linear Vector version
        y.prefix_sum();

        yrev = x.directed_subset_reference(-1);
        yrev.prefix_sum();

        z = x;
        tree_prefix_sum(z, true);
        assert(z.same_as(yrev.directed_subset_reference(-1)));

        z = x;
        tree_prefix_sum(z);
        assert(z.same_as(y));
    }

    // Shared Vectors
    ASharedVector<int> sh_a = secret_share_a(x, 0);
    tree_prefix_sum(sh_a);
    z = sh_a.open();

    if (pid == 0) {
        assert(z.same_as(y));
    }

    BSharedVector<int> sh_b = secret_share_b(x, 0);
    tree_prefix_sum(sh_b);
    z = sh_b.open();

    if (pid == 0) {
        assert(z.same_as(y));
    }
}

int main(int argc, char** argv) {
    orq_init(argc, argv);
    // The party's unique id
    pid = runTime->getPartyID();

    test_minimal_agg_example();

#ifndef PRINT
    test_vector_aggregation<int8_t>(SIZE);
    test_vector_aggregation<int64_t>(SIZE);
#endif
    test_vector_aggregation<int>(SIZE);

    test_distinct<int8_t>(SIZE);
    test_distinct<int32_t>(SIZE);
    test_distinct<int64_t>(SIZE);

    test_multi_distinct<int>();

    test_table_operators<int>();
    test_table_operators<int8_t>();
    test_table_operators<int64_t>();

    test_max_monotonic();

    test_prefix_sum();

    // Tear down communication

    return 0;
}
