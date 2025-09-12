/**
 * @file q3.cpp
 * @brief Implements TPCH Query 3
 * @date 2024-06-1
 *
 * Equivalent SQL:
 * select
 *     l_orderkey,
 *     sum(l_extendedprice*(1-l_discount)) as revenue,
 *     o_orderdate,
 *     o_shippriority
 * from
 *     customer,
 *     orders,
 *     lineitem
 * where
 *     c_mktsegment = '[SEGMENT]'
 *     and c_custkey = o_custkey
 *     and l_orderkey = o_orderkey
 *     and o_orderdate < date '[DATE]'
 *     and l_shipdate > date '[DATE]'
 * group by
 *     l_orderkey,
 *     o_orderdate,
 *     o_shippriority
 * order by
 *     revenue desc,
 *     o_orderdate;
 *
 * Ignores o_shippriority because it is always 0 as per TCPH spec
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

    // TPCH Q3 query parameters
    const int DATE = 100;
    const int SEGMENT = 1;

    ////////////////////////////////////////////////////////////////
    // Database Initialization

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

    // Query DB setup
    auto db = TPCDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    single_cout("Q3 SF " << db.scaleFactor);

    ////////////////////////////////////////////////////////////////
    // Query

    auto LineItem = db.getLineitemTable();
    auto Orders = db.getOrdersTable();
    auto Customers = db.getCustomersTable();

    LineItem.project({"[OrderKey]", "[ShipDate]", "ExtendedPrice", "Discount"});
    Orders.project({"[OrderKey]", "[CustKey]", "[OrderDate]"});
    Customers.project({"[CustKey]", "[MktSegment]"});

#ifdef PRINT_TABLES
    single_cout("CUSTOMERS, size " << db.customersSize());
    print_table(Customers.open_with_schema(), pid);

    single_cout("ORDERS, size " << db.ordersSize());
    print_table(Orders.open_with_schema(), pid);

    single_cout("LINEITEM, size " << db.lineitemsSize());
    print_table(LineItem.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // c_mktsegment = '[SEGMENT]'
    Customers.filter(Customers["[MktSegment]"] == SEGMENT);
    Customers.deleteColumns({"[MktSegment]"});

    // and o_orderdate < date '[DATE]'
    Orders.filter(Orders["[OrderDate]"] < DATE);

    // and l_shipdate > date '[DATE]'
    LineItem.filter(LineItem["[ShipDate]"] > DATE);
    LineItem.deleteColumns({"[ShipDate]"});

    stopwatch::timepoint("Filters");

    LineItem.addColumns({"Revenue"}, LineItem.size());
    LineItem["Revenue"] = LineItem["ExtendedPrice"] * (-LineItem["Discount"] + 100) / 100;
    LineItem.deleteColumns({"ExtendedPrice", "Discount"});

    stopwatch::timepoint("Revenue");

    auto CO = Customers.inner_join(Orders, {"[CustKey]"});
    CO.deleteColumns({"[CustKey]"});

    stopwatch::timepoint("Customers + Orders join");

    CO.addColumns({"GroupRevenue"}, CO.size());
    // TODO: doesn't group by [OrderDate] but [OrderKey] should be unique anyways so I'm not sure it
    // matters
    auto COL = CO.inner_join(LineItem, {"[OrderKey]"},
                             {
                                 {"Revenue", "GroupRevenue", sum<A>},
                                 {"[OrderDate]", "[OrderDate]", copy<B>},
                             });
    COL.deleteColumns({"Revenue"});

    stopwatch::timepoint("Customers/Orders + LineItem join");

    COL.addColumns({"[GroupRevenue]"}, COL.size());
    COL.convert_a2b("GroupRevenue", "[GroupRevenue]");
    COL.deleteColumns({"GroupRevenue"});

    // Sort over valid first to move valid columns to the top
    COL.sort({std::make_pair(ENC_TABLE_VALID, DESC), std::make_pair("[GroupRevenue]", DESC),
              std::make_pair("[OrderDate]", ASC)});

    stopwatch::timepoint("Sort");

    // Return only the 10 orders with the highest value
    COL.head(10);

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    COL.finalize();
#endif

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    ////////////////////////////////////////////////////////////////
    // Correctness Test

#ifndef QUERY_PROFILE

    auto resultOpened = COL.open_with_schema();
    auto key_col = COL.get_column(resultOpened, "[OrderKey]");
    auto revenue_col = COL.get_column(resultOpened, "[GroupRevenue]");
    auto date_col = COL.get_column(resultOpened, "[OrderDate]");

    if (pid == 0) {
        // Run Q3 through SQL to verify result
        int ret;
        const char* query = R"sql(
            select
                l.OrderKey,
                sum(l.ExtendedPrice*(100-l.Discount)/100) as Revenue,
                o.OrderDate
            from
                CUSTOMER as c,
                ORDERS as o,
                LINEITEM as l
            where
                c.MktSegment = ?
                and c.CustKey = o.CustKey
                and l.OrderKey = o.OrderKey
                and o.OrderDate < ?
                and l.ShipDate > ?
            group by
                l.OrderKey,
                o.OrderDate
            order by
                Revenue DESC,
                o.OrderDate
            limit 10
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, SEGMENT);
        sqlite3_bind_int(stmt, 2, DATE);
        sqlite3_bind_int(stmt, 3, DATE);

        // Assert result against SQL result
        auto res = sqlite3_step(stmt);
        auto i = 0;
        while (res == SQLITE_ROW) {
            int sql_key = sqlite3_column_int(stmt, 0);
            int sql_revenue = sqlite3_column_int(stmt, 1);
            int sql_date = sqlite3_column_int(stmt, 2);

            ASSERT_SAME(sql_key, key_col[i]);
            ASSERT_SAME(sql_revenue, revenue_col[i]);
            ASSERT_SAME(sql_date, date_col[i]);

            res = sqlite3_step(stmt);
            ++i;
        }
        if (res == SQLITE_ERROR) {
            auto errmsg = sqlite3_errmsg(sqlite_db);
            throw std::runtime_error(errmsg);
        }
        ASSERT_SAME(i, key_col.size());
        if (i == 0) {
            single_cout("Empty result");
        }

        if (res != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }
    }

    // Close SQLite DB
    sqlite3_close(sqlite_db);
#endif

    return 0;
}
