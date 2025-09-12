/**
 * @file q10.cpp
 * @brief TPCH Query 10
 *
 * Equivalent SQL:
 *  select
 *      c_custkey,
 *      c_name,
 *      sum(l_extendedprice * (1 - l_discount)) as revenue,
 *      c_acctbal,
 *      n_name,
 *      c_address,
 *      c_phone,
 *      c_comment
 *  from
 *      customer,
 *      orders,
 *      lineitem,
 *      nation
 *  where
 *      c_custkey = o_custkey
 *      and l_orderkey = o_orderkey
 *      and o_orderdate >= date '[DATE]'
 *      and o_orderdate < date '[DATE]' + interval '3' month
 *      and l_returnflag = 'R'
 *      and c_nationkey = n_nationkey
 *  group by
 *      c_custkey,
 *      c_name,
 *      c_acctbal,
 *      c_phone,
 *      n_name,
 *      c_address,
 *      c_comment
 *  order by
 *      revenue desc;
 *
 */

#include <stdlib.h> /* atof */

#include "orq.h"
#include "profiling/stopwatch.h"
#include "tpch_dbgen.h"

// #define QUERY_PROFILE

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using T = int64_t;

using namespace orq::aggregators;
using namespace orq::benchmarking;

using A = ASharedVector<T>;
using B = BSharedVector<T>;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pid = runTime->getPartyID();

    // TPCH Q10 query parameters
    const int DATE = 100;
    const int DATE_INTERVAL = 10;  // Arbitrary date interval to account for date format
    const int RETURNFLAG = 0;

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

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    ////////////////////////////////////////////////////////////////
    // Database Initialization

    auto db = TPCDatabase<T>(sf, sqlite_db);
    single_cout("Q10 SF " << db.scaleFactor);

    ////////////////////////////////////////////////////////////////
    // Query

    // get the tables
    auto Customers = db.getCustomersTable();
    auto Orders = db.getOrdersTable();
    auto LineItem = db.getLineitemTable();
    auto Nations = db.getNationTable();

    Customers.project(
        {"[CustKey]", "[C_Name]", "[AcctBal]", "[Phone]", "[Address]", "[Comment]", "[NationKey]"});
    Orders.project({"[OrderKey]", "[CustKey]", "[OrderDate]"});
    LineItem.project({"[OrderKey]", "[ReturnFlag]", "ExtendedPrice", "Discount"});
    Nations.project({"[NationKey]", "[Name]"});

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // Filter LineItem for returned items
    LineItem.filter(LineItem["[ReturnFlag]"] == RETURNFLAG);
    LineItem.deleteColumns({"[ReturnFlag]"});
    // Filter Orders for dates
    Orders.filter(Orders["[OrderDate]"] >= DATE);
    Orders.filter(Orders["[OrderDate]"] < DATE + DATE_INTERVAL);
    Orders.deleteColumns({"[OrderDate]"});

    stopwatch::timepoint("filter");

    // compute revenue
    LineItem.addColumns({"Revenue"}, LineItem.size());
    LineItem["Revenue"] = LineItem["ExtendedPrice"] * (-LineItem["Discount"] + 100) / 100;
    LineItem.deleteColumns({"ExtendedPrice", "Discount"});

    stopwatch::timepoint("revenue calculation");

    // Perform the joins
    auto OrderLineItemJoin =
        Orders.inner_join(LineItem, {"[OrderKey]"}, {{"[CustKey]", "[CustKey]", copy<B>}});

    // Join the result with Customers
    auto CustomerOrderLineItemJoin =
        Customers.inner_join(OrderLineItemJoin, {"[CustKey]"},
                             {{"[C_Name]", "[C_Name]", copy<B>},
                              {"[AcctBal]", "[AcctBal]", copy<B>},
                              {"[Phone]", "[Phone]", copy<B>},
                              {"[Address]", "[Address]", copy<B>},
                              {"[Comment]", "[Comment]", copy<B>},
                              {"[NationKey]", "[NationKey]", copy<B>}});

    // Finally, join with Nations
    auto FinalJoin = Nations.inner_join(CustomerOrderLineItemJoin, {"[NationKey]"},
                                        {{"[Name]", "[Name]", copy<B>}});

    stopwatch::timepoint("join");

    // Group by and aggregate
    auto result = FinalJoin.aggregate({"[CustKey]"}, {{"Revenue", "Revenue", sum<A>}});

    stopwatch::timepoint("aggregation");

    // convert revenue column to binary
    result.addColumns({"[Revenue]"}, result.size());
    result.convert_a2b("Revenue", "[Revenue]");
    result.deleteColumns({"Revenue"});

    // Sort the result
    result.sort({"[Revenue]"}, DESC);

    stopwatch::timepoint("sort");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    result.finalize();
