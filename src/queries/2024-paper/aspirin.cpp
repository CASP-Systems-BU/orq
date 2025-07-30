/**
 * @file aspirin.cpp
 * @brief Implements Aspirin Count query from the Secrecy paper.
 * @date 2024-11-06
 *
 * SQL:
 * SELECT COUNT(DISTINCT pid)
 * FROM diagnosis d JOIN medication m
 * WHERE d.pid = m.pid AND d.time <= m.time
 *   AND d.diag = hd AND m.med = aspirin
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
    auto M = db.getMedicationTable();

#ifdef SECRECY_QUERY
    // Secrecy uses two tables of the same size. Shrink both.
    auto new_size = std::min(D.size(), M.size());
    D.head(new_size);
    M.head(new_size);
#endif

    single_cout("AspirinCount SF " << db.scaleFactor << " (" << D.size() + M.size() << " rows)");

    int Q_DIAG = 3;
    int Q_MED = 5;

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    D.filter(D["[diagnosis]"] == Q_DIAG);
    D.deleteColumns({"[diagnosis]"});

    M.filter(M["[med]"] == Q_MED);
    M.deleteColumns({"[med]"});

    D.renameColumn("[time]", "[d.time]");
    M.renameColumn("[time]", "[m.time]");

    stopwatch::timepoint("Filter");

    // d.time <= m.time
    // compute min d.time and max m.time. if <=, count
    D.aggregate({ENC_TABLE_VALID, "[pid]"}, {{"[d.time]", "[d.time]", min<B>}});

    M.aggregate({ENC_TABLE_VALID, "[pid]"}, {{"[m.time]", "[m.time]", max<B>}});

    stopwatch::timepoint("min/max");

    // Current state: in each table, one row per pid, with either the min time
    // or the max time - so we can do a unique key join.

    auto T = D.inner_join(M, {"[pid]"}, {{"[d.time]", "[d.time]", copy<B>}});
    T.filter(T["[d.time]"] <= T["[m.time]"]);
    T.project({"[pid]"});

    stopwatch::timepoint("join");

    T.addColumns({"Count"});
    T.convert_b2a_bit(ENC_TABLE_VALID, "Count");
    T.prefix_sum("Count");
    T.project({"Count"});
    T.tail(1);

    stopwatch::timepoint("count");
    stopwatch::done();
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    auto result = T.open_with_schema(false);
    print_table(result, pid);

    if (pid == 0) {
        const char *query = R"sql(
            SELECT COUNT(DISTINCT d.pid)
            FROM diagnosis d JOIN medication m  
            WHERE d.pid = m.pid AND d.time <= m.time
                AND d.diag = ? AND m.med = ?
        )sql";

        sqlite3_stmt *stmt;
        auto err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        if (err) {
            std::cerr << "sqlite error: " << sqlite3_errmsg(sqlite_db) << "\n";
            abort();
        }

        sqlite3_bind_int(stmt, 1, Q_DIAG);
        sqlite3_bind_int(stmt, 2, Q_MED);

        auto count_col = T.get_column(result, "Count");

        int i;
        for (i = 0; (err = sqlite3_step(stmt)) == SQLITE_ROW; i++) {
            int count = sqlite3_column_int(stmt, 0);

            assert(count == count_col[0]);
        }

        if (err != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
            abort();
        } else {
            std::cout << i << " rows OK\n";
        }
    }

#endif

    sqlite3_close(sqlite_db);

    MPI_Finalize();
}