/**
 * @file pwd-reuse.cpp
 * @brief Implements Password Reuse query from the Secrecy (2023) paper
 * @date 2024-11-08
 *
 * SQL:
 *   SELECT ID
 *   FROM R
 *   GROUP BY ID, PWD
 *   HAVING COUNT(*)>1
 *
 */

#include "../../../include/secrecy.h"
#include "secrecy_dbgen.h"

// #define QUERY_PROFILE

// #define PRINT_TABLES

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using T = int64_t;

int main(int argc, char **argv) {
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    sqlite3 *sqlite_db = nullptr;
#ifndef QUERY_PROFILE
    if (pid == 0) {
        if (sqlite3_open(NULL, &sqlite_db) != 0) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
    }
#endif

    auto db = SecrecyDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    auto R = db.getPasswordTable();
    single_cout("Password SF " << db.scaleFactor << " (" << R.size() << " rows)");

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // no need for valid bit, because this is a fresh table
    R.addColumns({"Count"});
    R.aggregate({"[ID]", "[Password]"}, {
        {"Count", "Count", count<A>}
    });

    stopwatch::timepoint("aggregate");

    R.addColumns({"[Count]"});
    R.convert_a2b("Count", "[Count]");
    R.filter(R["[Count]"] > 1);
    R.project({"[ID]"});

    stopwatch::timepoint("filter");

#ifdef QUERY_PROFILE
    R.finalize();
#endif

    stopwatch::done();
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE
    auto result = R.open_with_schema();
    print_table(result, pid);

    if (pid == 0) {
        const char *query = R"sql(
            SELECT ID
            FROM Password
            GROUP BY ID, PWD
            HAVING COUNT(*)>1
        )sql";

        sqlite3_stmt *stmt;
        auto err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        if (err) {
            std::cerr << "sqlite error: " << sqlite3_errmsg(sqlite_db) << "\n";
            abort();
        }

        int i;
        for (i = 0; (err = sqlite3_step(stmt)) == SQLITE_ROW; i++) {
            int id = sqlite3_column_int(stmt, 0);

            assert(id == R.get_column(result, "[ID]")[i]);
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

    MPI_Finalize();
}