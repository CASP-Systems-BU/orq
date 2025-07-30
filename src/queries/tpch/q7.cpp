/**
 * @file q7.cpp
 * @brief Implements TPCH Query 7
 * 
 * Equivalent SQL:
 *  select
 *      supp_nation,
 *      cust_nation,
 *      l_year, sum(volume) as revenue
 *  from (
 *          select
 *              n1.n_name as supp_nation,
 *              n2.n_name as cust_nation,
 *              extract(year from l_shipdate) as l_year,
 *              l_extendedprice * (1 - l_discount) as volume
 *          from
 *              supplier,
 *              lineitem,
 *              orders,
 *              customer,
 *              nation n1,
 *              nation n2
 *          where
 *              s_suppkey = l_suppkey
 *              and o_orderkey = l_orderkey
 *              and c_custkey = o_custkey
 *              and s_nationkey = n1.n_nationkey
 *              and c_nationkey = n2.n_nationkey
 *              and (
 *                  (n1.n_name = '[NATION1]' and n2.n_name = '[NATION2]')
 *                  or (n1.n_name = '[NATION2]' and n2.n_name = '[NATION1]')
 *              )
 *              and l_shipdate between date '1995-01-01' and date '1996-12-31'
 *      ) as shipping
 *  group by
 *      supp_nation,
 *      cust_nation,
 *      l_year
 *  order by
 *      supp_nation,
 *      cust_nation,
 *      l_year;
 * 
 */

#include "../../../include/secrecy.h"
#include "../../../include/benchmark/stopwatch.h"

#include "tpch_dbgen.h"
#include <stdlib.h>     /* atof */

// #define PRINT_TABLES

// #define QUERY_PROFILE

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using T = int64_t;

using namespace secrecy::aggregators;
using namespace secrecy::benchmarking;

using A = ASharedVector<T>;
using B = BSharedVector<T>;


