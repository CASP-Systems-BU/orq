/**
 * @file sf-q6.cpp
 * @brief Implements TPCH Query 6
 * @date 2024-04-08
 *
 * Equivalent SQL:
 *  select
 *      sum(l_extendedprice*l_discount) as revenue
 *  from
 *      lineitem
 *where
 *      l_shipdate >= date '[DATE]'
 *      and l_shipdate < date '[DATE]' + interval '1' year
 *      and l_discount between [DISCOUNT] - 0.01 and [DISCOUNT] + 0.01
 *      and l_quantity < [QUANTITY];
 *
 */

#include "orq.h"
#include "profiling/stopwatch.h"

#define TPCH_DATE_THRESHOLD 37
#define TPCH_DATE_INTERVAL 365  // arbitrary, just to account for date format
#define TPCH_DISCOUNT_THRESHOLD 5
#define TPCH_QUANTITY_THRESHOLD 60

#include <sys/time.h>

#include "../tpch/tpch_dbgen.h"

// #define PRINT_TABLES
// #define QUERY_PROFILE

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

using T = int32_t;

using sec = duration<float, seconds::period>;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }
    sf /= 2.0;

    // Setup SQLite DB for output validation
    sqlite3* sqlite_db = nullptr;
    int err = sqlite3_open(NULL, &sqlite_db);  // NULL -> Create in-memory database
    if (err) {
        throw std::runtime_error(sqlite3_errmsg(sqlite_db));
    } else {
        single_cout("SQLite DB created");
    }

    // Query DB setup
    auto db = TPCDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    single_cout("SecretFlow Q6 SF " << db.scaleFactor * 2);

    // proactive sharing

    // TPCH Generator
    // auto LineItem = db.getLineitemTable(true);

    // SecretFlow Generator, TPCH Ratios
    // auto LineItem = db.getLineitemTableSecretFlow(sf * 6'000'000);

    // SecretFlow Generator, Secretflow Ratios
    auto LineItem = db.getLineitemTableSecretFlow(sf * (1 << 20));

    single_cout("\nLINEITEM size: " << LineItem.size() << std::endl);

    LineItem.project({"[ShipDateAdj]", "[ShipDateAdjInterval]", "ExtendedPrice", "Discount",
                      "[DiscountHigh]", "[DiscountLow]", "[QuantityAdj]"});

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // Run Q6 through SQLite to verify result
    int ret;
    const char* query = R"sql(
        select
            sum(ExtendedPrice*Discount) as revenue
        from
            LINEITEM
        where
            ShipDate >= ?
            and ShipDate < (? + ?)
            and Discount >= (? - 1) and  Discount <= (? + 1)
            and Quantity < ?;
    )sql";
    sqlite3_stmt* stmt;
    ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
    // Fill in query placeholders
    sqlite3_bind_int(stmt, 1, TPCH_DATE_THRESHOLD);
    sqlite3_bind_int(stmt, 2, TPCH_DATE_THRESHOLD);
    sqlite3_bind_int(stmt, 3, TPCH_DATE_INTERVAL);
    sqlite3_bind_int(stmt, 4, TPCH_DISCOUNT_THRESHOLD);
    sqlite3_bind_int(stmt, 5, TPCH_DISCOUNT_THRESHOLD);
    sqlite3_bind_int(stmt, 6, TPCH_QUANTITY_THRESHOLD);

    sqlite3_step(stmt);
    size_t sqlResult = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);

    stopwatch::timepoint("plaintext");

    int64_t otherResult = 0;
    if (pid == 1) {
        runTime->comm0()->sendShare((int64_t)sqlResult, 1);
    } else {
        runTime->comm0()->receiveShare((int64_t&)otherResult, 1);
    }

    int64_t result = sqlResult + otherResult;

    stopwatch::timepoint("sum");

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();
    /*
    There is no way to check correctness of this query, because we are using
    SQLite (the thing we use to check correctness) to do all the work anyway.
    So by definition, the query is correct, since we consider SQLite as the
    ground truth.
    */

    // Close SQLite DB
    sqlite3_close(sqlite_db);

    return 0;
}
