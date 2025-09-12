#include "../tpch/tpch_dbgen.h"

// per SF - we want SF1 to be ~5M rows.
#define SECRECY_R_SIZE (2000 * 1000)
#define SECRECY_S_SIZE (3000 * 1000)

#define SECRECY_T1_SIZE (2000 * 1000)
#define SECRECY_T2_SIZE (1000 * 1000)
#define SECRECY_T3_SIZE (1000 * 1000)

#define DIAGNOSIS_SIZE (3000 * 1000)
#define MEDICATION_SIZE (2000 * 1000)
#define STANDARD_SIZE (5000 * 1000)

template <typename T = int>
class SecrecyDatabase : public TPCDatabase<T> {
   public:
    SecrecyDatabase(double sf) : TPCDatabase<T>(sf) {}

    SecrecyDatabase(double sf, sqlite3* sqlite_db) : TPCDatabase<T>(sf, sqlite_db) {}

    size_t secrecyRSize() { return std::round(this->scaleFactor * SECRECY_R_SIZE); }

    size_t secrecySSize() { return std::round(this->scaleFactor * SECRECY_S_SIZE); }

    size_t secrecyT1Size() { return std::round(this->scaleFactor * SECRECY_T1_SIZE); }

    size_t secrecyT2Size() { return std::round(this->scaleFactor * SECRECY_T2_SIZE); }

    size_t secrecyT3Size() { return std::round(this->scaleFactor * SECRECY_T3_SIZE); }

    const size_t MAX_TIME = 10000;

    const int N_DISEASE = 100;
    const int N_DISEASE_CLASS = 16;
    const int MAX_COST = 10000;

