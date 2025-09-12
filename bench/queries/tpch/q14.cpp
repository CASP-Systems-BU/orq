/**
 * @file q14.cpp
 * @brief TPCH Query 14
 *
 * Equivalent SQL:
 *  select
 *      100.00 * sum(case
 *                  when p_type like 'PROMO%'
 *                  then l_extendedprice*(1-l_discount)
 *                  else 0
 *              end) / sum(l_extendedprice * (1 - l_discount)) as promo_revenue
 *  from
 *      lineitem,
 *      part
 *  where
 *      l_partkey = p_partkey
 *      and l_shipdate >= date '[DATE]'
 *      and l_shipdate < date '[DATE]' + interval '1' month
 *
 */

#include <sys/time.h>

#include "orq.h"
#include "profiling/stopwatch.h"
#include "tpch_dbgen.h"

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

using T = int64_t;

using sec = duration<float, seconds::period>;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    // TPCH Q14 query parameters
    const int DATE = 100;
    const int DATE_INTERVAL = 10;  // Arbitrary date interval to account for date format

    // Setup SQLite DB for output validation
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

    ////////////////////////////////////////////////////////////////
    // Database Initialization

    // Query DB setup
    auto db = TPCDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    single_cout("Q14 SF " << db.scaleFactor);

    ////////////////////////////////////////////////////////////////
    // Query

    auto LineItem = db.getLineitemTable();
    auto Part = db.getPartTable();

    LineItem.project({"[ShipDate]", "[PartKey]", "Discount", "ExtendedPrice"});
    Part.project({"[PartKey]", "[Type]"});

#ifdef PRINT_TABLES
    single_cout("LINEITEM, size " << db.lineitemsSize());
    print_table(LineItem.open_with_schema(), pid);

    single_cout("PART, size " << db.partSize());
    print_table(Part.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // [SQL] and l_shipdate >= date '[DATE]'
    // [SQL] and l_shipdate < date '[DATE]' + interval '1' month
    LineItem.filter(LineItem["[ShipDate]"] >= DATE);
    LineItem.filter(LineItem["[ShipDate]"] < DATE + DATE_INTERVAL);

    stopwatch::timepoint("Filters");

    // [SQL] p_partkey = l_partkey
    auto Join = Part.inner_join(LineItem, {"[PartKey]"}, {{"[Type]", "[Type]", copy<B>}});

    stopwatch::timepoint("Join");

    std::vector<std::string> sum_extra_columns = {"Sum", "[Divide]"};

    // Sum 1
    // [SQL] sum(case...end)
    // Note: p_type like 'PROMO%' implemented as [Type] == 1
    auto Case1 = Join.deepcopy();
    Case1.addColumns(sum_extra_columns, Case1.size());
    Case1.filter(Case1["[Type]"] == 1);
    Case1["ExtendedPrice"] = Case1["ExtendedPrice"] * (-Case1["Discount"] + 100) / 100;
    Case1.convert_b2a_bit(ENC_TABLE_VALID, "Sum");
    Case1["Sum"] = Case1["Sum"] * Case1["ExtendedPrice"];
    Case1.prefix_sum("Sum");
    Case1.tail(1);
    // print_table(Sum1.open_with_schema(), pid);

    stopwatch::timepoint("Agg: Sum 1");

    // Sum 2
    // [SQL] sum(l_extendedprice * (1 - l_discount))
    auto Case2 = Join.deepcopy();
    Case2.addColumns(sum_extra_columns, Case2.size());
    Case2["ExtendedPrice"] = Case2["ExtendedPrice"] * (-Case2["Discount"] + 100) / 100;
    Case2.convert_b2a_bit(ENC_TABLE_VALID, "Sum");
    Case2["Sum"] = Case2["Sum"] * Case2["ExtendedPrice"];
    Case2.prefix_sum("Sum");
    Case2.tail(1);
    // print_table(Sum2.open_with_schema(), pid);

    stopwatch::timepoint("Agg: Sum 2");

    // Multiply by 100 and divide
    // [SQL] 100.00 * sum...
    Case1["Sum"] = Case1["Sum"] * 100;

    // [SQL] sum(case...end) / sum(...)
    Case1["[Divide]"] = Case1["Sum"] / Case2["Sum"];

    stopwatch::timepoint("Division");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    Case1.finalize();
#endif

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    ////////////////////////////////////////////////////////////////
    // Correctness Test

#ifndef QUERY_PROFILE

    // Fetch result
    // ignore valid
    auto result_column = Case1.get_column(Case1.open_with_schema(false), "[Divide]");

    if (pid == 0) {
        size_t result = 0;
        if (result_column.size() > 0) {
            result = result_column[0];
        }

        int ret;
        // Note: "Type == 1" used as a substitute for "p_type like 'PROMO%'""
        const char* query = R"sql(
            select 
                100 * sum(case
                            when p.Type = 1
                            then l.ExtendedPrice*(100 - l.Discount)/100
                            else 0
                        end) / sum(l.ExtendedPrice * (100 - l.Discount)/100) as promo_revenue
            from
                LINEITEM l,
                PART p
            where
                l.PartKey = p.PartKey
                and l.ShipDate >= ?
                and l.ShipDate < (? + ?)
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, DATE);
        sqlite3_bind_int(stmt, 2, DATE);
        sqlite3_bind_int(stmt, 3, DATE_INTERVAL);

        size_t sqlResult;
        ret = sqlite3_step(stmt);
        if (ret != SQLITE_ROW) {
            std::cerr << "Execution failed: " << sqlite3_errmsg(sqlite_db) << std::endl;
        } else {
            sqlResult = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);

        // single_cout(std::endl);
        single_cout("Calculated result | Promo revenue: " << result);
        single_cout("SQL Query result  | Promo revenue: " << sqlResult << std::endl);

        ASSERT_SAME(sqlResult, result);
    }
#endif

    // Close SQLite DB
    sqlite3_close(sqlite_db);

    return 0;
}
