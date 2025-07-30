/**
 * @file q18.cpp
 * @brief Implements TPCH Query 18
 *
 * Equivalent SQL:
 *  select
 *      c_name,
 *      c_custkey,
 *      o_orderkey,
 *      o_orderdate,
 *      o_totalprice,
 *      sum(l_quantity)
 *  from
 *      customer,
 *      orders,
 *      lineitem
 *  where
 *      o_orderkey in (
 *          select
 *              l_orderkey
 *          from
 *              lineitem
 *          group by
 *              l_orderkey
 *          having
 *              sum(l_quantity) > [QUANTITY]
 *      )
 *      and c_custkey = o_custkey
 *      and o_orderkey = l_orderkey
 *  group by
 *      c_name,
 *      c_custkey,
 *      o_orderkey,
 *      o_orderdate,
 *      o_totalprice
 *  order by
 *      o_totalprice desc,
 *      o_orderdate;
 *
 */

#include "../../../include/secrecy.h"
#include "../../../include/benchmark/stopwatch.h"

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

    // TPCH Q18 query parameters
    // Randomly selected within [312..315] as per spec. This doesn't work for the currently enforced LINEITEMS_PER_ORDER.
    // The above range is quite close to the upper bound in quantity per order (50 * 7 = 350). 
    // Using 180 for the current upper bound (50 * 4 = 200).
    const int QUANTITY = 180;

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

    single_cout("Q18 SF " << db.scaleFactor);

    auto Customer = db.getCustomersTable();
    auto Order = db.getOrdersTable();
    auto LineItem = db.getLineitemTable();

    Customer.project({"[CustKey]", "[C_Name]"});
    Order.project({"[OrderKey]", "[CustKey]", "[OrderDate]", "[TotalPrice]"});
    LineItem.project({"[OrderKey]", "Quantity"});

