/**
 * @file q2.cpp
 * @brief Implements Q2 from Secrecy Paper
 * @date 2024-10-20
 *
 * Equivalent SQL:
 *  SELECT R.ak, COUNT(*) as cnt
 *  FROM R, S
 *  WHERE R.id = S.id
 *  GROUP BY R.ak
 */

#include <sys/time.h>

#include "orq.h"
#include "profiling/stopwatch.h"
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

    auto R = db.getSecrecyRTable();
    auto S = db.getSecrecySTable();

#ifdef PRINT_TABLES
    single_cout("R, size " << R.size());
    print_table(R.open_with_schema(), pid);

    single_cout("S, size " << S.size());
    print_table(S.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // Sort S on id
    // Add a new column S[CNT] and initialize it to 1
    // SUM S[CNT] per id using aggregate
    // Now S contains one row per id along with the total number of occurrences of this id in the
    // original S
    S.addColumns({"CNT"}, S.size());
    S.aggregate({"[id]"}, {{"CNT", "CNT", count<A>}});

    // Semi-join S with R and copy S[CNT] to R for each match.
    // Now R contains partial aggregates
    auto RJ = S.inner_join(R, {"[id]"}, {{"CNT", "CNT", copy<A>}});
    RJ.project({"[ak]", "CNT"});

    // Sort R on ak
    // SUM R[CNT] per ak using aggregation to compute the final aggregation per ak
    // Open (R.ak, R[CNT])
    RJ.aggregate({"[ak]"}, {{"CNT", "CNT", sum<A>}});

    stopwatch::done();
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    runTime->print_statistics();

    auto RJO = RJ.open_with_schema();
    print_table(RJO, pid);

    // TODO: get output columns
    auto out_ak = RJ.get_column(RJO, "[ak]");
    auto out_cnt = RJ.get_column(RJO, "CNT");

    if (pid == 0) {
        const char* query = R"sql(
            SELECT SecrecyR.ak, COUNT(*) as cnt
            FROM SecrecyR, SecrecyS
            WHERE SecrecyR.id = SecrecyS.id 
            GROUP BY SecrecyR.ak
        )sql";

        sqlite3_stmt* stmt;
        auto err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        if (err) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }

        int i;
        for (i = 0; sqlite3_step(stmt) == SQLITE_ROW; i++) {
            auto ak = sqlite3_column_int(stmt, 0);
            auto cnt = sqlite3_column_int(stmt, 1);

            ASSERT_SAME(ak, out_ak[i]);
            ASSERT_SAME(cnt, out_cnt[i]);
        }
        single_cout(i << " rows OK");
    }
#endif
    sqlite3_close(sqlite_db);

    return 0;
}