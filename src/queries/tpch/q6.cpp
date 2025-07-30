/**
 * @file q6.cpp
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

#include "../../../include/secrecy.h"
#include "../../../include/benchmark/stopwatch.h"


#define TPCH_DATE_THRESHOLD 100
#define TPCH_DATE_INTERVAL 10 // arbitrary, just to account for date format
#define TPCH_DISCOUNT_THRESHOLD 5
#define TPCH_QUANTITY_THRESHOLD 40

#include "tpch_dbgen.h"
#include <sys/time.h>

// #define PRINT_TABLES

// #define QUERY_PROFILE

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace secrecy::operators;
using namespace std::chrono;
using namespace secrecy::aggregators;
using namespace secrecy::benchmarking;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using T = int64_t;

using sec = duration<float, seconds::period>;

int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    // Setup SQLite DB for output validation
    sqlite3 *sqlite_db = nullptr;
#ifndef QUERY_PROFILE
    if (pid == 0) {
        int err = sqlite3_open(NULL, &sqlite_db); // NULL -> Create in-memory database
        if(err) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        } else {
            single_cout("SQLite DB created");
        }
    }
#endif

    // Query DB setup
    auto db = TPCDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    single_cout("Q6 SF " << db.scaleFactor);

    // proactive sharing
    auto LineItem = db.getLineitemTable(true);

    single_cout("L size: " << LineItem.size());

    LineItem.project({"[ShipDateAdj]", "[ShipDateAdjInterval]", "ExtendedPrice", "Discount", "[DiscountHigh]", "[DiscountLow]", "[QuantityAdj]"});

#ifdef PRINT_TABLES
    single_cout("LINEITEM, size " << db.lineitemsSize());

    print_table(LineItem.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    LineItem.addColumns({"Revenue", "SumRevenue"});

    stopwatch::timepoint("Add temp columns");

    // l_shipdate >= date[DATE]
    // l_shipdate - DATE >= 0
    // ! (l_shipdate - date < 0)
    LineItem.filter(!*LineItem["[ShipDateAdj]"].ltz());

    // l_shipdate < date '[DATE]' + interval '1' year
    // l_shipdate - (date + interval) < 0
    LineItem.filter(LineItem["[ShipDateAdjInterval]"].ltz());

    // l_discount >= [DISCOUNT] - 0.01 AND l_discount <= [DISCOUNT] + 0.01
    // l_discount - (DISCOUNT - 1) >= 0
    // ! (l_discount - (DISCOUNT - 1) < 0)
    // 
    // l_discount <= DISCOUNT + 1
    // l_discount < DISCOUNT + 2
    // l_discount - (DISCOUNT + 2) < 0
    LineItem.filter(!*LineItem["[DiscountHigh]"].ltz());
    LineItem.filter(LineItem["[DiscountLow]"].ltz());

    // l_quantity < [QUANTITY]
    // l_quantity - QUANTITY < 0
    LineItem.filter(LineItem["[QuantityAdj]"].ltz());

    stopwatch::timepoint("Filters");

    LineItem["Revenue"] = LineItem["ExtendedPrice"] * LineItem["Discount"];

    // Global sum over Revenue. Multiply by valid to select correct values.
    LineItem.convert_b2a_bit(ENC_TABLE_VALID, "SumRevenue");
    LineItem["SumRevenue"] *= LineItem["Revenue"];
    LineItem.prefix_sum("SumRevenue");

    LineItem.tail(1);

    stopwatch::timepoint("Sum");

    // Print result
    LineItem.deleteColumns({
        "Discount", "ExtendedPrice", "Revenue"
    });

    // Only one row left, so no need to shuffle.

    stopwatch::done(); // print wall clock time
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();
    
#ifndef QUERY_PROFILE

    // pass `false` here because the last row might be invalid... but it still
    // contains the answer.
    auto result_column = LineItem.get_column(
        LineItem.open_with_schema(false), "SumRevenue");

    if (pid == 0) {

        size_t result = 0;
        if (result_column.size() > 0) {
            result = result_column[0];
        }

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

        // single_cout(std::endl);
        // single_cout("Calculated result | Revenue: " << result);
        // single_cout("SQL Query result  | Revenue: " << sqlResult << std::endl);

        ASSERT_SAME(sqlResult, result);
        single_cout("SQL OK");
    }

#endif
    // Close SQLite DB
    sqlite3_close(sqlite_db);

    MPI_Finalize();

    return 0;
}
