/**
 * @file distinct_patients.cpp
 * @brief Query to find distinct patients who have been diagnosed with heart disease (`hd`) and
 * prescribed `aspirin`.
 * @date 2024-10-22
 *
 * Equivalent SQL:
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
 *   - diagnosis:   [code, diag]
 *   - medication:  [pid, code, med]
 *
 * This query has been slightly modified from the version presented in the ORQ paper for improved
 * clarity.
 *
 */

// To run correctness tests
#include <sqlite3.h>

// Include the ORQ library
#include "orq.h"

// Macro to tell ORQ to run whichever protocol we compiled with
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// Setting these up to be roughly similar in size to TPCH, SF 1 -> ~5M rows
// Update later based on dataset if required
#define DEMOGRAPHIC_MULTIPLIER (500 * 1000)
#define DIAGNOSIS_MULTIPLIER (4000 * 1000)
#define MEDICATION_MULTIPLIER (500 * 1000)

#define CODE_TYPES 500
#define DIAG_TYPES 500
#define MED_TYPES 50

// These functions will generate a table (of the appropriate size) with random data and load them
// into SQLite.
EncodedTable<int> getDemographicTable(const double, sqlite3*);
EncodedTable<int> getDiagnosisTable(const double, sqlite3*);
EncodedTable<int> getMedicationTable(const double, sqlite3*);

/**
 * @brief Main function. ALL parties will execute this code in (approximate) lock step.
 */
int main(int argc, char** argv) {
    // Initialize the ORQ framework. Specifically, sets up:
    //   - inter-party communication backend (either MPI or NoCopy)
    //   - Constructs the RunTime object
    //   - Sets up worker threads
    //   - Sets up randomness generation (shared-key PRGs, etc.)
    //
    // See include/backend/common/setup.h for more.
    orq_init(argc, argv);

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    // single_count is a helper macro which only runs outputs to Party 0's std::cout.
    // Since all parties run the same program, regular std::cout calls will all appear at the same
    // time and clobber each other.
    single_cout("Distinct Patients, SF " << sf);

    // Query parameters. We assume an arbitrary enumeration.
    const int DIAG = 392;  // "hd"
    const int MED = 16;    // "aspirin"

    // Setup SQL DB. Only Party 0 does this (and only Party 0 will run the correctness check)
    sqlite3* sqlite_db = nullptr;
    if (runTime->getPartyID() == 0) {
        int err = sqlite3_open(NULL, &sqlite_db);  // NULL -> Create in-memory database
        if (err) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        } else {
            std::cout << "SQLite DB created\n";
        }
    }

    // Generate tables with random data. In reality, multiple data owners would each contribute a
    // table (in this example, maybe an insurance company has one table, and a hospital another).
    // Alternatively, we could imagine each data owner having a disjoint set of rows, and all tables
    // being joined under secret sharing into one larger table. (This scenario would occur if, say,
    // many hospitals wanted to run a large analysis together but were unable or unwilling to share
    // plaintext data.)
    auto Demographic = getDemographicTable(sf, sqlite_db);
    auto Diagnosis = getDiagnosisTable(sf, sqlite_db);
    auto Medication = getMedicationTable(sf, sqlite_db);

    // Print out the table sizes
    single_cout("Demographic: " << Demographic.size() << " rows x "
                                << Demographic.getSchema().size() << " cols => "
                                << Demographic.size() * Demographic.getSchema().size() *
                                       sizeof(int) / 1e6
                                << " MB");

    single_cout("Diagnosis:   " << Diagnosis.size() << " rows x " << Diagnosis.getSchema().size()
                                << " cols => "
                                << Diagnosis.size() * Diagnosis.getSchema().size() * sizeof(int) /
                                       1e6
                                << " MB");

    single_cout("Medication:  " << Medication.size() << " row x " << Medication.getSchema().size()
                                << " cols => "
                                << Medication.size() * Medication.getSchema().size() * sizeof(int) /
                                       1e6
                                << " MB");

    // If we're running a tiny problem instance, print the tables for debugging
    if (Demographic.size() < 16) {
        // NOTE: open() here is NOT secure. This is to get a sense of what the tables might look
        // like. In a real execution, we would never open a table until the computation is complete.
        print_table(Demographic.open_with_schema(), runTime->getPartyID());
        print_table(Diagnosis.open_with_schema(), runTime->getPartyID());
        print_table(Medication.open_with_schema(), runTime->getPartyID());
    }

    // The stopwatch namespace provides timing utilities.
    stopwatch::timepoint("Start");

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

    stopwatch::timepoint("Med-Diag join");

    // [SQL] de.pid = m.pid
    auto DemographicJoin = MedDiagnosis.inner_join(Demographic, {"[pid]"});

    stopwatch::timepoint("Demog. join");

    // [SQL] SELECT DISTINCT pid
    DemographicJoin.distinct({"[pid]"});

    stopwatch::timepoint("Distinct pids");

    // Oblivious execution does not guarantee that the opened table won't reveal anything about the
    // query inputs. Thus, the finalize command shuffles a table using ORQ's oblivious shuffling
    // facilities, and also obliviously masks out any invalid rows to prevent leakage.
    //
    // However, we will skip shuffling here (paramater `false`) only so that correctness tests will
    // be deterministic. Note that this technically is not secure, since it leaks the order of
    // intermediate values in the query.
    DemographicJoin.finalize(false);

    stopwatch::done();

    // When run under the single-party debug protocol, output the number of operations performed
    runTime->print_statistics();

    // Show the network utilization of this execution
    runTime->print_communicator_statistics();

    // Open the final table,
    auto resultOpened = DemographicJoin.open_with_schema();
    // and extract the patient ID column. We'll check this against the SQL result
    auto pid_col = DemographicJoin.get_column(resultOpened, "[pid]");

    // In reality, we probably wouldn't want to open to a computing party. Instead, a separate
    // entity (such as a data analyst) would receive secret shares from each of the computing
    // parties and reconstruct the shares locally. ORQ easily supports such a setup via its secret
    // share-export functionalities.

    // Only Party 0 runs the correctness check (it generated the random data, so is the only one who
    // knows the correct answer).
    if (runTime->getPartyID() == 0) {
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
            // Uncomment the below to view the actual output!
            // single_cout("SQL: " << sqlPid << " | MPC: " << pid_col[i]);
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

    sqlite3_close(sqlite_db);
    return 0;
}

