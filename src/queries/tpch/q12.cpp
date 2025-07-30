/**
 * @file q12.cpp
 * @brief Implement TPCH query 12
 * @date 2024-08-05
 * 
 * Equivalent SQL:
 * 
 * select
 *      l_shipmode,
 *      sum(case
 *          when o_orderpriority ='1-URGENT'
 *          or o_orderpriority ='2-HIGH'
 *          then 1
 *          else 0
 *      end) as high_line_count,
 *      sum(case
 *          when o_orderpriority <> '1-URGENT'
 *          and o_orderpriority <> '2-HIGH'
 *          then 1
 *          else 0
 *      end) as low_line_count
 *  from
 *      orders,
 *      lineitem
 *  where
 *      o_orderkey = l_orderkey
 *      and l_shipmode in ('[SHIPMODE1]', '[SHIPMODE2]')
 *      and l_commitdate < l_receiptdate
 *      and l_shipdate < l_commitdate
 *      and l_receiptdate >= date '[DATE]'
 *      and l_receiptdate < date '[DATE]' + interval '1' year
 *  group by
 *      l_shipmode
 *  order by
 *      l_shipmode;
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

using T = int64_t;

int main(int argc, char ** argv) {
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    // arbitrary
    const int SHIPMODE1 = 3;
    const int SHIPMODE2 = 4;
    const int DATE_INPUT = 15;

    sqlite3 *sqlite_db = nullptr;
#ifndef QUERY_PROFILE
    if (pid == 0) {
        int err = sqlite3_open(NULL, &sqlite_db);
        if(err) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
    }
#endif

    auto db = TPCDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    single_cout("Q12 SF " << db.scaleFactor);

    auto L = db.getLineitemTable();
    auto O = db.getOrdersTable();

    L.project({"[ShipMode]", "[CommitDate]", "[ReceiptDate]", "[ShipDate]", "[OrderKey]"});
    O.project({"[OrderPriority]", "[OrderKey]"});

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // SQL: l_shipmode in ('[SHIPMODE1]', '[SHIPMODE2]')
    L.filter((L["[ShipMode]"] == SHIPMODE1) | (L["[ShipMode]"] == SHIPMODE2));
    L.filter(L["[CommitDate]"] < L["[ReceiptDate]"]);
    L.filter(L["[ShipDate]"] < L["[CommitDate]"]);
    L.filter(L["[ReceiptDate]"] >= DATE_INPUT);
    L.filter(L["[ReceiptDate]"] < DATE_INPUT + 365);

    L.deleteColumns({"[CommitDate]", "[ReceiptDate]", "[ShipDate]"});

    stopwatch::timepoint("Filter");

    O.addColumns({"[HighLine]"}, O.size());

    O["[HighLine]"] = (O["[OrderPriority]"] == 1) | (O["[OrderPriority]"] == 2);

    O.deleteColumns({"[OrderPriority]"});

    auto T = O.inner_join(L, {"[OrderKey]"}, {
        {"[HighLine]", "[HighLine]", copy<B>},
    });

    T.deleteColumns({"[OrderKey]"});

    stopwatch::timepoint("Join");

    T.addColumns({"LineCount"}, T.size());

    T.aggregate({ENC_TABLE_VALID, "[ShipMode]", "[HighLine]"}, {
        {"LineCount", "LineCount", count<A>},
    });

    stopwatch::timepoint("Aggregation");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    T.finalize();
#endif

    stopwatch::done(); // print wall clock time
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    auto resultTable = T.open_with_schema();

    if (pid == 0) {
        print_table(resultTable, pid);

        auto counts = T.get_column(resultTable, "LineCount");
        auto line = T.get_column(resultTable, "[HighLine]");
        auto mode = T.get_column(resultTable, "[ShipMode]");

        std::map<std::pair<int, int>, int> check;
        for(int i = 0; i < counts.size(); i++) {
            check[{mode[i], line[i]}] = counts[i];
        }

        const char * query = R"sql(
        select
            L.shipmode,
            sum(case
                when O.orderpriority = 1
                or O.orderpriority = 2
                then 1
                else 0
            end) as high_line_count,
            sum(case
                when O.orderpriority <> 1
                and O.orderpriority <> 2
                then 1
                else 0
            end) as low_line_count
        from
            orders as O,
            lineitem as L
        where
            O.orderkey = L.orderkey
            and L.shipmode in (?, ?)
            and L.commitdate < L.receiptdate
            and L.shipdate < L.commitdate
            and L.receiptdate >= ?
            and L.receiptdate < ? + 365
        group by
            L.shipmode
        order by
            L.shipmode;
        )sql";

        sqlite3_stmt * stmt;
        auto ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        sqlite3_bind_int(stmt, 1, SHIPMODE1);
        sqlite3_bind_int(stmt, 2, SHIPMODE2);
        sqlite3_bind_int(stmt, 3, DATE_INPUT);
        sqlite3_bind_int(stmt, 4, DATE_INPUT);

        int i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            auto shipMode = sqlite3_column_int(stmt, 0);
            auto highCount = sqlite3_column_int(stmt, 1);
            auto lowCount = sqlite3_column_int(stmt, 2);

            assert((check[{shipMode, 0}] == lowCount));
            assert((check[{shipMode, 1}] == highCount));
            i++;
        }
        ASSERT_SAME(i, (mode.size())/2);  // Output format slightly different, hence the division by 2
        if (i == 0) {single_cout("Empty result");}

        if (ret != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }

        single_cout("SQL correctness check OK");
    }

#endif
    sqlite3_close(sqlite_db);
    
    MPI_Finalize();
}