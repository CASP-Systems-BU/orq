/**
 * @file q8.cpp
 * @brief Implements TPCH Query 8
 * 
 * Equivalent SQL:
 *  select
 *  o_year,
 *      sum(case
 *          when nation = '[NATION]'
 *          then volume
 *          else 0
 *      end) / sum(volume) as mkt_share
 *  from (
 *          select
 *              extract(year from o_orderdate) as o_year,
 *              l_extendedprice * (1-l_discount) as volume,
 *              n2.n_name as nation
 *          from
 *              part,
 *              supplier,
 *              lineitem,
 *              orders,
 *              customer,
 *              nation n1,
 *              nation n2,
 *              region
 *          where
 *              p_partkey = l_partkey
 *              and s_suppkey = l_suppkey
 *              and l_orderkey = o_orderkey
 *              and o_custkey = c_custkey
 *              and c_nationkey = n1.n_nationkey
 *              and n1.n_regionkey = r_regionkey
 *              and r_name = '[REGION]'
 *              and s_nationkey = n2.n_nationkey
 *              and o_orderdate between date '1995-01-01' and date '1996-12-31'
 *              and p_type = '[TYPE]'
 *      ) as all_nations
 *  group by
 *      o_year
 *  order by
 *      o_year;
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

    // TPCH Q8 query parameters
    const int DATE_START = 100; // arbitrary dates such that start < end
    const int DATE_END = 110;
    const int DATE_MID = 105;
    const int NATION_NAME = 9;       // arbitrary
    const int REGION_NAME = 7;  // arbitrary
    const int PART_TYPE = 4;    // arbitrary
    
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
    single_cout("Q8 SF " << db.scaleFactor);

    auto Part = db.getPartTable();
    auto Supplier = db.getSupplierTable();
    auto LineItem = db.getLineitemTable();
    auto Orders = db.getOrdersTable();
    auto Customer = db.getCustomersTable();
    auto Nation1 = db.getNationTable();
    auto Nation2 = Nation1.deepcopy();
    auto Region = db.getRegionTable();

    // project tables
    Part.project({"[PartKey]", "[Type]"});
    Supplier.project({"[SuppKey]", "[NationKey]"});
    LineItem.project({"ExtendedPrice", "Discount", "[PartKey]", "[SuppKey]", "[OrderKey]"});
    Orders.project({"[OrderKey]", "[CustKey]", "[OrderDate]"});
    Customer.project({"[CustKey]", "[NationKey]"});
    Nation1.project({"[NationKey]", "[RegionKey]", "[Name]"});
    Nation2.project({"[NationKey]", "[RegionKey]", "[Name]"});
    Region.project({"[RegionKey]", "[Name]"});

    stopwatch::timepoint("Start");
    stopwatch::profile_init();


    // filter by part type
    Part.filter(Part["[Type]"] == PART_TYPE);
    Part.deleteColumns({"[Type]"});

    // filter by region name
    Region.filter(Region["[Name]"] == REGION_NAME);
    Region.deleteColumns({"[Name]"});

    // filter orders for orderdate range
    Orders.filter(Orders["[OrderDate]"] >= DATE_START);
    Orders.filter(Orders["[OrderDate]"] < DATE_END);
    // don't delete the column because it will be necessary later

    // extract year from orderdate
    // since there are only two options, split down the middle of the date range
    Orders.addColumns({"[Year]"}, Orders.size());
    Orders["[Year]"] = Orders["[OrderDate]"] >= DATE_MID;
    Orders.deleteColumns({"[OrderDate]"});
    

    stopwatch::timepoint("filter");


    LineItem.addColumns({"Volume"}, LineItem.size());
    LineItem["Volume"] = LineItem["ExtendedPrice"] * (-LineItem["Discount"] + 100) / 100;
    LineItem.deleteColumns({"ExtendedPrice", "Discount"});

    stopwatch::timepoint("calculate volume");


    auto PartLineItemJoin = Part.inner_join(LineItem, {"[PartKey]"}, {
    });
    PartLineItemJoin.deleteColumns({"[PartKey]"});
    
    auto OrderPartLineItemJoin = Orders.inner_join(PartLineItemJoin, {"[OrderKey]"}, {
        {"[CustKey]", "[CustKey]", copy<B>},
        {"[Year]", "[Year]", copy<B>}
    });
    OrderPartLineItemJoin.deleteColumns({"[OrderKey]"});

    auto CustomerOrderPartLineItemJoin = Customer.inner_join(OrderPartLineItemJoin, {"[CustKey]"}, {
        {"[NationKey]", "[NationKey]", copy<B>}
    });
    CustomerOrderPartLineItemJoin.deleteColumns({"[CustKey]"});

    auto N1CustomerOrderPartLineItemJoin = Nation1.inner_join(CustomerOrderPartLineItemJoin, {"[NationKey]"}, {
        {"[RegionKey]", "[RegionKey]", copy<B>}
    });
    N1CustomerOrderPartLineItemJoin.deleteColumns({"[NationKey]"});

    auto RegN1CustomerOrderPartLineItemJoin = Region.inner_join(N1CustomerOrderPartLineItemJoin, {"[RegionKey]"}, {
    });
    RegN1CustomerOrderPartLineItemJoin.deleteColumns({"[RegionKey]"});

    auto SupplierRegN1CustomerOrderPartLineItemJoin = Supplier.inner_join(RegN1CustomerOrderPartLineItemJoin, {"[SuppKey]"}, {
        {"[NationKey]", "[NationKey]", copy<B>}
    });
    SupplierRegN1CustomerOrderPartLineItemJoin.deleteColumns({"[SuppKey]"});

    auto FinalJoin = Nation2.inner_join(SupplierRegN1CustomerOrderPartLineItemJoin, {"[NationKey]"}, {
        {"[Name]", "[Name]", copy<B>}
    });

    stopwatch::timepoint("join");


    // compute a flag for whether the nation matches, then filter Volume based on the flag
    FinalJoin.addColumns({"[ValidNation]", "ValidNation", "VolumeFiltered"});

    FinalJoin["[ValidNation]"] = (FinalJoin["[Name]"] == NATION_NAME);
    FinalJoin.convert_b2a_bit("[ValidNation]", "ValidNation");

    FinalJoin["VolumeFiltered"] = FinalJoin["Volume"] * FinalJoin["ValidNation"];
    
    stopwatch::timepoint("post-join computation");

    FinalJoin.addColumns({"VolumeSum", "VolumeFilteredSum"});
    // group by and aggregate
    auto result = FinalJoin.aggregate(
        {ENC_TABLE_VALID, "[Year]"},
        {{"Volume", "VolumeSum", sum<A>}, {"VolumeFiltered", "VolumeFilteredSum", sum<A>}} 
    );

    // Final Join is now at most the length of Orders, because we are grouping
    // by an attribute from that table. This will help limit the size of the
    // private division we need to do below.
    result.sort({ENC_TABLE_VALID});
    result.tail(Orders.size());

    stopwatch::timepoint("aggregation");

    // compute the percentage of volume that passes the filter (the market share)
    // convert volumes to binary and scale it by 100 because we're using percentages
    result.addColumns({"[MarketShare]"});
    result["VolumeFilteredSum"] = result["VolumeFilteredSum"] * 100;
    // PRIVATE DIVISION
    result["[MarketShare]"] = result["VolumeFilteredSum"] / result["VolumeSum"];

    stopwatch::timepoint("private division");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    result.finalize();
#endif

    stopwatch::done(); // print wall clock time
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();
    
#ifndef QUERY_PROFILE

    auto resultOpened = result.open_with_schema();
    auto year_col = result.get_column(resultOpened, "[Year]");
    auto market_share_col = result.get_column(resultOpened, "[MarketShare]");

    // SQL validation
    if (pid == 0) {
        // Run Q8 through SQL to verify result
        int ret;
        const char* query = R"sql(
            select
                o_year,
                sum(case
                    when nation = ?
                    then (100 * volume)
                    else 0
                end) / sum(volume) as mkt_share
            from (
                select
                    (o.OrderDate >= ?) as o_year,
                    (l.ExtendedPrice * (100 - l.Discount) / 100) as volume,
                    n2.Name as nation
                from
                    PART as p,
                    SUPPLIER as s,
                    LINEITEM as l,
                    ORDERS as o,
                    CUSTOMER as c,
                    NATION as n1,
                    NATION as n2,
                    REGION as r
                where
                    p.PartKey = l.PartKey
                    and s.SuppKey = l.SuppKey
                    and l.OrderKey = o.OrderKey
                    and o.CustKey = c.CustKey
                    and c.NationKey = n1.NationKey
                    and n1.RegionKey = r.RegionKey
                    and r.Name = ?
                    and s.NationKey = n2.NationKey
                    and o.OrderDate >= ?
                    and o.OrderDate < ?
                    and p.Type = ?
                ) as all_nations
            group by
                o_year
            order by
                o_year;
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        if (ret != SQLITE_OK) {
            printf("SQL error: %s\n", sqlite3_errmsg(sqlite_db));
        }
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, NATION_NAME);
        sqlite3_bind_int(stmt, 2, DATE_MID);
        sqlite3_bind_int(stmt, 3, REGION_NAME);
        sqlite3_bind_int(stmt, 4, DATE_START);
        sqlite3_bind_int(stmt, 5, DATE_END);
        sqlite3_bind_int(stmt, 6, PART_TYPE);

        // Assert result against SQL result
        auto res = sqlite3_step(stmt);
        auto i = 0;
        while (res == SQLITE_ROW) {
            int sql_year = sqlite3_column_int(stmt, 0);
            int sql_market_share = sqlite3_column_int(stmt, 1);
            
            ASSERT_SAME (sql_year, year_col[i]);
            ASSERT_SAME (sql_market_share, market_share_col[i]);

            res = sqlite3_step(stmt);
            ++i;
        }
        ASSERT_SAME(i, year_col.size());
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