size_t demographicSize(const double scaleFactor) {
    return std::round(scaleFactor * DEMOGRAPHIC_MULTIPLIER);
}
size_t diagnosisSize(const double scaleFactor) {
    return std::round(scaleFactor * DIAGNOSIS_MULTIPLIER);
}
size_t medicationSize(const double scaleFactor) {
    return std::round(scaleFactor * MEDICATION_MULTIPLIER);
}

// Implementation of the data-generation functions. At a high level, generates a random vector in
// the appropriate range, and secret shares it (we assume P0 is the data owner, for simplicity, but
// any data owner configurations are supported). Then, P0 also inserts the (plaintext) data into its
// local SQL database for later correctness checks.
EncodedTable<int> getDemographicTable(const double scaleFactor, sqlite3* sqlite_db) {
    auto S = demographicSize(scaleFactor);

    Vector<int> pid(S);
    runTime->populateLocalRandom(pid);
    pid %= S;

    std::vector<std::string> schema = {"[pid]"};
    EncodedTable<int> table = secret_share<int>({pid}, schema);
    table.tableName = "DEMOGRAPHIC";

    // SQLite table setup
    if (sqlite_db == nullptr) {
        return table;
    }

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

    return table;
}

EncodedTable<int> getDiagnosisTable(const double scaleFactor, sqlite3* sqlite_db) {
    auto S = diagnosisSize(scaleFactor);

    Vector<int> code(S);
    runTime->populateLocalRandom(code);
    code %= CODE_TYPES;

    Vector<int> diag(S);
    runTime->populateLocalRandom(diag);
    diag %= DIAG_TYPES;

    std::vector<std::string> schema = {"[code]", "[diag]"};
    EncodedTable<int> table = secret_share<int>({code, diag}, schema);
    table.tableName = "DIAGNOSIS";

    // SQLite table setup
    if (sqlite_db == nullptr) {
        return table;
    }

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

    return table;
}

EncodedTable<int> getMedicationTable(const double scaleFactor, sqlite3* sqlite_db) {
    auto S = medicationSize(scaleFactor);

    Vector<int> pid(S);
    runTime->populateLocalRandom(pid);
    pid %= S;

    Vector<int> code(S);
    runTime->populateLocalRandom(code);
    code %= CODE_TYPES;

    Vector<int> med(S);
    runTime->populateLocalRandom(med);
    med %= MED_TYPES;

    std::vector<std::string> schema = {"[pid]", "[code]", "[med]"};
    EncodedTable<int> table = secret_share<int>({pid, code, med}, schema);
    table.tableName = "MEDICATION";

    // SQLite table setup
    if (sqlite_db == nullptr) {
        return table;
    }

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

    return table;
}