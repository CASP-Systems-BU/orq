/**
 * @file q15.cpp
 * @brief Implements TPCH Query 15
 * @date 2024-08-6
 *
 * Equivalent SQL:
 *   create view revenue[STREAM_ID] (supplier_no, total_revenue) as
 *      select
 *          l_suppkey,
 *          sum(l_extendedprice * (1 - l_discount))
 *      from
 *          lineitem
 *      where
 *          l_shipdate >= date '[DATE]'
 *          and l_shipdate < date '[DATE]' + interval '3' month
 *      group by
 *          l_suppkey;
 *
 *   select
 *      s_suppkey,
 *      s_name,
 *      s_address,
 *      s_phone,
 *      total_revenue
 *   from
 *      supplier,
 *      revenue[STREAM_ID]
 *   where
 *      s_suppkey = supplier_no
 *      and total_revenue = (
 *          select
 *              max(total_revenue)
 *          from
 *              revenue[STREAM_ID]
 *          )
 *   order by
 *          s_suppkey;
 *
 *   drop view revenue[STREAM_ID];
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

int main(int argc, char **argv) {
    orq_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    // TPCH Q15 query parameters
    const int DATE = 80;
    const int DATE_INTERVAL = 20;  // Arbitrary date interval to account for date format

    // Setup SQLite DB for output validation
    sqlite3 *sqlite_db = nullptr;
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

    single_cout("Q15 SF " << db.scaleFactor);

    ////////////////////////////////////////////////////////////////
    // Query

    auto LineItem = db.getLineitemTable();
    auto Supplier = db.getSupplierTable();

    LineItem.project({"[SuppKey]", "ExtendedPrice", "Discount", "[ShipDate]"});
    Supplier.project({"[SuppKey]", "[Name]", "[Address]", "[Phone]"});

#ifdef PRINT_TABLES
    single_cout("LINEITEM, size " << db.lineitemsSize());
    print_table(LineItem.open_with_schema(), pid);

    single_cout("Suppliers, size " << db.supplierSize());
    print_table(Supplier.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // l_shipdate >= date '[DATE]'
    LineItem.filter(LineItem["[ShipDate]"] >= DATE);
    // l_shipdate < date '[DATE]' + interval '3' month
    LineItem.filter(LineItem["[ShipDate]"] < DATE + DATE_INTERVAL);
    LineItem.addColumns({"Revenue", "TotalRevenue"});
    // Compute l_extendedprice * (1 - l_discount) and store it into a new column 'Revenue'
    // NOTE: ((-LineItem["Discount"] + 100 ) / 100 is needed because we don't support floats
    // NOTE: Make sure we apply the multiplication first, otherwise division truncates the result
    LineItem["Revenue"] = (LineItem["ExtendedPrice"] * (-LineItem["Discount"] + 100)) / 100;
    // SELECT l_suppkey, sum(l_extendedprice * (1 - l_discount)) GROUP BY l_suppkey
    LineItem.aggregate({"[SuppKey]"}, {{"Revenue", "TotalRevenue", sum<A>}});
    // Convert arithmetic column to boolean for using MAX below
    LineItem.addColumns({"[TotalRevenue]"});
    LineItem.convert_a2b("TotalRevenue", "[TotalRevenue]");
    // Remove unnecessary columns
    LineItem.project({"[SuppKey]", "[TotalRevenue]"});

    stopwatch::timepoint("View");

    // SELECT max(total_revenue) FROM revenue[STREAM_ID]
    LineItem.sort({ENC_TABLE_VALID, "[TotalRevenue]"}, ASC);
    // Add new column for global MAX
    LineItem.addColumns({"[MaxTotalRevenue]"});

    // 'MaxTotalRevenue' value is now in the last row of 'MaxRevenue'. Extract it and replicate it
    // in all rows 1- Extract 'MaxTotalRevenue' value
    B extracted(1);
    extracted = ((B *)LineItem["[TotalRevenue]"].contents.get())
                    ->simple_subset_reference(LineItem.size() - 1, 1, LineItem.size() - 1);
    // 2- Replicate it in all rows of 'MaxRevenue'
    *((B *)LineItem["[MaxTotalRevenue]"].contents.get()) =
        extracted.repeated_subset_reference(LineItem.size());

    // Apply filter total_revenue = ( SELECT max(total_revenue) FROM revenue[STREAM_ID] )
    LineItem.filter(LineItem["[TotalRevenue]"] == LineItem["[MaxTotalRevenue]"]);
    // Remove unnecessary columns
    LineItem.deleteColumns({"[MaxTotalRevenue]"});

    // s_suppkey = supplier_no (PK - FK)
    auto SupLine = Supplier.inner_join(LineItem, {"[SuppKey]"},
                                       {{"[Name]", "[Name]", copy<B>},
                                        {"[Address]", "[Address]", copy<B>},
                                        {"[Phone]", "[Phone]", copy<B>}});

    stopwatch::timepoint("Join");

    // order by s_suppkey;
    SupLine.sort({"[SuppKey]"}, ASC);

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    SupLine.finalize();
#endif

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    ////////////////////////////////////////////////////////////////
    // Correctness Test

#ifndef QUERY_PROFILE

    // Fetch result
    auto resultOpened = SupLine.open_with_schema();
    // print_table(resultOpened, pid);

    auto supKey_col = SupLine.get_column(resultOpened, "[SuppKey]");
    auto supName_col = SupLine.get_column(resultOpened, "[Name]");
    auto supAdd_col = SupLine.get_column(resultOpened, "[Address]");
    auto supPhone_col = SupLine.get_column(resultOpened, "[Phone]");
    auto totalRev_col = SupLine.get_column(resultOpened, "[TotalRevenue]");

    if (pid == 0) {
        // Run Q15 through SQL to verify result
        int ret;
        const char *query = R"sql(

            with revenue (supplier_no, total_revenue) as (
                select
                    SuppKey,
                    sum((ExtendedPrice * (100 - Discount)) / 100)
                from
                    lineitem
                where
                    ShipDate >= ? 
                    and ShipDate < ? + ?
                group by
                    SuppKey
            )
            select
                s.SuppKey,
                s.Name,
                s.Address,
                s.Phone,
                total_revenue
            from
                supplier as s,
                revenue
            where
                s.SuppKey = supplier_no
                and total_revenue = (
                    select
                        max(total_revenue)
                    from
                        revenue
                )
            order by
                s.SuppKey;
        )sql";
        sqlite3_stmt *stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, DATE);
        sqlite3_bind_int(stmt, 2, DATE);
        sqlite3_bind_int(stmt, 3, DATE_INTERVAL);

        if (ret) {
            auto errmsg = sqlite3_errmsg(sqlite_db);
            throw std::runtime_error(errmsg);
        }
        // Assert result against SQL result
        int idx = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            int supKey = sqlite3_column_int(stmt, 0);
            int supName = sqlite3_column_int(stmt, 1);
            int supAdd = sqlite3_column_int(stmt, 2);
            int supPhone = sqlite3_column_int(stmt, 3);
            int total_rev = sqlite3_column_int(stmt, 4);

            // std::cout << supKey << " " << supName << " " << supAdd << " " ;
            // std::cout << supPhone << " " << total_rev << std::endl;
            ASSERT_SAME(supKey, supKey_col[idx]);
            ASSERT_SAME(supName, supName_col[idx]);
            ASSERT_SAME(supAdd, supAdd_col[idx]);
            ASSERT_SAME(supPhone, supPhone_col[idx]);
            ASSERT_SAME(total_rev, totalRev_col[idx]);

            idx++;
        }
        ASSERT_SAME(idx, supKey_col.size());
        if (idx == 0) {
            single_cout("Empty result");
        }

        if (ret != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }

        single_cout(idx << " rows OK");
    }

#endif
    // Close SQLite DB
    // TODO: Only party 0 should open and close the db and only when TPCH_PROFILE is defined
    sqlite3_close(sqlite_db);

    return 0;
}