#ifdef PRINT_TABLES
    single_cout("CUSTOMER, size " << db.customersSize());
    print_table(Customer.open_with_schema(), pid);

    single_cout("ORDER, size " << db.ordersSize());
    print_table(Order.open_with_schema(), pid);

    single_cout("LINEITEM, size " << db.lineitemsSize());
    print_table(LineItem.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // [SQL] group by l_orderkey + sum(l_quantity)
    LineItem.addColumns({"SumQuantity", "[SumQuantity]"}, LineItem.size());
    LineItem.aggregate({"[OrderKey]"}, {{"Quantity", "SumQuantity", sum<A>}});

    stopwatch::timepoint("SubQuery Agg");

    // [SQL] having sum(l_quantity) > QUANTITY
    LineItem.convert_a2b("SumQuantity", "[SumQuantity]");
    LineItem.filter(LineItem["[SumQuantity]"] > QUANTITY);
    LineItem.project({"[OrderKey]", "SumQuantity"});

    stopwatch::timepoint("HAVING Filter");

    // [SQL] o_orderkey in (subquery)
    auto FilteredOrders = Order.inner_join(LineItem, {"[OrderKey]"}, {
        {"[CustKey]", "[CustKey]", copy<B>},
        {"[OrderDate]", "[OrderDate]", copy<B>},
        {"[TotalPrice]", "[TotalPrice]", copy<B>}
    });

    stopwatch::timepoint("OrderKey Join");

    // [SQL] c_custkey = o_custkey
    auto FinalJoin = Customer.inner_join(FilteredOrders, {"[CustKey]"}, {
        {"[C_Name]", "[C_Name]", copy<B>}
    });

    stopwatch::timepoint("CustKey Join");

    // [SQL] group by ... + sum(l_quantity)
    FinalJoin.addColumns({"FinalSum"}, FinalJoin.size());
    FinalJoin.aggregate({ENC_TABLE_VALID, "[CustKey]", "[OrderKey]"}, {
        {"SumQuantity", "FinalSum", sum<A>}
    });

    stopwatch::timepoint("Group by");

    // [SQL] order by o_totalprice desc, o_orderdate
    FinalJoin.sort({
        std::make_pair("[TotalPrice]", DESC),
        std::make_pair("[OrderDate]", ASC)
    });

    stopwatch::timepoint("Order by");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    FinalJoin.finalize();
#endif

    stopwatch::done(); // print wall clock time
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    auto result = FinalJoin.open_with_schema();
    
    auto c_name = FinalJoin.get_column(result, "[C_Name]");
    auto c_custkey = FinalJoin.get_column(result, "[CustKey]");
    auto o_orderkey = FinalJoin.get_column(result, "[OrderKey]");
    auto o_orderdate = FinalJoin.get_column(result, "[OrderDate]");
    auto o_totalprice = FinalJoin.get_column(result, "[TotalPrice]");
    auto sum_quantity = FinalJoin.get_column(result, "FinalSum");

    if (pid == 0) {

        // Fetch Q18 SQL result to validate
        int ret;
        const char* query = R"sql(
            select
                c.name,
                c.custkey,
                o.orderkey,
                o.orderdate,
                o.totalprice,
                sum(l.quantity)
            from
                customer as c,
                orders as o,
                lineitem as l
            where
                o.orderkey in (
                    select
                        l.orderkey
                    from
                        lineitem as l
                    group by
                        l.orderkey
                    having
                        sum(l.quantity) > ?
                )
                and c.custkey = o.custkey
                and o.orderkey = l.orderkey
            group by
                c.name,
                c.custkey,
                o.orderkey,
                o.orderdate,
                o.totalprice
            order by
                o.totalprice desc,
                o.orderdate;
        )sql";

        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, QUANTITY);

        size_t i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            int sql_c_name = sqlite3_column_int(stmt, 0);
            int sql_c_custkey = sqlite3_column_int(stmt, 1);
            int sql_o_orderkey = sqlite3_column_int(stmt, 2);
            int sql_o_orderdate = sqlite3_column_int(stmt, 3);
            int sql_o_totalprice = sqlite3_column_int(stmt, 4);
            int sql_sum_quantity = sqlite3_column_int(stmt, 5);

            if (pid == 0) {
                // Print results
                // std::cout << "c_name: " << sql_c_name << " | " << "c_custkey: " << sql_c_custkey << " | " 
                //           << "o_orderkey: " << sql_o_orderkey << " | " << "o_orderdate: " << sql_o_orderdate << " | "
                //           << "o_totalprice: " << sql_o_totalprice << " | " << "sum_quantity: " << sql_sum_quantity << std::endl;

                // std::cout << "c_name: " << c_name[i] << " | " << "c_custkey: " << c_custkey[i] << " | "
                //           << "o_orderkey: " << o_orderkey[i] << " | " << "o_orderdate: " << o_orderdate[i] << " | " 
                //           << "o_totalprice: " << o_totalprice[i] << " | " << "sum_quantity: " << sum_quantity[i] << std::endl;

                // std::cout << std::endl;

                ASSERT_SAME(c_name[i], sql_c_name);
                ASSERT_SAME(c_custkey[i], sql_c_custkey);
                ASSERT_SAME(o_orderkey[i], sql_o_orderkey);
                ASSERT_SAME(o_orderdate[i], sql_o_orderdate);
                ASSERT_SAME(o_totalprice[i], sql_o_totalprice);
                ASSERT_SAME(sum_quantity[i], sql_sum_quantity);
            }
            ++i;
        }
        ASSERT_SAME(i, c_name.size());
        if (i == 0) {single_cout("Empty result");}

        if (ret != SQLITE_DONE) {
            single_cout("Error executing statement: " << sqlite3_errmsg(sqlite_db));
        }
    }
#endif
    // Close SQLite DB
    sqlite3_close(sqlite_db);

    MPI_Finalize();

    return 0;
}
