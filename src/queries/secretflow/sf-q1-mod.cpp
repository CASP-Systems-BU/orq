/**
 * @file sf-q1-mod.cpp
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

#include "../../../include/secrecy.h"
#include "../tpch/tpch_dbgen.h"

// #define PRINT_TABLES
// #define QUERY_PROFILE

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using namespace secrecy::aggregators;
using namespace secrecy::benchmarking;

// This query overflows when using 32-bit tables.
using T = int64_t;

using A = ASharedVector<T>;
using B = BSharedVector<T>;

// From Secretflow queries
const int SHIP_DATE = (365 - 37);

int main(int argc, char** argv) {
    secrecy_init(argc, argv);
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

    // TODO: command line argument for scale factor
    auto db = TPCDatabase<T>(sf, sqlite_db);
    single_cout("Q1 SF " << db.scaleFactor);

    // TPCH Generator
    // auto L_alice = db.getLineitemTable();

    // SecretFlow Generator, TPCH Ratios
    // auto L_alice = db.getLineitemTableSecretFlow(sf * 6'000'000);

    // SecretFlow Generator, Secretflow Ratios
    auto L_alice = db.getLineitemTableSecretFlow(sf * (1 << 20));

    auto L_bob = L_alice.deepcopy();

    single_cout("\nLINEITEM Alice size: " << L_alice.size());
    single_cout("LINEITEM Bob size: " << L_bob.size() << std::endl);

    L_alice.project({"[ShipDate]", "[OrderKey]", "[ReturnFlag]", "ExtendedPrice", "Discount", "Tax",
                     "Quantity"});
    L_bob.project({"[OrderKey]", "[LineStatus]"});

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    std::vector<Vector<T>> data;
    int alice_size;
    if (pid == 0) {
        const char* query = R"sql(
            select
                ReturnFlag,
                OrderKey,
                Quantity,
                ExtendedPrice,
                Discount,
                (ExtendedPrice * (100-Discount) / 100) as disc_price,
                (ExtendedPrice * (100-Discount) / 100 * (100+Tax) / 100) as charge
            from lineitem
            where ShipDate <= ?;
        )sql";

        sqlite3_stmt* stmt;
        auto ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, SHIP_DATE);

        std::vector<T> _ret_flag;
        std::vector<T> _order_key;
        std::vector<T> _qty;
        std::vector<T> _ext_price;
        std::vector<T> _discount;
        std::vector<T> _disc_price;
        std::vector<T> _charge;

        int i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            _ret_flag.push_back(sqlite3_column_int(stmt, 0));
            _order_key.push_back(sqlite3_column_int(stmt, 1));
            _qty.push_back(sqlite3_column_int(stmt, 2));
            _ext_price.push_back(sqlite3_column_int(stmt, 3));
            _discount.push_back(sqlite3_column_int(stmt, 4));
            _disc_price.push_back(sqlite3_column_int(stmt, 5));
            _charge.push_back(sqlite3_column_int(stmt, 6));

            i++;
        }
        if (i == 0) {
            single_cout("Empty result");
        }

        if (ret != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }

        Vector<T> ret_flag(_ret_flag);
        Vector<T> order_key(_order_key);
        Vector<T> qty(_qty);
        Vector<T> ext_price(_ext_price);
        Vector<T> discount(_discount);
        Vector<T> disc_price(_disc_price);
        Vector<T> charge(_charge);

        data = {ret_flag, order_key, qty, ext_price, discount, disc_price, charge};

        alice_size = ret_flag.size();
        runTime->comm0()->sendShare(alice_size, 1);
    } else {
        runTime->comm0()->receiveShare(alice_size, -1);
        Vector<T> dummy(alice_size);
        data = {dummy, dummy, dummy, dummy, dummy, dummy, dummy};
    }
    stopwatch::timepoint("plaintext");

    std::vector<std::string> schema = {"[ReturnFlag]", "[OrderKey]", "Quantity", "ExtendedPrice",
                                       "Discount",     "DiscPrice",  "Charge"};

    EncodedTable<T> mainTable = secret_share<T>(data, schema, 0);

    stopwatch::timepoint("share table");

    auto combinedTable =
        L_bob.uu_join(mainTable, {"[OrderKey]"}, {{"[LineStatus]", "[LineStatus]", copy<B>}});

    stopwatch::timepoint("join");

    // Build a compound key.
    // ReturnFlag is (0, 1, 2) so only needs 2 bits
    // LineStatus is a bit (0, 1)
    // Equivalent to SecretFlow's SBK_Valid
    combinedTable.addColumn("[Key]");
    combinedTable["[Key]"] = combinedTable["[ReturnFlag]"] << 1 | combinedTable["[LineStatus]"];

    // don't sort on valid, because we're going to zero out those rows
    combinedTable.sort({"[Key]"});
    stopwatch::timepoint("sort");

    std::vector<std::string> intermediate_columns = {
        "SumDiscPrice",  "SumCharge",  "SumQuantity", "SumPrice", "SumDisc",
        "[AvgQuantity]", "[AvgPrice]", "[AvgDisc]",   "Count",    "VA"};
    combinedTable.addColumns(intermediate_columns);

    combinedTable.convert_b2a_bit(ENC_TABLE_VALID, "VA");

    combinedTable["Quantity"] *= combinedTable["VA"];
    combinedTable["ExtendedPrice"] *= combinedTable["VA"];
    combinedTable["Discount"] *= combinedTable["VA"];
    combinedTable["DiscPrice"] *= combinedTable["VA"];
    combinedTable["Charge"] *= combinedTable["VA"];

    stopwatch::timepoint("valid&mask");

    combinedTable.aggregate({"[Key]"},
                            {
                                {"Quantity", "SumQuantity", sum<A>},
                                {"ExtendedPrice", "SumPrice", sum<A>},
                                {"Discount", "SumDisc", sum<A>},
                                {"VA", "Count", sum<A>},  // count(*)
                                {"DiscPrice", "SumDiscPrice", sum<A>},
                                {"Charge", "SumCharge", sum<A>},
                            },
                            {.reverse = true, .do_sort = false});

    stopwatch::timepoint("aggregation");

    combinedTable.deleteColumns(
        {"Quantity", "Discount", "ExtendedPrice", "Charge", "DiscPrice", "VA"});

    // We don't actually need to shuffle here: by just masking, we zero out
    // all invalid rows; the spacing of the valid rows only leaks per-group
    // counts, which are already provided by the query output.
    //// combinedTable.shuffle();
    combinedTable.mask();

    stopwatch::timepoint("mask");

    // combinedTable.addColumn("[Count]");
    // combinedTable.convert_a2b("Count", "[Count]");

    // combinedTable["[AvgQuantity]"] = combinedTable["SumQuantity"] / combinedTable["[Count]"];
    // combinedTable["[AvgPrice]"] = combinedTable["SumPrice"] / combinedTable["[Count]"];
    // combinedTable["[AvgDisc]"] = combinedTable["SumDisc"] / combinedTable["[Count]"];

    std::cout << "outsize2: " << combinedTable.size() << std::endl;

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    combinedTable.finalize();
#endif

#ifndef QUERY_PROFILE

    auto res = combinedTable.open_with_schema();
    print_table(res, pid);

    auto rf_col = combinedTable.get_column(res, "[ReturnFlag]");
    auto ls_col = combinedTable.get_column(res, "[LineStatus]");
    auto co_col = combinedTable.get_column(res, "Count");
    auto sq_col = combinedTable.get_column(res, "SumQuantity");
    auto sp_col = combinedTable.get_column(res, "SumPrice");
    auto sd_col = combinedTable.get_column(res, "SumDiscPrice");
    auto sc_col = combinedTable.get_column(res, "SumCharge");
    auto ad_col = combinedTable.get_column(res, "SumDisc") / co_col;
    auto ap_col = sp_col / co_col;
    auto aq_col = sq_col / co_col;
    stopwatch::timepoint("open & division");

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    std::map<std::pair<int, int>, std::vector<T>> check;

    if (pid == 0) {
        for (int i = 0; i < rf_col.size(); i++) {
            check[{rf_col[i], ls_col[i]}] = {
                co_col[i], sq_col[i], sp_col[i], sd_col[i],
                sc_col[i], ad_col[i], ap_col[i], aq_col[i],
            };
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
            // i64 because these can overflow 32b
            T sq = sqlite3_column_int64(stmt, 2);
            T sb = sqlite3_column_int64(stmt, 3);
            T sd = sqlite3_column_int64(stmt, 4);
            T sc = sqlite3_column_int64(stmt, 5);
            auto aq = sqlite3_column_int(stmt, 6);
            auto ap = sqlite3_column_int(stmt, 7);
            auto ad = sqlite3_column_int(stmt, 8);
            auto co = sqlite3_column_int(stmt, 9);
            // whew...

            auto r = std::vector<T>({co, sq, sb, sd, sc, ad, ap, aq});
            // print(r);
            // print(check[{rf, ls}]);

            // Check often fails due to overflows; try smaller inputs
            assert((check[{rf, ls}] == r));

            i++;
        }
        std::cout << "Size: " << i << std::endl;
        ASSERT_SAME(i, rf_col.size());
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

    sqlite3_close(sqlite_db);

    MPI_Finalize();
}