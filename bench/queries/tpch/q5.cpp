/**
 * @file q5.cpp
 * @brief Implements TPCH Query 5
 * @date 2024-08-21
 *
 * Equivalent SQL:
 * select
 *     n_name,
 *     sum(l_extendedprice * (1 - l_discount)) as revenue
 * from
 *     customer,
 *     orders,
 *     lineitem,
 *     supplier,
 *     nation,
 *     region
 * where
 *     c_custkey = o_custkey
 *     and l_orderkey = o_orderkey
 *     and l_suppkey = s_suppkey
 *     and c_nationkey = s_nationkey
 *     and s_nationkey = n_nationkey
 *     and n_regionkey = r_regionkey
 *     and r_name = '[REGION]'
 *     and o_orderdate >= date '[DATE]'
 *     and o_orderdate < date '[DATE]' + interval '1' year
 * group by n_name
 * order by revenue desc;
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

    // TPCH Q5 query parameters
    const int DATE = 100;
    const int DATE_INTERVAL = 10;  // Arbitrary date interval to account for date format
    const int REGION = 1;

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

    single_cout("Q5 SF " << db.scaleFactor);

    ////////////////////////////////////////////////////////////////
    // Query

    auto Customer = db.getCustomersTable();
    auto Orders = db.getOrdersTable();
    auto Lineitem = db.getLineitemTable();
    auto Supplier = db.getSupplierTable();
    auto Nation = db.getNationTable();
    auto Region = db.getRegionTable();

    Customer.project({"[CustKey]", "[NationKey]"});
    Orders.project({"[OrderKey]", "[CustKey]", "[OrderDate]"});
    Lineitem.project({"[OrderKey]", "[LineNumber]", "[SuppKey]", "ExtendedPrice", "Discount"});
    Supplier.project({"[SuppKey]", "[NationKey]"});
    Nation.project({"[NationKey]", "[RegionKey]", "[Name]"});
    Region.project({"[RegionKey]", "[Name]"});

#ifdef PRINT_TABLES
    single_cout("CUSTOMER, size " << db.customersSize());
    print_table(Customer.open_with_schema(), pid);

    single_cout("ORDERS, size " << db.ordersSize());
    print_table(Orders.open_with_schema(), pid);

    single_cout("LINEITEM, size " << db.lineitemsSize());
    print_table(Lineitem.open_with_schema(), pid);

    single_cout("SUPPLIER, size " << db.supplierSize());
    print_table(Supplier.open_with_schema(), pid);

    single_cout("NATION, size " << db.nationSize());
    print_table(Nation.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // and o_orderdate >= date '[DATE]'
    // and o_orderdate < date '[DATE]' + interval '1' year
    Orders.filter(Orders["[OrderDate]"] >= DATE & Orders["[OrderDate]"] < (DATE + DATE_INTERVAL));
    Orders.deleteColumns({"[OrderDate]"});

    // and r_name = '[REGION]'
    Region.filter(Region["[Name]"] == REGION);

    // and n_regionkey = r_regionkey
    auto SelectedNations = Region.inner_join(Nation, {"[RegionKey]"});
    SelectedNations.project({"[NationKey]", "[Name]"});

    stopwatch::timepoint("Filters");

    Lineitem.addColumns({"Revenue"}, Lineitem.size());
    Lineitem["Revenue"] = Lineitem["ExtendedPrice"] * (-Lineitem["Discount"] + 100) / 100;
    Lineitem.deleteColumns({"ExtendedPrice", "Discount"});

    stopwatch::timepoint("Revenue");

    auto ItemsBySupplier =
        SelectedNations.inner_join(Supplier, {"[NationKey]"}, {{"[Name]", "[Name]", copy<B>}})
            .inner_join(Lineitem, {"[SuppKey]"},
                        {
                            {"[Name]", "[Name]", copy<B>},
                        });

    ItemsBySupplier.deleteColumns({"[NationKey]", "[SuppKey]"});
    Supplier.deleteTable();

    auto ItemsByCustomer = Nation
                               .inner_join(Customer, {"[NationKey]"},
                                           {
                                               {"[Name]", "[Name]", copy<B>},
                                           })
                               .inner_join(Orders, {"[CustKey]"},
                                           {
                                               {"[Name]", "[Name]", copy<B>},
                                           })
                               .inner_join(Lineitem, {"[OrderKey]"},
                                           {
                                               {"[Name]", "[Name]", copy<B>},
                                           });
    ItemsByCustomer.deleteColumns({"[NationKey]", "[CustKey]", "[SuppKey]"});

    // Collect garbage
    Customer.deleteTable();
    Orders.deleteTable();
    size_t L_size = Lineitem.size();
    Lineitem.deleteTable();

    auto FinalItems =
        ItemsByCustomer.inner_join(ItemsBySupplier, {"[OrderKey]", "[LineNumber]", "[Name]"}, {});
    FinalItems.deleteColumns({"[OrderKey]", "[LineNumber]"});

    ItemsByCustomer.deleteTable();
    ItemsBySupplier.deleteTable();

    stopwatch::timepoint("Joins");

    // sort on valid to move invalid columns to the bottom
    FinalItems.aggregate({"[Name]"}, {{"Revenue", "Revenue", sum<A>}});

    // Group-by over nation: at most |Nation| rows.
    FinalItems.sort({ENC_TABLE_VALID});
    FinalItems.tail(Nation.size());

    stopwatch::timepoint("Group");

    FinalItems.addColumns({"[Revenue]"}, FinalItems.size());
    FinalItems.convert_a2b("Revenue", "[Revenue]");
    FinalItems.deleteColumns({"Revenue"});

    FinalItems.sort({"[Revenue]"}, DESC);

    stopwatch::timepoint("Sort");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    FinalItems.finalize();
#endif

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    ////////////////////////////////////////////////////////////////
    // Correctness Test

#ifndef QUERY_PROFILE

    auto resultOpened = FinalItems.open_with_schema();
    auto nation_name_col = FinalItems.get_column(resultOpened, "[Name]");
    auto revenue_col = FinalItems.get_column(resultOpened, "[Revenue]");

    // print_table(resultOpened, pid);

    if (pid == 0) {
        // Run Q5 through SQL to verify result
        int ret;
        const char* query = R"sql(
            select
                n.Name,
                sum(l.ExtendedPrice * (100 - l.Discount) / 100) as Revenue
            from
                CUSTOMER as c,
                ORDERS as o,
                LINEITEM as l,
                SUPPLIER as s,
                NATION as n,
                REGION as r
            where
                c.CustKey = o.CustKey
                and l.OrderKey = o.OrderKey
                and l.SuppKey = s.SuppKey
                and c.NationKey = s.NationKey
                and s.NationKey = n.NationKey
                and n.RegionKey = r.RegionKey
                and r.Name = ?
                and o.OrderDate >= ?
                and o.OrderDate < ? + ?
            group by n.Name
            order by Revenue desc
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, REGION);
        sqlite3_bind_int(stmt, 2, DATE);
        sqlite3_bind_int(stmt, 3, DATE);
        sqlite3_bind_int(stmt, 4, DATE_INTERVAL);

        // Assert result against SQL result
        auto res = sqlite3_step(stmt);
        auto i = 0;
        while (res == SQLITE_ROW) {
            int sql_nation_name = sqlite3_column_int(stmt, 0);
            int sql_revenue = sqlite3_column_int(stmt, 1);

            ASSERT_SAME(sql_nation_name, nation_name_col[i]);
            ASSERT_SAME(sql_revenue, revenue_col[i]);

            res = sqlite3_step(stmt);
            ++i;
        }
        if (res == SQLITE_ERROR) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
        if (res != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
        ASSERT_SAME(i, nation_name_col.size());
        if (i == 0) {
            single_cout("Empty result");
        }

        std::cout << i << " rows OK\n";
    }

#endif
    // Close SQLite DB
    sqlite3_close(sqlite_db);

    return 0;
}
