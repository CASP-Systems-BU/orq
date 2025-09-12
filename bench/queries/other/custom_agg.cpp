/**
 * @file custom_agg.cpp
 * @brief Implements Custom Aggregation query from https://dl.acm.org/doi/10.1145/3448016.3452808
 * @date 2024-10-20
 *
 * Equivalent SQL:
 *   SELECT class_, SUM(cost * (1- coinsurance))
 *   FROM T1, T2, T3
 *   WHERE T1.person=T2.person AND T2.disease=T3.disease
 *   GROUP BY class_
 */

#include "orq.h"
#include "secrecy_dbgen.h"

// #define QUERY_PROFILE

// #define PRINT_TABLES

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using namespace orq::debug;
using namespace orq::service;
using namespace orq::operators;
using namespace std::chrono;
using namespace orq::aggregators;
using namespace orq::benchmarking;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using T = int64_t;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    // Setup SQLite DB for output validation
    sqlite3* sqlite_db = nullptr;
#ifndef QUERY_PROFILE
    int err = sqlite3_open(NULL, &sqlite_db);
    if (err) {
        throw std::runtime_error(sqlite3_errmsg(sqlite_db));
    } else {
        single_cout("SQLite DB created");
    }
#endif

    // Query DB setup
    auto db = SecrecyDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    single_cout("SF " << db.scaleFactor);

    auto T1 = db.getSecrecyT1Table();
    T1.project({"[person]", "coinsurance"});
    auto T2 = db.getSecrecyT2Table();
    auto T3 = db.getSecrecyT3Table();

#ifdef PRINT_TABLES
    single_cout("T1, size " << T1.size());
    print_table(T1.open_with_schema(), pid);

    single_cout("T2, size " << T2.size());
    print_table(T2.open_with_schema(), pid);

    single_cout("T3, size " << T3.size());
    print_table(T3.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // Add a new column 'percentage' to T1 and
    T1.addColumns({"percentage"});
    // Set 'percentage' to 1 - 'coinsurance'
    // NOTE: This is because we do not support floats
    T1["percentage"] = -T1["coinsurance"] + 100;
    T1.project({"[person]", "percentage"});

    // Sum percentages per person
    // After the aggreagtion is applied, T1 contains one valid record per 'person'
    T1.aggregate({"[person]"}, {{"percentage", "percentage", sum<A>}});
    stopwatch::timepoint("Pre-aggregation T1");
    // Join T1 (PK) with T2 on 'person' and copy `percentage` per matched person into T2
    auto T12 = T1.inner_join(T2, {"[person]"}, {{"percentage", "percentage", copy<A>}});

    stopwatch::timepoint("T1 join T2");

    T12.addColumns({"insCost"});
    T12["insCost"] = T12["percentage"] * T12["cost"];

    T12.project({"[disease]", "insCost"});

    // Sum cost per 'disease'
    // After the aggregation is applied, T12 contains one valid record per 'disease'
    T12.aggregate({"[disease]"}, {{"insCost", "insCost", sum<A>}});
    stopwatch::timepoint("Pre-aggregation T3");
    // Join T12 (PK) with T3 on 'disease' and copy `insCost` per matched 'disease' into T3
    auto T123 = T12.inner_join(T3, {"[disease]"}, {{"insCost", "insCost", copy<A>}});

    stopwatch::timepoint("T2 join T3");

    T123.project({"[class_]", "insCost"});

    // Sum 'insCost' per 'class_'
    T123.aggregate({"[class_]"}, {{"insCost", "insCost", sum<A>}});
    T123["insCost"] = T123["insCost"] / 100;

    stopwatch::timepoint("Aggregation T3");

#ifdef QUERY_PROFILE
    T123.finalize();
#endif

    stopwatch::done();
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    // runTime->print_statistics();

    auto T123_out = T123.open_with_schema();
    print_table(T123_out, pid);

    // Get output columns
    auto out_class_ = T123.get_column(T123_out, "[class_]");
    auto out_agg = T123.get_column(T123_out, "insCost");

    if (pid == 0) {
        const char* query = R"sql(
            SELECT class_, SUM(cost * (100 - coinsurance)) / 100
            FROM T1, T2, T3
            WHERE T1.person=T2.person AND T2.disease=T3.disease
            GROUP BY class_
        )sql";

        sqlite3_stmt* stmt;
        auto err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        if (err) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }

        int i;
        for (i = 0; sqlite3_step(stmt) == SQLITE_ROW; i++) {
            auto class_ = sqlite3_column_int64(stmt, 0);
            auto agg = sqlite3_column_int64(stmt, 1);

            std::cout << "class=" << class_ << " agg=" << agg << "\n";
            auto diff = agg - out_agg[i];
            if (diff != 0) {
                std::cout << "  diff=" << diff << "\n";
            }

            ASSERT_SAME(class_, out_class_[i]);
            ASSERT_SAME(agg, out_agg[i]);
        }
        single_cout(i << " rows OK");
    }
#endif
    sqlite3_close(sqlite_db);

    return 0;
}