/**
 * @file q1.cpp
 * @brief Implements TPCH Query 1
 *
 * Equivalent SQL:
 * select
 *     l_returnflag,
 *     l_linestatus,
 *     sum(l_quantity) as sum_qty,
 *     sum(l_extendedprice) as sum_base_price,
 *     sum(l_extendedprice*(1-l_discount)) as sum_disc_price,
 *     sum(l_extendedprice*(1-l_discount)*(1+l_tax)) as sum_charge,
 *     avg(l_quantity) as avg_qty,
 *     avg(l_extendedprice) as avg_price,
 *     avg(l_discount) as avg_disc,
 *     count(*) as count_order
 * from
 *     lineitem
 * where
 *     l_shipdate <= date '1998-12-01' - interval '[DELTA]' day (3)
 * group by
 *     l_returnflag,
 *     l_linestatus
 * order by
 *     l_returnflag,
 *     l_linestatus;
 *
 */

#include "../../../include/benchmark/stopwatch.h"
#include "../../../include/secrecy.h"
#include "tpch_dbgen.h"

// #define PRINT_TABLES

// #define QUERY_PROFILE

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using namespace secrecy::aggregators;
using namespace secrecy::benchmarking;

using T = int64_t;

using A = ASharedVector<T>;
using B = BSharedVector<T>;

const int SHIP_DATE = 60;

