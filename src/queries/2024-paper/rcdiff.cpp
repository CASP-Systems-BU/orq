/**
 * @file rcdiff.cpp
 * @brief Implements Recurrent C. Diff query from the Secrecy paper.
 * @date 2024-11-07
 *
 * SQL:
 * WITH rcd AS (
 *      SELECT pid, time, row_no
 *      FROM diagnosis
 *      WHERE diag=cdiff)
 *    SELECT DISTINCT pid
 *    FROM rcd r1 JOIN rcd r2
 *    WHERE r1.pid = r2.pid
 *       AND r2.time - r1.time >= 15 DAYS
 *       AND r2.time - r1.time <= 56 DAYS
 *       AND r2.row_no = r1.row_no + 1
 *
 * Note: row_no is not present in our schema; it is an implicit row generated
 * after sorting.
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

    auto D = db.getDiagnosisTable();
    single_cout("RCDiff SF " << db.scaleFactor << " (" << D.size() << " rows)");

    int Q_DIAG = 8;
    int T_MIN = 15;
    int T_MAX = 56;

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    D.filter(D["[diagnosis]"] == Q_DIAG);
    D.deleteColumns({"[diagnosis]"});

    D.sort({ENC_TABLE_VALID, "[pid]", "[time]"});

    stopwatch::timepoint("sort");

    D.addColumns({"[time2]", "[pid2]", "[delta]"});

    // match up adjacent rows
    // copy the length-(n-1) suffix of [time] and [pid] into the length-(n-1)
    // prefix of [time2] and [pid2], respectively.
    // then row i of [time2] and [pid2] will equal row (i+1) of [time] and [pid]
    // this implements `r2.row_no = r1.row_no + 1` implicitly.
    D.asBSharedVector("[time2]").slice(0, D.size() - 1) = D.asBSharedVector("[time]").slice(1);

    D.asBSharedVector("[pid2]").slice(0, D.size() - 1) = D.asBSharedVector("[pid]").slice(1);

    // Don't bother with conversion - use adder circuit
    D["[delta]"] = D["[time2]"] - D["[time]"];
    D.filter((D["[delta]"] >= T_MIN) & (D["[delta]"] <= T_MAX) & (D["[pid2]"] == D["[pid]"]));
    D.deleteColumns({"[pid2]", "[time]", "[time2]", "[delta]"});
    D.distinct({"[pid]"});

    stopwatch::timepoint("filter+distinct");
    stopwatch::done();
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    auto result = D.open_with_schema();
    print_table(result, pid);

    if (pid == 0) {
        const char *query = R"sql(
          WITH rcd AS (
            SELECT pid, time, ROW_NUMBER() OVER(ORDER BY pid, time) as row_no
            FROM diagnosis
            WHERE diag=?)
        SELECT DISTINCT r1.pid
            FROM rcd r1 JOIN rcd r2 
            WHERE r1.pid = r2.pid 
                AND r2.time - r1.time >= ?
                AND r2.time - r1.time <= ?
                AND r2.row_no = r1.row_no + 1
            ORDER BY r1.pid
        )sql";

        sqlite3_stmt *stmt;
        auto err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        if (err) {
            std::cerr << "sqlite error: " << sqlite3_errmsg(sqlite_db) << "\n";
            abort();
        }

        sqlite3_bind_int(stmt, 1, Q_DIAG);
        sqlite3_bind_int(stmt, 2, T_MIN);
        sqlite3_bind_int(stmt, 3, T_MAX);

        int i;
        for (i = 0; (err = sqlite3_step(stmt)) == SQLITE_ROW; i++) {
            int pid = sqlite3_column_int(stmt, 0);

            // std::cout << "pid " << pid << "\n";
            assert(pid == D.get_column(result, "[pid]")[i]);
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