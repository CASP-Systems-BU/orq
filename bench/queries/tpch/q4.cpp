/**
 * @file q4.cpp
 * @brief Implements TPCH Query 4
 * @date 2024-06-1
 *
 * Equivalent SQL:
 *   select
 *       o_orderpriority,
 *       count(*) as order_count
 *   from
 *       orders
 *   where
 *       o_orderdate >= date '[DATE]'
 *       and o_orderdate < date '[DATE]' + interval '3' month
 *       and exists (
 *           select
 *               *
 *           from
 *               lineitem
 *           where
 *               l_orderkey = o_orderkey
 *               and l_commitdate < l_receiptdate
 *       )
 *       group by
 *           o_orderpriority
 *       order by
 *           o_orderpriority;
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

    // TPCH Q6 query parameters
    const int DATE = 100;
    const int DATE_INTERVAL = 10;  // Arbitrary date interval to account for date format

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

    single_cout("Q4 SF " << db.scaleFactor);

    ////////////////////////////////////////////////////////////////
    // Query

    auto LineItem = db.getLineitemTable();
    auto Orders = db.getOrdersTable();

    single_cout("L size: " << LineItem.size());
    single_cout("O size: " << Orders.size());

    LineItem.project({"[OrderKey]", "[CommitDate]", "[ReceiptDate]"});
    Orders.project({"[OrderKey]", "[OrderPriority]", "[OrderDate]"});

#ifdef PRINT_TABLES
    single_cout("ORDERS, size " << db.ordersSize());
    print_table(Orders.open_with_schema(), pid);

    single_cout("LINEITEM, size " << db.lineitemsSize());
    print_table(LineItem.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // o_orderdate >= date '[DATE]'
    Orders.filter(Orders["[OrderDate]"] >= DATE);

    // and o_orderdate < date '[DATE]' + interval '3' month
    Orders.filter(Orders["[OrderDate]"] < DATE + DATE_INTERVAL);

    // and l_commitdate < l_receiptdate
    LineItem.filter(LineItem["[CommitDate]"] < LineItem["[ReceiptDate]"]);

    stopwatch::timepoint("Filters");

    // l_orderkey = o_orderkey
    auto OrderLineItemJoin = Orders.inner_join(LineItem, {"[OrderKey]"},
                                               {{"[OrderPriority]", "[OrderPriority]", copy<B>}});

    stopwatch::timepoint("Join");

    // and exists (select...)
    // DISTINCT after the join to replicate the semantics of EXISTS
    OrderLineItemJoin.distinct({"[OrderKey]"});

    stopwatch::timepoint("Distinct");

    // Adding extra columns for count
    std::vector<std::string> count_extra_columns = {"OrderCount"};
    OrderLineItemJoin.addColumns(count_extra_columns, OrderLineItemJoin.size());

    // count(*) as order_count
    OrderLineItemJoin.aggregate({"[OrderPriority]"}, {{"OrderCount", "OrderCount", count<A>}});

    stopwatch::timepoint("Count");

    // Deleting extra columns
    OrderLineItemJoin.deleteColumns({"[CommitDate]", "[OrderDate]", "[OrderKey]", "[ReceiptDate]"});

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    OrderLineItemJoin.finalize();
#endif

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    ////////////////////////////////////////////////////////////////
    // Correctness Test

#ifndef QUERY_PROFILE

    // Fetch result
    auto resultOpened = OrderLineItemJoin.open_with_schema();
    auto priority_col = OrderLineItemJoin.get_column(resultOpened, "[OrderPriority]");
    auto count_col = OrderLineItemJoin.get_column(resultOpened, "OrderCount");

    if (pid == 0) {
        // Create result map to assert against SQL
        std::unordered_map<int, int> resultMap;
        for (int i = 0; i < priority_col.size(); i++) {
            resultMap[priority_col[i]] = count_col[i];
        }

        // Run Q4 through SQL to verify result
        int ret;
        const char* query = R"sql(
            select
                o.OrderPriority,
                count(*) as OrderCount
            from
                ORDERS as o
            where
                o.OrderDate >= ?
                and o.OrderDate < (? + ?)
                and exists (
                    select
                        *
                    from
                        LINEITEM as l
                    where
                        l.OrderKey = o.OrderKey
                        and l.CommitDate < l.ReceiptDate
                )
            group by
                o.OrderPriority
            order by
                o.OrderPriority
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, DATE);
        sqlite3_bind_int(stmt, 2, DATE);
        sqlite3_bind_int(stmt, 3, DATE_INTERVAL);

        // Assert result against SQL result
        int i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            int sqlPriority = sqlite3_column_int(stmt, 0);
            int sqlCount = sqlite3_column_int(stmt, 1);

            // single_cout("Priority " << sqlPriority << " -> SQL: " << sqlCount << " | Query: " <<
            // resultMap[sqlPriority]);

            ASSERT_SAME(sqlCount, resultMap[sqlPriority]);
            i++;
        }
        ASSERT_SAME(i, priority_col.size());
        if (i == 0) {
            single_cout("Empty result");
        }

        if (ret != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }

        sqlite3_finalize(stmt);
    }

#endif

    // Close SQLite DB
    sqlite3_close(sqlite_db);

    return 0;
}