#endif

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    ////////////////////////////////////////////////////////////////
    // Correctness Test

#ifndef QUERY_PROFILE

    auto resultOpened = result.open_with_schema();
    auto custkey_col = result.get_column(resultOpened, "[CustKey]");
    auto name_col = result.get_column(resultOpened, "[C_Name]");
    auto acctbal_col = result.get_column(resultOpened, "[AcctBal]");
    auto phone_col = result.get_column(resultOpened, "[Phone]");
    auto nation_col = result.get_column(resultOpened, "[Name]");
    auto address_col = result.get_column(resultOpened, "[Address]");
    auto comment_col = result.get_column(resultOpened, "[Comment]");
    auto revenue_col = result.get_column(resultOpened, "[Revenue]");

    // SQL validation
    if (pid == 0) {
        // Run Q10 through SQL to verify result
        int ret;
        const char* query = R"sql(
            select
                c.CustKey,
                c.Name,
                sum(l.ExtendedPrice * (100 - l.Discount) / 100) as Revenue,
                c.AcctBal,
                n.Name,
                c.Address,
                c.Phone,
                c.Comment
            from
                CUSTOMER as c,
                ORDERS as o,
                LINEITEM as l,
                NATION as n
            where
                c.CustKey = o.CustKey
                and l.OrderKey = o.OrderKey
                and o.OrderDate >= ?
                and o.OrderDate < ? + ?
                and l.ReturnFlag = ?
                and c.NationKey = n.NationKey
            group by
                c.CustKey,
                c.Name,
                c.AcctBal,
                c.Phone,
                n.Name,
                c.Address,
                c.Comment
            order by
                Revenue desc
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, DATE);
        sqlite3_bind_int(stmt, 2, DATE);
        sqlite3_bind_int(stmt, 3, DATE_INTERVAL);
        sqlite3_bind_int(stmt, 4, RETURNFLAG);

        // Assert result against SQL result
        auto res = sqlite3_step(stmt);
        auto i = 0;
        while (res == SQLITE_ROW) {
            int sql_custkey = sqlite3_column_int(stmt, 0);
            int sql_name = sqlite3_column_int(stmt, 1);
            int sql_revenue = sqlite3_column_int(stmt, 2);
            int sql_acctbal = sqlite3_column_int(stmt, 3);
            int sql_nation = sqlite3_column_int(stmt, 4);
            int sql_address = sqlite3_column_int(stmt, 5);
            int sql_phone = sqlite3_column_int(stmt, 6);
            int sql_comment = sqlite3_column_int(stmt, 7);

            ASSERT_SAME(sql_custkey, custkey_col[i]);
            ASSERT_SAME(sql_name, name_col[i]);
            ASSERT_SAME(sql_revenue, revenue_col[i]);
            ASSERT_SAME(sql_acctbal, acctbal_col[i]);
            ASSERT_SAME(sql_nation, nation_col[i]);
            ASSERT_SAME(sql_address, address_col[i]);
            ASSERT_SAME(sql_phone, phone_col[i]);
            ASSERT_SAME(sql_comment, comment_col[i]);

            res = sqlite3_step(stmt);
            ++i;
        }
        ASSERT_SAME(i, custkey_col.size());
        if (i == 0) {
            single_cout("Empty result");
        }
        if (res == SQLITE_ERROR) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
        if (res != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }

        std::cout << i << " rows OK\n";
    }

#endif
    // Clean up
    if (sqlite_db) {
        sqlite3_close(sqlite_db);
    }

    return 0;
}