#include "../../include/secrecy.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

/*
 * Run microbenchmark for join.
 *
 * We construct a table of towns, and then randomly generate people who live
 * in each town, to compare performance of join over multiple sizes of tables.
 *
 * To make sorting efficient (currently using bitonic), we enforce that the
 * total size of the data being joined should be a power of two.
 *
 * I think the size of the L and R tables shouldn't actually matter, since after
 * concatenation, which row is which is entirely oblivious.
 */

#define NUM_TOWNS 16

#define NUM_AGGREGATIONS 4

#define MIN_TABLE_EXPONENT 5
#define MAX_TABLE_EXPONENT 20

std::vector<secrecy::Vector<int>> build_town_table(int n) {
    std::vector<secrecy::Vector<int>> res = {
        secrecy::Vector<int>(NUM_TOWNS), secrecy::Vector<int>(NUM_TOWNS),
        secrecy::Vector<int>(NUM_TOWNS), secrecy::Vector<int>(NUM_TOWNS),
        secrecy::Vector<int>(NUM_TOWNS),
    };

    while (n-- > 0) {
        // town ID
        res[0][n] = n;
        // mayor ID
        res[1][n] = rand() % 128;
        // population
        res[2][n] = rand() % 1000000;
        // taxes
        res[3][n] = rand() % 1000;
        // schools
        res[4][n] = rand() % 5;
    }
    return res;
}

std::vector<secrecy::Vector<int>> build_citizens(int n) {
    std::vector<secrecy::Vector<int>> res = {
        secrecy::Vector<int>(n),
        secrecy::Vector<int>(n),  // Income
        secrecy::Vector<int>(n),
        secrecy::Vector<int>(n),  // Cars
        secrecy::Vector<int>(n),
        secrecy::Vector<int>(n),  // House Sqft
        secrecy::Vector<int>(n),
        secrecy::Vector<int>(n),  // Children
        secrecy::Vector<int>(n),
    };

    while (n-- > 0) {
        // town id
        res[0][n] = rand() % NUM_TOWNS;
    }
    return res;
}

const std::vector<std::string> town_schema = {"[TownID]", "MayorID", "Population", "Taxes",
                                              "Schools"};
const std::vector<std::string> citizen_schema = {"[TownID]",  "Income",    "TotalIncome",
                                                 "Cars",      "TotalCars", "HouseSqft",
                                                 "TotalSqft", "Children",  "TotalChildren"};

void run_join_test(EncodedTable<int> towns, EncodedTable<int> citizens, int num_joins,
                   int num_aggs) {
    using A = ASharedVector<int>;
    using B = BSharedVector<int>;

    std::vector<std::tuple<std::string, std::string, AggregationSelector<A, B>>> Agg;

    for (int i = 0; i < num_joins; i++) {
        Agg.push_back({town_schema[i + 1], town_schema[i + 1], secrecy::aggregators::copy<A>});
    }

    for (int i = 0; i < num_aggs; i++) {
        Agg.push_back(
            {citizen_schema[2 * i + 1], citizen_schema[2 * i + 2], secrecy::aggregators::sum<A>});
    }

    auto t1 = std::chrono::steady_clock::now();
    auto joined = towns.inner_join(citizens, {"[TownID]"}, Agg);
    auto t2 = std::chrono::steady_clock::now();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

    single_cout(num_joins << " join, " << num_aggs << " agg: " << elapsed_ms << " ms");
}

void micro_uniq_join() {
    const int min_exp = 10;
    const int max_exp = 24;

    using A = ASharedVector<int>;

    const int repeat = 3;

    for (int b = min_exp; b <= max_exp; b++) {
        size_t total_size = 1 << b;

        // Arbitrary split. Doesn't matter since we just concatenate.
        size_t L = total_size / 2;
        size_t R = total_size / 2;

        auto k1 = Vector<int>(L);
        auto a1 = Vector<int>(L);
        auto b1 = Vector<int>(L);

        EncodedTable<int> t1 = secret_share<int>({k1, a1, b1}, {"[K]", "A", "B"});

        auto k2 = Vector<int>(R);
        auto c2 = Vector<int>(R);

        EncodedTable<int> t2 = secret_share<int>({k2, c2}, {"[K]", "C"});

        single_cout(std::format("Start total size={}", total_size));

        // First test: join only, no attribute copy. basically a semijoin
        for (int i = 0; i < repeat; i++) {
            stopwatch::get_elapsed();
            t1.inner_join(t2, {"[K]"});
            auto un = stopwatch::get_elapsed();

            t1.uu_join(t2, {"[K]"});
            auto uu = stopwatch::get_elapsed();

            auto speedup = (double)un / uu;
            single_cout(std::format("  semi: un={} uu={}  {:.2f}x", un, uu, speedup));
        }

        // Second test: full join, copy attribute (not much overhead in either case)
        for (int i = 0; i < repeat; i++) {
            stopwatch::get_elapsed();
            t1.inner_join(t2, {"[K]"}, {{"C", "C", copy<A>}});
            auto un = stopwatch::get_elapsed();

            t1.uu_join(t2, {"[K]"}, {{"C", "C", copy<A>}});
            auto uu = stopwatch::get_elapsed();

            auto speedup = (double)un / uu;
            single_cout(std::format("  full: un={} uu={}  {:.2f}x", un, uu, speedup));
        }
    }
}

int main(int argc, char** argv) {
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

    bool run_uniq_join_micro = true;

    if (run_uniq_join_micro) {
        micro_uniq_join();
    } else {
        single_cout("JOIN benchmarks: Generating " << NUM_TOWNS << " towns");

        auto town_data = build_town_table(NUM_TOWNS);
        EncodedTable<int> towns = secret_share(town_data, town_schema);

        print_table(towns.open_with_schema(), pid);

        // single_cout("n concat sort aggregation overall");

        for (int s = MIN_TABLE_EXPONENT; s <= MAX_TABLE_EXPONENT; s++) {
            auto total_size = 1 << s;
            auto num_citizens = total_size - NUM_TOWNS;

            auto citizen_data = build_citizens(num_citizens);
            EncodedTable<int> citizens = secret_share(citizen_data, citizen_schema);

            single_cout("==== Table size " << total_size);

            for (auto nj = 0; nj <= NUM_AGGREGATIONS; nj++) {
                run_join_test(towns, citizens, nj, nj);
            }
        }
    }

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}