int main(int argc, char** argv) {
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    sqlite3* sqlite_db = nullptr;
#ifndef QUERY_PROFILE
    if (pid == 0) {
        int err = sqlite3_open(NULL, &sqlite_db);
        if (err) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
    }
#endif

    // TODO: command line argument for scale factor
    auto db = TPCDatabase<T>(sf, sqlite_db);
    single_cout("Q1 SF " << db.scaleFactor);

    auto L = db.getLineitemTable();

    L.project({"[ShipDate]", "[ReturnFlag]", "[LineStatus]", "ExtendedPrice", "Discount", "Tax",
               "Quantity"});

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // print_table(L.open_with_schema(), pid);

    std::vector<std::string> intermediate_columns = {
        "DiscPrice",   "Charge",        "SumDiscPrice", "SumCharge",     "CountOrder",
        "SumQuantity", "SumPrice",      "SumDisc",      "CountQuantity", "CountPrice",
        "CountDisc",   "[AvgQuantity]", "[AvgPrice]",   "[AvgDisc]",
    };

    // Arbitrary threshold - recent shipments
    L.filter(L["[ShipDate]"] <= SHIP_DATE);
    stopwatch::timepoint("filter");

    L.sort({ENC_TABLE_VALID, "[ReturnFlag]", "[LineStatus]"});
    stopwatch::timepoint("sort");

    L.addColumns(intermediate_columns, L.size());

    // Discount is a percentage... cheat with integer math + public division
    // Need to write (100 - Discount) like this because column/element ops only
    // support constant on the RHS.
    L["DiscPrice"] = L["ExtendedPrice"] * (-L["Discount"] + 100) / 100;

    // Same with tax
    L["Charge"] = L["DiscPrice"] * (L["Tax"] + 100) / 100;

    stopwatch::timepoint("interm. calc.");

    L.aggregate({ENC_TABLE_VALID, "[ReturnFlag]", "[LineStatus]"},
                {
                    // Aggregations for average:
                    {"Quantity", "SumQuantity", sum<A>},
                    {"Quantity", "CountQuantity", count<A>},
                    {"ExtendedPrice", "SumPrice", sum<A>},
                    {"ExtendedPrice", "CountPrice", count<A>},
                    {"Discount", "SumDisc", sum<A>},
                    {"Discount", "CountDisc", count<A>},
                    // Others
                    {"DiscPrice", "SumDiscPrice", sum<A>},
                    {"Charge", "SumCharge", sum<A>},
                    {"CountOrder", "CountOrder", count<A>},
                },
                {.reverse = true, .do_sort = false});

    stopwatch::timepoint("aggregation");

    // trim table so we're only doing a small number of divisions
    L.deleteColumns(
        {"Quantity", "Discount", "ExtendedPrice", "Tax", "Charge", "DiscPrice", "[ShipDate]"});

    // handle valid in aggregation
    L.sort({ENC_TABLE_VALID});

    stopwatch::timepoint("valid sort");

    L.tail(6);

    // Use auto a2b conversion
    L["[AvgQuantity]"] = L["SumQuantity"] / L["CountQuantity"];
    L["[AvgPrice]"] = L["SumPrice"] / L["CountPrice"];
    L["[AvgDisc]"] = L["SumDisc"] / L["CountDisc"];

    stopwatch::timepoint("average");

    L.deleteColumns(
        {"SumQuantity", "SumPrice", "SumDisc", "CountQuantity", "CountPrice", "CountDisc"});

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    L.finalize();
#endif

    stopwatch::done(); // print wall clock time
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    auto res = L.open_with_schema();
    print_table(res, pid);

    auto rf_col = L.get_column(res, "[ReturnFlag]");
    auto ls_col = L.get_column(res, "[LineStatus]");
    auto co_col = L.get_column(res, "CountOrder");
    auto sc_col = L.get_column(res, "SumCharge");
    auto sd_col = L.get_column(res, "SumDiscPrice");
    auto ad_col = L.get_column(res, "[AvgDisc]");
    auto ap_col = L.get_column(res, "[AvgPrice]");
    auto aq_col = L.get_column(res, "[AvgQuantity]");

    std::map<std::pair<int, int>, std::vector<T>> check;

    if (pid == 0) {
        for (int i = 0; i < rf_col.size(); i++) {
            check[{rf_col[i], ls_col[i]}] = {co_col[i], sc_col[i], sd_col[i],
                                             ad_col[i], ap_col[i], aq_col[i]};
        }
        const char* query = R"sql(
            select
                ReturnFlag,
                LineStatus,
                sum(Quantity) as sum_qty,
                sum(ExtendedPrice) as sum_base_price,
                sum(ExtendedPrice * (100-Discount) / 100) as sum_disc_price,
                sum(ExtendedPrice * (100-Discount) / 100 * (100+Tax) / 100) as sum_charge,
                avg(Quantity) as avg_qty,
                avg(ExtendedPrice) as avg_price,
                avg(Discount) as avg_disc,
                count(*) as count_order
            from lineitem
                where ShipDate <= ?
                group by ReturnFlag, LineStatus
                order by ReturnFlag, LineStatus;
        )sql";

        sqlite3_stmt* stmt;
        auto ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, SHIP_DATE);

        int i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            auto rf = sqlite3_column_int(stmt, 0);
            auto ls = sqlite3_column_int(stmt, 1);
            auto sq = sqlite3_column_int(stmt, 2);
            auto sb = sqlite3_column_int(stmt, 3);
            auto sd = sqlite3_column_int(stmt, 4);
            auto sc = sqlite3_column_int(stmt, 5);
            auto aq = sqlite3_column_int(stmt, 6);
            auto ap = sqlite3_column_int(stmt, 7);
            auto ad = sqlite3_column_int(stmt, 8);
            auto co = sqlite3_column_int(stmt, 9);
            // whew...

            // std::cout << rf << " " << ls << "\t" << \
            //     co << " " << sc << " " << sd << " " << ad << " " << ap << " " \
            //     << aq << "\n";

            // Check often fails due to overflows; try smaller inputs
            assert((check[{rf, ls}] == std::vector<T>({co, sc, sd, ad, ap, aq})));

            i++;
        }
        ASSERT_SAME(i, rf_col.size());
        if (i == 0) {
            single_cout("Empty result");
        }

        if (ret != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }

        sqlite3_finalize(stmt);
    }
#endif
    sqlite3_close(sqlite_db);

    MPI_Finalize();
}