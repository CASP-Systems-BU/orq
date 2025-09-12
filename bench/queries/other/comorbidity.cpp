/**
 * @file comorbidity.cpp
 * @brief Implements the Comorbidity Query from the Secrecy paper
 * @date 2024-10-21
 *
 * SQL:
 *   SELECT
 *       diag, COUNT(*) cnt
 *   FROM
 *       diagnosis
 *   WHERE
 *       pid IN cohort
 *   GROUP BY
 *       diag
 *   ORDER BY
 *       cnt DESC
 *   LIMIT 10
 *
 * SCHEMA:
 *   - diagnosis: [pid, diag]
 *   - cohort: [pid]
 *
 */

#include <sqlite3.h>
#include <sys/time.h>

#include "orq.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::operators;
using namespace std::chrono;
using namespace orq::aggregators;
using namespace orq::benchmarking;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using sec = duration<float, seconds::period>;

// #define QUERY_PROFILE

// #define PRINT_TABLES

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using T = int64_t;
using uT = std::make_unsigned_t<T>;

// Setting these up to be roughly similar in size to TPCH, SF 1 -> ~5M rows
// Update later based on dataset if required
#define COHORT_MULTIPLIER (500 * 1000)
#define DIAGNOSIS_MULTIPLIER (COHORT_MULTIPLIER * 10)
#define DIAGNOSIS_TYPE_COUNT (1000)

size_t cohortSize(const double scaleFactor) { return std::round(scaleFactor * COHORT_MULTIPLIER); }
size_t diagnosisSize(const double scaleFactor) {
    return std::round(scaleFactor * DIAGNOSIS_MULTIPLIER);
}

template <typename T>
EncodedTable<T> getCohortTable(const double scaleFactor, sqlite3* sqlite_db) {
    auto S = cohortSize(scaleFactor);

    Vector<T> pid(S);
    std::iota(pid.begin(), pid.end(), 0);

    std::vector<std::string> schema = {"[pid]"};
    EncodedTable<T> table = secret_share<T>({pid}, schema);
    table.tableName = "COHORT";

    // SQLite table setup
    if (sqlite_db != nullptr) {
        int ret;
        sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

        // Create table
        const char* sqlCreate = R"sql(
            CREATE TABLE COHORT (pid INTEGER);
        )sql";
        ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
        if (ret != SQLITE_OK) {
            single_cout("Create error (Cohort): " << sqlite3_errmsg(sqlite_db))
        }

        // Prepare insert statement
        sqlite3_stmt* stmt = nullptr;
        const char* sqlInsert = R"sql(
            INSERT INTO COHORT (pid) VALUES (?);
        )sql";
        sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

        // Insert data
        for (size_t i = 0; i < S; ++i) {
            sqlite3_bind_int(stmt, 1, pid[i]);

            ret = sqlite3_step(stmt);
            if (ret != SQLITE_DONE) {
                single_cout("Insert error (Cohort): " << sqlite3_errmsg(sqlite_db))
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);

        ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
        if (ret != SQLITE_OK) {
            single_cout("Commit error (Cohort): " << sqlite3_errmsg(sqlite_db))
        }
    }

    return table;
}

template <typename T>
EncodedTable<T> getDiagnosisTable(const double scaleFactor, sqlite3* sqlite_db) {
    auto S = diagnosisSize(scaleFactor);

    Vector<uT> u_pid(S);
    runTime->populateLocalRandom(u_pid);
    Vector<T> pid(S);
    // Arbitrarily setting max pid to 2 * cohortSize to allow for pids in Diagnosis
    // that are not in Cohort (For "WHERE pid IN cohort" to be meaningful)
    pid = u_pid % (cohortSize(scaleFactor) * 2);

    Vector<uT> u_diag(S);
    runTime->populateLocalRandom(u_diag);
    Vector<T> diag(S);
    diag = u_diag % DIAGNOSIS_TYPE_COUNT;

    std::vector<std::string> schema = {"[pid]", "[diag]"};
    EncodedTable<T> table = secret_share<T>({pid, diag}, schema);
    table.tableName = "DIAGNOSIS";

    // SQLite table setup
    if (sqlite_db != nullptr) {
        int ret;
        sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

        // Create table
        const char* sqlCreate = R"sql(
            CREATE TABLE DIAGNOSIS (pid INTEGER, diag INTEGER);
        )sql";
        ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
        if (ret != SQLITE_OK) {
            single_cout("Create error (Diagnosis): " << sqlite3_errmsg(sqlite_db))
        }

        // Prepare insert statement
        sqlite3_stmt* stmt = nullptr;
        const char* sqlInsert = R"sql(
            INSERT INTO DIAGNOSIS (pid, diag) VALUES (?, ?);
        )sql";
        sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

        // Insert data
        for (size_t i = 0; i < S; ++i) {
            sqlite3_bind_int(stmt, 1, pid[i]);
            sqlite3_bind_int(stmt, 2, diag[i]);

            ret = sqlite3_step(stmt);
            if (ret != SQLITE_DONE) {
                single_cout("Insert error (Diagnosis): " << sqlite3_errmsg(sqlite_db))
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);

        ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
        if (ret != SQLITE_OK) {
            single_cout("Commit error (Diagnosis): " << sqlite3_errmsg(sqlite_db))
        }
    }

    return table;
}

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    // Setup SQL DB
    sqlite3* sqlite_db = nullptr;