    EncodedTable<T> getSecrecyRTable() {
        auto S = secrecyRSize();

        auto id = this->randomColumn(S, 0, int(std::sqrt(S)));
        auto ak = this->randomColumn(S, 0, int(std::sqrt(S)));

        std::vector<std::string> schema = {"[id]", "[ak]"};

        EncodedTable<T> table = secret_share<T>({id, ak}, schema);

        if (this->sqlite_db) {
            sqlite3_exec(this->sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE SecrecyR (
                    id INTEGER, ak INTEGER
                );
            )sql";
            ret = sqlite3_exec(this->sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on SecrecyR (Create): " << sqlite3_errmsg(this->sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO SecrecyR (
                    id, ak)
                VALUES (?, ?);
            )sql";
            sqlite3_prepare_v2(this->sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, id[i]);
                sqlite3_bind_int(stmt, 2, ak[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout(
                        "SQL error on SecrecyR (Insert): " << sqlite3_errmsg(this->sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(this->sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on SecrecyR (Commit): " << sqlite3_errmsg(this->sqlite_db))
            }
        } else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "SecrecyR";
        return table;
    }

    EncodedTable<T> getSecrecySTable() {
        auto S = secrecyRSize();

        auto id = this->randomColumn(S, 0, int(std::sqrt(S)));

        std::vector<std::string> schema = {"[id]"};

        EncodedTable<T> table = secret_share<T>({id}, schema);

        if (this->sqlite_db) {
            sqlite3_exec(this->sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE SecrecyS (
                    id INTEGER
                );
            )sql";
            ret = sqlite3_exec(this->sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on SecrecyS (Create): " << sqlite3_errmsg(this->sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO SecrecyS (id)
                VALUES (?);
            )sql";
            sqlite3_prepare_v2(this->sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, id[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout(
                        "SQL error on SecrecyS (Insert): " << sqlite3_errmsg(this->sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(this->sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on SecrecyS (Commit): " << sqlite3_errmsg(this->sqlite_db))
            }
        } else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "SecrecyS";
        return table;
    }

    EncodedTable<T> getDiagnosisTable() {
        size_t S = DIAGNOSIS_SIZE * this->scaleFactor;

        size_t num_patients = S / 2;

        auto pid = this->randomColumn(S, 0, num_patients);
        auto time = this->randomColumn(S, 0, MAX_TIME);
        auto diag = this->randomColumn(S, 0, 10);

        std::vector<std::string> schema = {"[pid]", "[time]", "[diagnosis]"};

        EncodedTable<T> table = secret_share<T>({pid, time, diag}, schema);

        if (this->sqlite_db) {
            sqlite3_exec(this->sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE Diagnosis (
                    pid INTEGER, time INTEGER, diag INTEGER
                );
            )sql";
            ret = sqlite3_exec(this->sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on create: " << sqlite3_errmsg(this->sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO Diagnosis (pid, time, diag)
                VALUES (?, ?, ?);
            )sql";
            sqlite3_prepare_v2(this->sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, pid[i]);
                sqlite3_bind_int(stmt, 2, time[i]);
                sqlite3_bind_int(stmt, 3, diag[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on insert: " << sqlite3_errmsg(this->sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(this->sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Commit: " << sqlite3_errmsg(this->sqlite_db))
            }
        } else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "Diagnosis";
        return table;
    }

    EncodedTable<T> getMedicationTable() {
        size_t S = MEDICATION_SIZE * this->scaleFactor;

        size_t num_patients = S / 2;

        auto pid = this->randomColumn(S, 0, num_patients);
        auto time = this->randomColumn(S, 0, MAX_TIME);
        auto med = this->randomColumn(S, 0, 10);

        std::vector<std::string> schema = {"[pid]", "[time]", "[med]"};

        EncodedTable<T> table = secret_share<T>({pid, time, med}, schema);

        if (this->sqlite_db) {
            sqlite3_exec(this->sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE Medication (
                    pid INTEGER, time INTEGER, med INTEGER
                );
            )sql";
            ret = sqlite3_exec(this->sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on create: " << sqlite3_errmsg(this->sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO Medication (pid, time, med)
                VALUES (?, ?, ?);
            )sql";
            sqlite3_prepare_v2(this->sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, pid[i]);
                sqlite3_bind_int(stmt, 2, time[i]);
                sqlite3_bind_int(stmt, 3, med[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on insert: " << sqlite3_errmsg(this->sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(this->sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Commit: " << sqlite3_errmsg(this->sqlite_db))
            }
        } else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "Medication";
        return table;
    }

    EncodedTable<T> getPasswordTable() {
        size_t S = STANDARD_SIZE * this->scaleFactor;

        size_t num_users = S / 10;
        size_t num_passwd = S / 50;

        auto id = this->randomColumn(S, 0, num_users);
        auto pwd = this->randomColumn(S, 0, num_passwd);

        std::vector<std::string> schema = {"[ID]", "[Password]"};

        EncodedTable<T> table = secret_share<T>({id, pwd}, schema);

        if (this->sqlite_db) {
            sqlite3_exec(this->sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE Password (
                    id INTEGER, pwd INTEGER
                );
            )sql";
            ret = sqlite3_exec(this->sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on create: " << sqlite3_errmsg(this->sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO Password (id, pwd)
                VALUES (?, ?);
            )sql";
            sqlite3_prepare_v2(this->sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, id[i]);
                sqlite3_bind_int(stmt, 2, pwd[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on insert: " << sqlite3_errmsg(this->sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(this->sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Commit: " << sqlite3_errmsg(this->sqlite_db))
            }
        } else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "Password";
        return table;
    }

    EncodedTable<T> getTaxiTable() {
        size_t S = STANDARD_SIZE * this->scaleFactor;

        size_t num_companies = 1000;
        size_t max_fare = 100;
        // we want airport types to be ~5%
        size_t types = 20;

        auto company = this->randomColumn(S, 0, num_companies);

        // bias the distribution a bit
        company = (company * company) % num_companies;

        auto fare = this->randomColumn(S, 0, max_fare);
        auto type = this->randomColumn(S, 0, types);

        std::vector<std::string> schema = {"[Company]", "Fare", "[Type]"};

        EncodedTable<T> table = secret_share<T>({company, fare, type}, schema);

        if (this->sqlite_db) {
            sqlite3_exec(this->sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE TaxiData (
                    company INTEGER, fare INTEGER, TripType INTEGER
                );
            )sql";
            ret = sqlite3_exec(this->sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on create: " << sqlite3_errmsg(this->sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO TaxiData (company, fare, TripType)
                VALUES (?, ?, ?);
            )sql";
            sqlite3_prepare_v2(this->sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, company[i]);
                sqlite3_bind_int(stmt, 2, fare[i]);
                sqlite3_bind_int(stmt, 3, type[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on insert: " << sqlite3_errmsg(this->sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(this->sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Commit: " << sqlite3_errmsg(this->sqlite_db))
            }
        } else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "Taxi";
        return table;
    }

    EncodedTable<T> getSecrecyT1Table() {
        auto T1 = secrecyT1Size();

        auto person = this->randomColumn(T1, 0, int(std::sqrt(T1)));
        auto coinsurance = this->randomColumn(T1, 0, 100);
        auto state = this->randomColumn(T1, 0, 50);

        std::vector<std::string> schema = {"[person]", "coinsurance", "[state]"};

        EncodedTable<T> table = secret_share<T>({person, coinsurance, state}, schema);

        if (this->sqlite_db) {
            sqlite3_exec(this->sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE T1 (
                    person INTEGER, coinsurance INTEGER, state INTEGER
                );
            )sql";
            ret = sqlite3_exec(this->sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on T1 (Create): " << sqlite3_errmsg(this->sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO T1 (
                    person, coinsurance, state)
                VALUES (?, ?, ?);
            )sql";
            sqlite3_prepare_v2(this->sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < T1; ++i) {
                sqlite3_bind_int(stmt, 1, person[i]);
                sqlite3_bind_int(stmt, 2, coinsurance[i]);
                sqlite3_bind_int(stmt, 3, state[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on T1 (Insert): " << sqlite3_errmsg(this->sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(this->sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on T1 (Commit): " << sqlite3_errmsg(this->sqlite_db))
            }
        } else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "T1";
        return table;
    }

    EncodedTable<T> getSecrecyT2Table() {
        auto T2 = secrecyT2Size();

        auto person = this->randomColumn(T2, 0, int(std::sqrt(T2)));
        auto disease = this->randomColumn(T2, 0, N_DISEASE);
        auto cost = this->randomColumn(T2, 0, MAX_COST);

        std::vector<std::string> schema = {"[person]", "[disease]", "cost"};

        EncodedTable<T> table = secret_share<T>({person, disease, cost}, schema);

        if (this->sqlite_db) {
            sqlite3_exec(this->sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE T2 (
                    person INTEGER, disease INTEGER, cost INTEGER
                );
            )sql";
            ret = sqlite3_exec(this->sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on T2 (Create): " << sqlite3_errmsg(this->sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO T2 (
                    person, disease, cost)
                VALUES (?, ?, ?);
            )sql";
            sqlite3_prepare_v2(this->sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < T2; ++i) {
                sqlite3_bind_int(stmt, 1, person[i]);
                sqlite3_bind_int(stmt, 2, disease[i]);
                sqlite3_bind_int(stmt, 3, cost[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on T2 (Insert): " << sqlite3_errmsg(this->sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            const char* create_indexes = R"sql(
                CREATE INDEX idx_person ON T2(person);
                CREATE INDEX idx_disease ON T2(disease);
            )sql";
            ret = sqlite3_exec(this->sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on create index: " << sqlite3_errmsg(this->sqlite_db))
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(this->sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on T2 (Commit): " << sqlite3_errmsg(this->sqlite_db))
            }
        } else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "T2";
        return table;
    }

    EncodedTable<T> getSecrecyT3Table() {
        auto T3 = secrecyT3Size();

        auto disease = this->randomColumn(T3, 0, N_DISEASE);
        auto class_ = this->randomColumn(T3, 0, N_DISEASE_CLASS);

        std::vector<std::string> schema = {"[disease]", "[class_]"};

        EncodedTable<T> table = secret_share<T>({disease, class_}, schema);

        if (this->sqlite_db) {
            sqlite3_exec(this->sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE T3 (
                    disease INTEGER, class_ INTEGER
                );
            )sql";
            ret = sqlite3_exec(this->sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on T3 (Create): " << sqlite3_errmsg(this->sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO T3 (
                    disease, class_)
                VALUES (?, ?);
            )sql";
            sqlite3_prepare_v2(this->sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < T3; ++i) {
                sqlite3_bind_int(stmt, 1, disease[i]);
                sqlite3_bind_int(stmt, 2, class_[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on T3 (Insert): " << sqlite3_errmsg(this->sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(this->sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on T3 (Commit): " << sqlite3_errmsg(this->sqlite_db))
            }
        } else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "T3";
        return table;
    }

    EncodedTable<T> getCreditScoreTable() {
        size_t S = STANDARD_SIZE * this->scaleFactor;

        size_t num_agencies = 10;
        // not everyone has a score from every agency
        size_t num_users = 2 * S / num_agencies;
        int BASE_YEAR = 2020;
        int num_years = 10;
        int min_credit = 300;
        int max_credit = 850;

        auto uid = this->randomColumn(S, 0, num_users);
        auto agency = this->randomColumn(S, 0, num_agencies);
        auto year = this->randomColumn(S, BASE_YEAR, BASE_YEAR + num_years);
        auto cs = this->randomColumn(S, min_credit, max_credit);

        std::vector<std::string> schema = {"[UID]", "[AGENCY]", "[YEAR]", "[CS]"};

        EncodedTable<T> table = secret_share<T>({uid, agency, year, cs}, schema);

        if (this->sqlite_db) {
            sqlite3_exec(this->sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE CREDIT (
                    UID INTEGER, AGENCY INTEGER, YEAR INTEGER, CS INTEGER
                );
            )sql";
            ret = sqlite3_exec(this->sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout(
                    "SQL error on CreditScore (Create): " << sqlite3_errmsg(this->sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO CREDIT (
                    UID, AGENCY, YEAR, CS)
                VALUES (?, ?, ?, ?);
            )sql";
            sqlite3_prepare_v2(this->sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, uid[i]);
                sqlite3_bind_int(stmt, 2, agency[i]);
                sqlite3_bind_int(stmt, 3, year[i]);
                sqlite3_bind_int(stmt, 4, cs[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout(
                        "SQL error on CreditScore (Insert): " << sqlite3_errmsg(this->sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(this->sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout(
                    "SQL error on CreditScore (Commit): " << sqlite3_errmsg(this->sqlite_db))
            }
        } else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "CREDIT";
        return table;
    }
};