int main(int argc, char** argv) {
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

    // TPCH Q7 query parameters
    const int DATE_START = 100; // arbitrary dates such that start < end
    const int DATE_END = 110;
    const int DATE_MID = 105;
    const int NATION_1 = 9;  // arbitrary nation name
    const int NATION_2 = 31; // arbitrary nation name
    
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

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    auto db = TPCDatabase<T>(sf, sqlite_db);
    single_cout("Q7 SF " << db.scaleFactor);

    // get tables
    auto Supplier = db.getSupplierTable();
    auto LineItem = db.getLineitemTable();
    auto Orders = db.getOrdersTable();
    auto Customer = db.getCustomersTable();
    auto Nation1 = db.getNationTable();
    auto Nation2 = Nation1.deepcopy();

    // project tables
    Supplier.project({"[SuppKey]", "[NationKey]"});
    LineItem.project({"[ShipDate]", "ExtendedPrice", "Discount", "[SuppKey]", "[OrderKey]"});
    Orders.project({"[OrderKey]", "[CustKey]"});
    Customer.project({"[CustKey]", "[NationKey]"});
    Nation1.project({"[NationKey]", "[Name]"});
    Nation2.project({"[NationKey]", "[Name]"});

    // rename columns to avoid duplicates
    Nation1.renameColumn("[Name]", "[N1_Name]");
    Nation2.renameColumn("[Name]", "[N2_Name]");

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // filter lineitem for shipdate range
    LineItem.filter(LineItem["[ShipDate]"] >= DATE_START);
    LineItem.filter(LineItem["[ShipDate]"] < DATE_END);

    // extract year from shipdate
    // since there are only two options, split down the middle of the date range
    LineItem.addColumns({"[Year]"}, LineItem.size());
    LineItem["[Year]"] = LineItem["[ShipDate]"] >= DATE_MID;
    LineItem.deleteColumns({"[ShipDate]"});

    stopwatch::timepoint("filter");

    LineItem.addColumns({"Volume"}, LineItem.size());
    LineItem["Volume"] = LineItem["ExtendedPrice"] * (-LineItem["Discount"] + 100) / 100;
    LineItem.deleteColumns({"ExtendedPrice", "Discount"});

    stopwatch::timepoint("calculate volume");

    auto SupplierLineItemJoin = Supplier.inner_join(LineItem, {"[SuppKey]"}, {
        {"[NationKey]", "[NationKey]", copy<B>}
    });
    
    auto OrderSupplierLineItemJoin = Orders.inner_join(SupplierLineItemJoin, {"[OrderKey]"}, {
        {"[CustKey]", "[CustKey]", copy<B>}
    });

    auto N1OrderSupplierLineItemJoin = Nation1.inner_join(OrderSupplierLineItemJoin, {"[NationKey]"}, {
        {"[N1_Name]", "[N1_Name]", copy<B>}
    });

    auto CustomerN1OrderSupplierLineItemJoin = Customer.inner_join(N1OrderSupplierLineItemJoin, {"[CustKey]"}, {
        {"[NationKey]", "[NationKey]", copy<B>}
    });
    
    auto FinalJoin = Nation2.inner_join(CustomerN1OrderSupplierLineItemJoin, {"[NationKey]"}, {
        {"[N2_Name]", "[N2_Name]", copy<B>}
    });

    stopwatch::timepoint("join");

    // prior series of joins, all of which are bounded by |LineItem|
    FinalJoin.sort({ENC_TABLE_VALID});
    FinalJoin.tail(LineItem.size());

    // filter by nation names
    FinalJoin.filter((FinalJoin["[N1_Name]"] == NATION_1) | (FinalJoin["[N1_Name]"] == NATION_2));
    FinalJoin.filter(((FinalJoin["[N2_Name]"] == NATION_1) | (FinalJoin["[N2_Name]"] == NATION_2)) & (FinalJoin["[N1_Name]"] != FinalJoin["[N2_Name]"]));

    stopwatch::timepoint("post-join nation filter");

    // group by and aggregate
    auto result = FinalJoin.aggregate(
        {ENC_TABLE_VALID, "[N1_Name]", "[N2_Name]", "[Year]"},
        {{"Volume", "Volume", sum<A>}} 
    );

    stopwatch::timepoint("aggregation");
    stopwatch::timepoint("--");

    // rename aggregated volume to revenue
    result.renameColumn("Volume", "Revenue");

    // No need to re-sort here: aggregation didn't change sort order, just
    // invalidated.

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    result.finalize();
#endif

    stopwatch::done(); // print wall clock time
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    result.project({"[N1_Name]", "[N2_Name]", "[Year]", "Revenue"});

    auto resultOpened = result.open_with_schema();
    auto supp_nation_col = result.get_column(resultOpened, "[N1_Name]");
    auto cust_nation_col = result.get_column(resultOpened, "[N2_Name]");
    auto year_col = result.get_column(resultOpened, "[Year]");
    auto revenue_col = result.get_column(resultOpened, "Revenue");

    // SQL validation
    if (pid == 0) {
        // Run Q7 through SQL to verify result
        int ret;
        const char* query = R"sql(
            select
                supp_nation,
                cust_nation,
                lYear,
                sum(volume) as revenue
            from (
                select
                    n1.Name as supp_nation,
                    n2.Name as cust_nation,
                    (l.ShipDate >= ?) as lYear,
                    (l.ExtendedPrice * (100 - l.Discount) / 100) as volume
                from
                    SUPPLIER as s,
                    LINEITEM as l,
                    ORDERS as o,
                    CUSTOMER as c,
                    NATION as n1,
                    NATION as n2
                where
                    s.SuppKey = l.SuppKey
                    and o.OrderKey = l.OrderKey
                    and c.CustKey = o.CustKey
                    and s.NationKey = n1.NationKey
                    and c.NationKey = n2.NationKey
                    and (
                        (n1.Name = ? and n2.Name = ?)
                        or (n1.Name = ? and n2.Name = ?)
                    )
                    and l.ShipDate >= ?
                    and l.ShipDate < ?
                ) as shipping
            group by
                supp_nation,
                cust_nation,
                lYear
            order by
                supp_nation,
                cust_nation,
                lYear;
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        if (ret != SQLITE_OK) {
            printf("SQL error: %s\n", sqlite3_errmsg(sqlite_db));
        }
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, DATE_MID);
        sqlite3_bind_int(stmt, 2, NATION_1);
        sqlite3_bind_int(stmt, 3, NATION_2);
        sqlite3_bind_int(stmt, 4, NATION_2);
        sqlite3_bind_int(stmt, 5, NATION_1);
        sqlite3_bind_int(stmt, 6, DATE_START);
        sqlite3_bind_int(stmt, 7, DATE_END);

        // Assert result against SQL result
        auto res = sqlite3_step(stmt);
        auto i = 0;
        while (res == SQLITE_ROW) {
            int sql_supp_nation = sqlite3_column_int(stmt, 0);
            int sql_cust_nation = sqlite3_column_int(stmt, 1);
            int sql_year = sqlite3_column_int(stmt, 2);
            int sql_revenue = sqlite3_column_int(stmt, 3);

            ASSERT_SAME (sql_supp_nation, supp_nation_col[i]);
            ASSERT_SAME (sql_cust_nation, cust_nation_col[i]);
            ASSERT_SAME (sql_year, year_col[i]);
            ASSERT_SAME (sql_revenue, revenue_col[i]);

            res = sqlite3_step(stmt);
            ++i;
        }
        ASSERT_SAME(i, supp_nation_col.size());
        if (i == 0) {single_cout("Empty result");}

        if (res == SQLITE_ERROR) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
        if (res != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
    }

#endif

    // Clean up
    if (sqlite_db) {
        sqlite3_close(sqlite_db);
    }

    MPI_Finalize();

    return 0;
}