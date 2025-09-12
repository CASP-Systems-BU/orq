/**
 * @file distinct_patients.cpp
 * @brief Query to find distinct patients who have been diagnosed with "hd" and prescribed
 * "aspirin".
 * @date 2024-10-22
 *
 * SQL:
 *   SELECT
 *       DISTINCT pid
 *   FROM
 *       demographic de,
 *       diagnosis di,
 *       medication m
 *   WHERE
 *       di.diag = "hd"
 *       AND m.med = "aspirin"
 *       AND di.code = m.code
 *       AND de.pid = m.pid
 *
 * SCHEMA:
 *   - demographic: [pid]
 *   - diagnosis: [code, diag]
 *   - medication: [pid, code, med]
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
#define DEMOGRAPHIC_MULTIPLIER (500 * 1000)
#define DIAGNOSIS_MULTIPLIER (4000 * 1000)
#define MEDICATION_MULTIPLIER (500 * 1000)

#define CODE_TYPES 500
#define DIAG_TYPES 500
#define MED_TYPES 50

size_t demographicSize(const double scaleFactor) {
    return std::round(scaleFactor * DEMOGRAPHIC_MULTIPLIER);
}
size_t diagnosisSize(const double scaleFactor) {
    return std::round(scaleFactor * DIAGNOSIS_MULTIPLIER);
}
size_t medicationSize(const double scaleFactor) {
    return std::round(scaleFactor * MEDICATION_MULTIPLIER);
}

template <typename T>
EncodedTable<T> getDemographicTable(const double scaleFactor, sqlite3* sqlite_db) {
    auto S = demographicSize(scaleFactor);

    Vector<uT> u_pid(S);
    runTime->populateLocalRandom(u_pid);
    Vector<T> pid(S);
    pid = u_pid % demographicSize(scaleFactor);

    std::vector<std::string> schema = {"[pid]"};
    EncodedTable<T> table = secret_share<T>({pid}, schema);
    table.tableName = "DEMOGRAPHIC";

    // SQLite table setup
    if (sqlite_db != nullptr) {
        int ret;
        sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

        // Create table
        const char* sqlCreate = R"sql(
            CREATE TABLE DEMOGRAPHIC (pid INTEGER);
        )sql";
        ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
        if (ret != SQLITE_OK) {
            single_cout("Create error (Demographic): " << sqlite3_errmsg(sqlite_db))
        }

        // Prepare insert statement
        sqlite3_stmt* stmt = nullptr;
        const char* sqlInsert = R"sql(
            INSERT INTO DEMOGRAPHIC (pid) VALUES (?);
        )sql";
        sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

        // Insert data
        for (size_t i = 0; i < S; ++i) {
            sqlite3_bind_int(stmt, 1, pid[i]);

            ret = sqlite3_step(stmt);
            if (ret != SQLITE_DONE) {
                single_cout("Insert error (Demographic): " << sqlite3_errmsg(sqlite_db))
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);

        ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
        if (ret != SQLITE_OK) {
            single_cout("Commit error (Demographic): " << sqlite3_errmsg(sqlite_db))
        }
    }

    return table;
}

template <typename T>
EncodedTable<T> getDiagnosisTable(const double scaleFactor, sqlite3* sqlite_db) {
    auto S = diagnosisSize(scaleFactor);

    Vector<uT> u_code(S);
    runTime->populateLocalRandom(u_code);
    Vector<T> code(S);
    code = u_code % CODE_TYPES;

    Vector<uT> u_diag(S);
    runTime->populateLocalRandom(u_diag);
    Vector<T> diag(S);
    diag = u_diag % DIAG_TYPES;

    std::vector<std::string> schema = {"[code]", "[diag]"};
    EncodedTable<T> table = secret_share<T>({code, diag}, schema);
    table.tableName = "DIAGNOSIS";

    // SQLite table setup
    if (sqlite_db != nullptr) {
        int ret;
        sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

        // Create table
        const char* sqlCreate = R"sql(
            CREATE TABLE DIAGNOSIS (code INTEGER, diag INTEGER);
        )sql";
        ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
        if (ret != SQLITE_OK) {
            single_cout("Create error (Diagnosis): " << sqlite3_errmsg(sqlite_db))
        }

        // Prepare insert statement
        sqlite3_stmt* stmt = nullptr;
        const char* sqlInsert = R"sql(
            INSERT INTO DIAGNOSIS (code, diag) VALUES (?, ?);
        )sql";
        sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

        // Insert data
        for (size_t i = 0; i < S; ++i) {
            sqlite3_bind_int(stmt, 1, code[i]);
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

template <typename T>
EncodedTable<T> getMedicationTable(const double scaleFactor, sqlite3* sqlite_db) {
    auto S = medicationSize(scaleFactor);

    Vector<uT> u_pid(S);
    runTime->populateLocalRandom(u_pid);
    Vector<T> pid(S);
    pid = u_pid % medicationSize(scaleFactor);

    Vector<uT> u_code(S);
    runTime->populateLocalRandom(u_code);
    Vector<T> code(S);
    code = u_code % CODE_TYPES;

    Vector<uT> u_med(S);
    runTime->populateLocalRandom(u_med);
    Vector<T> med(S);
    med = u_med % MED_TYPES;

    std::vector<std::string> schema = {"[pid]", "[code]", "[med]"};
    EncodedTable<T> table = secret_share<T>({pid, code, med}, schema);
    table.tableName = "MEDICATION";

    // SQLite table setup
    if (sqlite_db != nullptr) {
        int ret;
        sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

        // Create table
        const char* sqlCreate = R"sql(
            CREATE TABLE MEDICATION (pid INTEGER, code INTEGER, med INTEGER);
        )sql";
        ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
        if (ret != SQLITE_OK) {
            single_cout("Create error (Medication): " << sqlite3_errmsg(sqlite_db))
        }

        // Prepare insert statement
        sqlite3_stmt* stmt = nullptr;
        const char* sqlInsert = R"sql(
            INSERT INTO MEDICATION (pid, code, med) VALUES (?, ?, ?);
        )sql";
        sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

        // Insert data
        for (size_t i = 0; i < S; ++i) {
            sqlite3_bind_int(stmt, 1, pid[i]);
            sqlite3_bind_int(stmt, 2, code[i]);
            sqlite3_bind_int(stmt, 3, med[i]);

            ret = sqlite3_step(stmt);
            if (ret != SQLITE_DONE) {
                single_cout("Insert error (Medication): " << sqlite3_errmsg(sqlite_db))
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);

        ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
        if (ret != SQLITE_OK) {
            single_cout("Commit error (Medication): " << sqlite3_errmsg(sqlite_db))
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

    // Query parameters
    const int DIAG = 0;  // "hd"
    const int MED = 0;   // "aspirin"

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

    auto Demographic = getDemographicTable<T>(sf, sqlite_db);
    auto Diagnosis = getDiagnosisTable<T>(sf, sqlite_db);
    auto Medication = getMedicationTable<T>(sf, sqlite_db);

#ifdef PRINT_TABLES
    print_table(Demographic.open_with_schema(), pid);
    print_table(Diagnosis.open_with_schema(), pid);
    print_table(Medication.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // [SQL] di.diag = "hd"
    Diagnosis.filter(Diagnosis["[diag]"] == DIAG);
    Diagnosis.project({"[code]"});

    // [SQL] m.med = "aspirin"
    Medication.filter(Medication["[med]"] == MED);
    Medication.project({"[code]", "[pid]"});

    stopwatch::timepoint("Filters");

    // [SQL] di.code = m.code
    auto MedDiagnosis = Diagnosis.inner_join(Medication, {"[code]"});
    MedDiagnosis.project({"[pid]"});

    stopwatch::timepoint("Med-Diagnosis join");

    // [SQL] de.pid = m.pid
    auto DemographicJoin = MedDiagnosis.inner_join(Demographic, {"[pid]"}, {});

    stopwatch::timepoint("Demographic join");

    // [SQL] SELECT DISTINCT pid
    DemographicJoin.distinct({"[pid]"});

    stopwatch::timepoint("Distinct pids");

#ifdef QUERY_PROFILE
    DemographicJoin.finalize();
#endif

    stopwatch::done();
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE
    auto resultOpened = DemographicJoin.open_with_schema();
    auto pid_col = DemographicJoin.get_column(resultOpened, "[pid]");

    if (pid == 0) {
        // Correctness check
        // Note: Extra "order by" to get a consistent order for the correctness check
        int ret;
        const char* query = R"sql(
            select
                distinct de.pid
            from
                demographic de,
                diagnosis di,
                medication m
            where
                di.diag = ?
                AND m.med = ?
                AND di.code = m.code
                AND de.pid = m.pid
            order by
                de.pid
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, DIAG);
        sqlite3_bind_int(stmt, 2, MED);

        int i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            int sqlPid = sqlite3_column_int(stmt, 0);
            // single_cout("PID: " << sqlPid << " | " << pid_col[i]);
            ASSERT_SAME(sqlPid, pid_col[i]);
            i++;
        }

        if (ret != SQLITE_DONE) {
            single_cout("Error executing statement: " << sqlite3_errmsg(sqlite_db));
        }
        sqlite3_finalize(stmt);

        single_cout("Calculated result size: " << pid_col.size());
        single_cout("SQL result size: " << i << std::endl);
        ASSERT_SAME(i, pid_col.size());
    }
#endif
    sqlite3_close(sqlite_db);
    return 0;
}