#ifndef QUERY_PROFILE
    if (pid == 0) {
        int err = sqlite3_open(NULL, &sqlite_db);  // NULL -> Create in-memory database
        if (err) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        } else {
            single_cout("SQLite DB created");
        }
    }
#endif

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    auto Cohort = getCohortTable<T>(sf, sqlite_db);
    auto Diagnosis = getDiagnosisTable<T>(sf, sqlite_db);

#ifdef PRINT_TABLES
    print_table(Cohort.open_with_schema(), pid);
    print_table(Diagnosis.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // [SQL] WHERE pid IN cohort
    auto FilteredDiagnosis = Diagnosis.semi_join(Cohort, {"[pid]"});

    stopwatch::timepoint("Join");

    // [SQL] GROUP BY diag, COUNT(*)
    FilteredDiagnosis.addColumns({"DiagCount"}, FilteredDiagnosis.size());
    FilteredDiagnosis.aggregate({"[diag]"}, {{"DiagCount", "DiagCount", count<A>}});

    stopwatch::timepoint("Aggregation");

    FilteredDiagnosis.addColumns({"[DiagCount]"}, FilteredDiagnosis.size());
    FilteredDiagnosis.convert_a2b("DiagCount", "[DiagCount]");

    // [SQL] ORDER BY cnt DESC
    FilteredDiagnosis.sort({ENC_TABLE_VALID, "[DiagCount]"}, DESC);

    stopwatch::timepoint("Sort");

#ifdef QUERY_PROFILE
    FilteredDiagnosis.finalize();
#endif

    stopwatch::done();
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE
    // Extra sort on "diag" outside the benchmarking scope to get a
    // consistent order for the correctness check
    FilteredDiagnosis.sort({ENC_TABLE_VALID, "[DiagCount]", "[diag]"}, DESC);
    FilteredDiagnosis.head(10);

    auto resultOpened = FilteredDiagnosis.open_with_schema();
    auto diag_col = FilteredDiagnosis.get_column(resultOpened, "[diag]");
    auto diagCount_col = FilteredDiagnosis.get_column(resultOpened, "[DiagCount]");

    if (pid == 0) {
        // Correctness check
        int ret;
        const char* query = R"sql(
            select
                diag, COUNT(*) cnt
            from
                diagnosis
            where
                pid IN cohort
            group by
                diag
            order by
                cnt DESC,
                diag DESC
            limit 10
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        int i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            int sqlDiag = sqlite3_column_int(stmt, 0);
            int sqlDiagCount = sqlite3_column_int(stmt, 1);

            //   single_cout("Diag: " << sqlDiag << " -> SQL: " << sqlDiagCount
            //                        << " | Query: " << diagCount_col[i]);
            ASSERT_SAME(sqlDiag, diag_col[i]);
            ASSERT_SAME(sqlDiagCount, diagCount_col[i]);

            i++;
        }

        if (ret != SQLITE_DONE) {
            single_cout("Error executing statement: " << sqlite3_errmsg(sqlite_db));
        }
        sqlite3_finalize(stmt);

        single_cout("Calculated result size: " << diag_col.size());
        single_cout("SQL result size: " << i << std::endl);
        ASSERT_SAME(i, diag_col.size());
    }
#endif
    sqlite3_close(sqlite_db);
    return 0;
}
