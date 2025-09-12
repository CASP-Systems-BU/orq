/**
 * @file market-share.cpp
 * @brief Implements Market Share (HHI) query from Conclave
 * @date 2024-11-08
 *
 * HHI is define as the sum of squares of market-shares of each company, where
 * market share equals revenue / total revenue.
 *
 * Total revenue can be factored out, so we compute sum-of-squares of revenues,
 * then divide by the squared total revenue. This allows computation of only a
 * single private division.
 *
 * SQL:
 *   SELECT SUM(Rev * Rev) / (TotalRev * TotalRev) AS HHI
 *   FROM (
 *     SELECT Sum(Fare) AS Rev, SUM(SUM(Fare)) OVER () AS TotalRev
 *     FROM TAXI_DATA
 *     WHERE TYPE = 'AIRPORT'
 *     GROUP BY Company
 *   )
 *
 * HHI can be computed with integer percents, in which case we multiply by
 * 10,000.
 */

#include "orq.h"
#include "secrecy_dbgen.h"

// #define QUERY_PROFILE

// #define PRINT_TABLES

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using T = int64_t;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char **argv) {
    orq_init(argc, argv);
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

    // prevent overflow for large inputs
    auto db = SecrecyDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    // arbitrary filtering
    const int AIRPORT = 0;

    auto T = db.getTaxiTable();
    single_cout("MarketShare SF " << db.scaleFactor << " (" << T.size() << " rows)");

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    T.filter(T["[Type]"] == AIRPORT);
    T.deleteColumns({"[Type]"});
    T.addColumns({"CompanyRevenue"});
    T.aggregate({"[Company]"}, {{"Fare", "CompanyRevenue", sum<A>}});
    T.deleteColumns({"[Company]", "Fare"});

    stopwatch::timepoint("CompAgg");

    T.addColumns({"Valid", "TotalRevenue", "SumSq"});
    T.convert_b2a_bit(ENC_TABLE_VALID, "Valid");
    T["TotalRevenue"] = T["Valid"] * T["CompanyRevenue"];
    T.prefix_sum("TotalRevenue");  // result in last row

    stopwatch::timepoint("TotalAgg");

    T["SumSq"] = T["Valid"] * T["CompanyRevenue"] * T["CompanyRevenue"];
    T.prefix_sum("SumSq");

    stopwatch::timepoint("SumSq");

    // results in the last row.
    T.tail(1);

    // Square the total revenue
    T["TotalRevenue"] = T["TotalRevenue"] * T["TotalRevenue"];

    T.addColumn({"[HHI]"});
    T["[HHI]"] = T["SumSq"] * 10000 / T["TotalRevenue"];
    T.project({"[HHI]"});

    stopwatch::timepoint("HHI");

#ifdef QUERY_PROFILE
    T.finalize();
#endif

    stopwatch::done();
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE
    auto result = T.open_with_schema(false);
    print_table(result, pid);

    if (pid == 0) {
        const char *query = R"sql(
            SELECT SUM(10000 * R * R) / (T * T) as HHI
            FROM (
                SELECT Company, SUM(Fare) AS R, SUM(SUM(Fare)) OVER () AS T
                FROM TAXIDATA
                WHERE TRIPTYPE = ?
                GROUP BY Company
            )
        )sql";

        sqlite3_stmt *stmt;
        auto err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        if (err) {
            std::cerr << "sqlite error: " << sqlite3_errmsg(sqlite_db) << "\n";
            abort();
        }

        err = sqlite3_bind_int(stmt, 1, AIRPORT);
        if (err) {
            std::cerr << "sqlite error: " << sqlite3_errmsg(sqlite_db) << "\n";
            abort();
        }

        int i;
        for (i = 0; (err = sqlite3_step(stmt)) == SQLITE_ROW; i++) {
            int hhi = sqlite3_column_int(stmt, 0);

            std::cout << "hhi=" << hhi << "\n";
            assert(hhi == T.get_column(result, "[HHI]")[i]);
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
}