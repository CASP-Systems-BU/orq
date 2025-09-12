/**
 * @file sf-q1-mod-obliv.cpp
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

#include "../tpch/tpch_dbgen.h"
#include "orq.h"
#include "profiling/stopwatch.h"

// #define PRINT_TABLES

// #define QUERY_PROFILE

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using namespace orq::aggregators;
using namespace orq::benchmarking;

using T = int64_t;

using A = ASharedVector<T>;
using B = BSharedVector<T>;

const int SHIP_DATE = 110;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    sqlite3* sqlite_db = nullptr;
    int err = sqlite3_open(NULL, &sqlite_db);
    if (err) {
        throw std::runtime_error(sqlite3_errmsg(sqlite_db));
    }

    auto db = TPCDatabase<T>(sf, sqlite_db);
    single_cout("Q1 SF " << db.scaleFactor);

    // TPCH Generator
    // auto L_alice = db.getLineitemTable();

    // SecretFlow Generator, TPCH Ratios
    // auto L_alice = db.getLineitemTableSecretFlow(sf * 6'000'000);

    // SecretFlow Generator, Secretflow Ratios
    auto L_alice = db.getLineitemTableSecretFlow(sf * 10'000'000);

    auto L_bob = L_alice.deepcopy();

    single_cout("\nLINEITEM Alice size: " << L_alice.size());
    single_cout("LINEITEM Bob size: " << L_bob.size() << std::endl);

    L_alice.project({"[ShipDate]", "[OrderKey]", "[ReturnFlag]", "ExtendedPrice", "Discount", "Tax",
                     "Quantity"});
    L_bob.project({"[OrderKey]", "[LineStatus]"});

    stopwatch::timepoint("Start");

    // Arbitrary threshold - recent shipments
    L_alice.filter(L_alice["[ShipDate]"] <= SHIP_DATE);

    stopwatch::timepoint("filter");

    auto L = L_bob.uu_join(L_alice, {"[OrderKey]"}, {{"[LineStatus]", "[LineStatus]", copy<B>}});

    stopwatch::timepoint("join");

    L.sort({ENC_TABLE_VALID, "[ReturnFlag]", "[LineStatus]"});
    stopwatch::timepoint("sort");

    std::vector<std::string> intermediate_columns = {
        "DiscPrice",   "Charge",        "SumDiscPrice", "SumCharge",     "CountOrder",
        "SumQuantity", "SumPrice",      "SumDisc",      "CountQuantity", "CountPrice",
        "CountDisc",   "[AvgQuantity]", "[AvgPrice]",   "[AvgDisc]",
    };

    L.addColumns(intermediate_columns, L.size());

    // Discount is a percentage... cheat with integer math + public division
    // Need to write (100 - Discount) like this because column/element ops only
    // support constant on the RHS.
    L["DiscPrice"] = L["ExtendedPrice"] * (-L["Discount"] + 100) / 100;

    // Same with tax
    L["Charge"] = L["DiscPrice"] * (L["Tax"] + 100) / 100;

    stopwatch::timepoint("interm. calc.");

    L.aggregate({"[ReturnFlag]", "[LineStatus]"},
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
    L.deleteColumns({"Quantity", "Discount", "ExtendedPrice", "Charge", "DiscPrice"});

    // handle valid in aggregation
    L.sort({ENC_TABLE_VALID});
    L.tail(6);

    stopwatch::timepoint("valid sort");

    L["[AvgQuantity]"] = L["SumQuantity"] / L["CountQuantity"];
    L["[AvgPrice]"] = L["SumPrice"] / L["CountPrice"];
    L["[AvgDisc]"] = L["SumDisc"] / L["CountDisc"];

    stopwatch::timepoint("division");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    combinedTable.finalize();
#endif

    stopwatch::done();
    /*
    #ifndef QUERY_PROFILE

        auto res = L.open_with_schema();
        print_table(res, pid);

        auto rf_col = L.get_column(res, "[ReturnFlag]");
        auto ls_col = L.get_column(res, "[LineStatus]");
        auto co_col = L.get_column(res, "CountOrder");
        auto sc_col = L.get_column(res, "SumCharge");
        auto sd_col = L.get_column(res, "SumDisc");
        auto ad_col = L.get_column(res, "[AvgDisc]");
        auto ap_col = L.get_column(res, "[AvgPrice]");
        auto aq_col = L.get_column(res, "[AvgQuantity]");

        std::cout << rf_col.size() << std::endl;
        std::cout << ls_col.size() << std::endl;
        std::cout << co_col.size() << std::endl;
        std::cout << sc_col.size() << std::endl;
        std::cout << sd_col.size() << std::endl;

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
                //assert((check[{rf, ls}] == std::vector<T>({co, sc, sd, ad, ap, aq})));

                i++;
            }
            std::cout << "correct size: " << i << std::endl;
            //ASSERT_SAME(i, rf_col.size());
            if (i == 0) {
                single_cout("Empty result");
            }

            if (ret != SQLITE_DONE) {
                std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
            }

            sqlite3_finalize(stmt);

            single_cout("Correctness check passed");
        }
    #endif
    */
    sqlite3_close(sqlite_db);
}