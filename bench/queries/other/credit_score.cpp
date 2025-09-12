/**
 * @file credit_score.cpp
 * @brief Credit Score query (orig. from Secrecy Paper), for Demo Paper
 * @date 2024-01-03
 *
 * Equivalent SQL:
 *
 * SELECT S.ID FROM (
 *   SELECT ID, MIN(CS) as cs1, MAX(CS) as cs2
 *   FROM R
 *   WHERE R.year=YEAR
 *   GROUP-BY ID ) as S
 * WHERE S.cs2 - S.cs1 > THRESHOLD
 *
 */

#include <sqlite3.h>

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

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#define TARGET_YEAR 2023
#define THRESHOLD 150

using T = int64_t;

int main(int argc, char **argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    sqlite3 *sqlite_db = nullptr;
#ifndef QUERY_PROFILE
    if (pID == 0) {
        if (sqlite3_open(NULL, &sqlite_db) != 0) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
    }
#endif

    auto db = SecrecyDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    auto R = db.getCreditScoreTable();
    single_cout("CreditScore SF " << db.scaleFactor << " (" << R.size() << ")");

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // Apply year filter
    R.filter(R["[YEAR]"] == TARGET_YEAR);

    stopwatch::timepoint("Filter");

    R.addColumns({"[CS_MIN]", "[CS_MAX]"});

    // Compute min, max
    R.aggregate({"[UID]"}, {
                               {"[CS]", "[CS_MIN]", min<B>},
                               {"[CS]", "[CS_MAX]", max<B>},
                           });

    stopwatch::timepoint("Aggregation");

    // Apply criteria
    // Note: *binary* subtraction; uses RCA with carry. More efficient than
    // conversion.
    R.filter(R["[CS_MAX]"] - R["[CS_MIN]"] > THRESHOLD);

    stopwatch::timepoint("Threshold");

    // Zero out duplicates and information we don't want disclosed
    R.project({"[UID]"});
    // R.finalize();

#ifdef QUERY_PROFILE
    R.finalize();
#endif

    stopwatch::done();
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    auto result = R.open_with_schema();
    // print_table(result, pID);

    if (pID == 0) {
        const char *query = R"sql(
        SELECT S.UID FROM (
            SELECT UID, MIN(CS) as CS1, MAX(CS) as CS2
            FROM CREDIT
            WHERE year=?
            GROUP BY UID ) as S
        WHERE S.CS2 - S.CS1 > ?
        )sql";

        sqlite3_stmt *stmt;
        auto err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        if (err) {
            std::cerr << "sqlite error: " << sqlite3_errmsg(sqlite_db) << "\n";
            abort();
        }

        sqlite3_bind_int(stmt, 1, TARGET_YEAR);
        sqlite3_bind_int(stmt, 2, THRESHOLD);

        int i;
        for (i = 0; (err = sqlite3_step(stmt)) == SQLITE_ROW; i++) {
            int uid = sqlite3_column_int(stmt, 0);

            // std::cout << "uid " << uid << "\n";
            assert(uid == R.get_column(result, "[UID]")[i]);
        }

        if (err != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
            abort();
        } else {
            assert(i == result.first[0].size());
            std::cout << i << " rows OK\n";
        }
    }
#endif
    sqlite3_close(sqlite_db);
    return 0;
}