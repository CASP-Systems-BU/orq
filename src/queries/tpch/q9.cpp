/**
 * @file q9.cpp
 * @brief TPCH Query 9
 * 
 * Equivalent SQL:
 * select
 *     nation,
 *     o_year,
 *     sum(amount) as sum_profit
 * from (
 *     select
 *         n_name as nation,
 *         extract(year from o_orderdate) as o_year,
 *         l_extendedprice * (1 - l_discount) - ps_supplycost * l_quantity as amount
 *     from
 *         part,
 *         supplier,
 *         lineitem,
 *         partsupp,
 *         orders,
 *         nation
 *     where
 *         s_suppkey = l_suppkey
 *         and ps_suppkey = l_suppkey
 *         and ps_partkey = l_partkey
 *         and p_partkey = l_partkey
 *         and o_orderkey = l_orderkey
 *         and s_nationkey = n_nationkey
 *         and p_name like '%[COLOR]%'
 *     ) as profit
 * group by
 *     nation,
 *     o_year
 * order by
 *     nation,
 *     o_year desc;
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

    // TPCH Q9 query parameters
    const int P_NAME_COLOR = 1;  // Substitution for p_name like '%[COLOR]%'

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

    single_cout("Q9 SF " << db.scaleFactor);

    auto Part = db.getPartTable();
    auto Supplier = db.getSupplierTable();
    auto LineItem = db.getLineitemTable();
    auto PartSupp = db.getPartSuppTable();
    auto Orders = db.getOrdersTable();
    auto Nation = db.getNationTable();

    Part.project({"[Name]", "[PartKey]"});
    Supplier.project({"[NationKey]", "[SuppKey]"});
    LineItem.project({"[SuppKey]", "[PartKey]", "[OrderKey]",
                      "ExtendedPrice", "Discount", "Quantity"});
    PartSupp.project({"[PartKey]", "[SuppKey]", "SupplyCost"});
    Orders.project({"[OrderKey]", "[OrderDate]"});
    Nation.project({"[NationKey]", "[Name]"});

#ifdef PRINT_TABLES
    single_cout("Part size: " << db.partSize());
    print_table(Part.open_with_schema(), pid);

    single_cout("Supplier size: " << db.supplierSize());
    print_table(Supplier.open_with_schema(), pid);

    single_cout("LineItem size: " << db.lineitemsSize());
    print_table(LineItem.open_with_schema(), pid);

    single_cout("PartSupp size: " << db.partSuppSize());
    print_table(PartSupp.open_with_schema(), pid);

    single_cout("Orders size: " << db.ordersSize());
    print_table(Orders.open_with_schema(), pid);

    single_cout("Nation size: " << db.nationSize());
    print_table(Nation.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    Part.filter(Part["[Name]"] == P_NAME_COLOR);

    stopwatch::timepoint("Part Filter");

    Supplier.addColumns({"[NationName]"}, Supplier.size());
    auto SuppliersJoin = Nation.inner_join(Supplier, {"[NationKey]"}, {
        {"[Name]", "[NationName]", copy<B>}
    });

    Supplier.deleteTable();

    stopwatch::timepoint("NationKey Join");

    auto LineItemSuppKeyJoin = SuppliersJoin.inner_join(LineItem, {"[SuppKey]"}, {
        {"[NationName]", "[NationName]", copy<B>}
    });


    SuppliersJoin.deleteTable();
    LineItem.deleteTable();

    stopwatch::timepoint("SuppKey Join");

    auto LineItemPartKeyJoin = Part.inner_join(LineItemSuppKeyJoin, {"[PartKey]"}, {});

    Part.deleteTable();
    LineItemSuppKeyJoin.deleteTable();
    stopwatch::timepoint("PartKey Join");

    auto LineItemOrderKeyJoin = Orders.inner_join(LineItemPartKeyJoin, {"[OrderKey]"}, {
        {"[OrderDate]", "[OrderDate]", copy<B>}
    });

    Orders.deleteTable();
    LineItemPartKeyJoin.deleteTable();
    stopwatch::timepoint("OrderKey Join");

    auto FinalJoin = PartSupp.inner_join(LineItemOrderKeyJoin, {"[PartKey]", "[SuppKey]"}, {
        {"SupplyCost", "SupplyCost", copy<A>}
    });

    PartSupp.deleteTable();
    LineItemOrderKeyJoin.deleteTable();

    stopwatch::timepoint("PartSupp Join");

    FinalJoin.project({
        "[NationName]", "[OrderDate]", "ExtendedPrice",
        "Discount", "Quantity", "SupplyCost"
    });

    FinalJoin.addColumns({"Amount", "SumProfit"}, FinalJoin.size());
    
    // Using a version without the PartSupp table
    FinalJoin["Amount"] = (FinalJoin["ExtendedPrice"] * (-FinalJoin["Discount"] + 100) / 100) - FinalJoin["SupplyCost"] * FinalJoin["Quantity"];

    stopwatch::timepoint("Calculate amount");

    FinalJoin.aggregate({ENC_TABLE_VALID, "[NationName]", "[OrderDate]"}, {
        {"Amount", "SumProfit", sum<A>}
    });

    stopwatch::timepoint("Group by + Sum");
    
    FinalJoin.sort({
        std::make_pair("[NationName]", ASC),
        std::make_pair("[OrderDate]", DESC)
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
    auto nation = FinalJoin.get_column(result, "[NationName]");
    auto o_year = FinalJoin.get_column(result, "[OrderDate]");
    auto sum_profit = FinalJoin.get_column(result, "SumProfit");


    if (pid == 0) {
        // Fetch Q9 SQL result to validate
        // Note: The "extract" operation is unnecessary here, 
        // given the integer representation of OrderDate
        int ret;
        const char* query = R"sql(
            select
                nation,
                o_year,
                sum(amount) as sum_profit
            from (
                select
                    n.Name as nation,
                    o.OrderDate as o_year,
                    (l.ExtendedPrice * (100 - l.Discount) / 100) - ps.supplycost * l.quantity as amount
                from
                    part as p,
                    supplier as s,
                    lineitem as l,
                    partsupp as ps,
                    orders as o,
                    nation as n
                where
                    s.SuppKey = l.SuppKey
                    and ps.SuppKey = l.SuppKey
                    and ps.PartKey = l.PartKey
                    and p.PartKey = l.PartKey
                    and o.OrderKey = l.OrderKey
                    and s.NationKey = n.NationKey
                    and p.Name = ?
                ) as profit
            group by
                nation,
                o_year
            order by
                nation,
                o_year desc;
        )sql";

        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, P_NAME_COLOR);

        int i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            int sqlNation = sqlite3_column_int(stmt, 0);
            int sqlYear = sqlite3_column_int(stmt, 1);
            int sqlProfit = sqlite3_column_int(stmt, 2);

            // std::cout << "nation: " << sqlNation << " | " << "o_year: " << sqlYear << " | " << "sum_profit: " << sqlProfit << std::endl;
            // std::cout << "nation: " << nation[i] << " | " << "o_year: " << o_year[i] << " | " << "sum_profit: " << sum_profit[i] << std::endl;
            // std::cout << std::endl;

            assert(i < nation.size());

            ASSERT_SAME(nation[i], sqlNation);
            ASSERT_SAME(o_year[i], sqlYear);
            ASSERT_SAME(sum_profit[i], sqlProfit);
            i++;
        }
        ASSERT_SAME(i, nation.size());
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
