/**
 * @file sf-q1.cpp
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

const int SHIP_DATE = 37;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }
    sf /= 2.0;

    sqlite3* sqlite_db = nullptr;
    int err = sqlite3_open(NULL, &sqlite_db);
    if (err) {
        throw std::runtime_error(sqlite3_errmsg(sqlite_db));
    }

    auto db = TPCDatabase<T>(sf, sqlite_db);
    single_cout("Q1 SF " << db.scaleFactor * 2);

    // TPCH Generator
    // auto L = db.getLineitemTable();

    // SecretFlow Generator, TPCH Ratios
    // auto L = db.getLineitemTableSecretFlow(sf * 6'000'000);

    // SecretFlow Generator, Secretflow Ratios
    auto L = db.getLineitemTableSecretFlow(sf * (1 << 20));

    single_cout("\nLINEITEM size: " << L.size() << std::endl);

    L.project({"[ShipDate]", "[ReturnFlag]", "[LineStatus]", "ExtendedPrice", "Discount", "Tax",
               "Quantity"});

    stopwatch::timepoint("Start");

    const char* query = R"sql(
        select ReturnFlag,
            LineStatus,
            sum(Quantity) as sum_qty,
            sum(ExtendedPrice) as sum_base_price,
            sum(ExtendedPrice * (100 - Discount) / 100) as sum_disc_price,
            sum(ExtendedPrice * (100 - Discount) / 100 * (100 + Tax) / 100) as sum_charge,
            sum(Discount),
            count(*) as count_order
        from lineitem
            where ShipDate <= ?
            group by ReturnFlag, LineStatus
    )sql";

    sqlite3_stmt* stmt;
    auto ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

    // Fill in query placeholders
    sqlite3_bind_int(stmt, 1, SHIP_DATE);

    int num_rows = 6;
    Vector<T> ret_flag(num_rows);
    Vector<T> line_status(num_rows);
    Vector<T> sum_quantity(num_rows);
    Vector<T> sum_extended_price(num_rows);
    Vector<T> sum_discounted_price(num_rows);
    Vector<T> sum_charge(num_rows);
    Vector<T> sum_discount(num_rows);
    Vector<T> count(num_rows);

    int i = 0;
    while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
        ret_flag[i] = sqlite3_column_int64(stmt, 0);
        line_status[i] = sqlite3_column_int64(stmt, 1);
        sum_quantity[i] = sqlite3_column_int64(stmt, 2);
        sum_extended_price[i] = sqlite3_column_int64(stmt, 3);
        sum_discounted_price[i] = sqlite3_column_int64(stmt, 4);
        sum_charge[i] = sqlite3_column_int64(stmt, 5);
        sum_discount[i] = sqlite3_column_int64(stmt, 6);
        count[i] = sqlite3_column_int64(stmt, 7);

        i++;
    }
    if (i == 0) {
        single_cout("Empty result");
    }

    if (ret != SQLITE_DONE) {
        std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
    }

    stopwatch::timepoint("plaintext");

    std::vector<Vector<T>> data = {
        ret_flag,   line_status,  sum_quantity, sum_extended_price, sum_discounted_price,
        sum_charge, sum_discount, count};

    std::vector<std::string> schema = {
        "ReturnFlag",         "LineStatus", "SumQuantity", "SumExtendedPrice",
        "SumDiscountedPrice", "SumCharge",  "SumDiscount", "Count"};

    EncodedTable<T> main_table = secret_share<T>(data, schema, 0);
    EncodedTable<T> other_table = secret_share<T>(data, schema, 1);

    stopwatch::timepoint("share table");

    main_table["SumQuantity"] = main_table["SumQuantity"] + other_table["SumQuantity"];
    main_table["SumExtendedPrice"] =
        main_table["SumExtendedPrice"] + other_table["SumExtendedPrice"];
    main_table["SumDiscountedPrice"] =
        main_table["SumDiscountedPrice"] + other_table["SumDiscountedPrice"];
    main_table["SumCharge"] = main_table["SumCharge"] + other_table["SumCharge"];
    main_table["SumDiscount"] = main_table["SumDiscount"] + other_table["SumDiscount"];
    main_table["Count"] = main_table["Count"] + other_table["Count"];

    stopwatch::timepoint("mpc sums");

    auto res = main_table.open_with_schema();
    auto rf_col = main_table.get_column(res, "ReturnFlag");
    auto ls_col = main_table.get_column(res, "LineStatus");
    auto co_col = main_table.get_column(res, "Count");
    auto sq_col = main_table.get_column(res, "SumQuantity");
    auto sc_col = main_table.get_column(res, "SumCharge");
    auto sd_col = main_table.get_column(res, "SumDiscount");
    auto sp_col = main_table.get_column(res, "SumExtendedPrice");

    print_table(res);

    // divide by zero prevention. subtract 1 if element is zero
    co_col -= (co_col == 0);

    // Can compute averages in plaintext since count is released.
    auto aq_col = sq_col / co_col;
    auto ad_col = sd_col / co_col;
    auto ap_col = sp_col / co_col;

    stopwatch::timepoint("open & averages");

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    print_table(res, pid);

    if (pid == 0) {
        single_cout_nonl("aq: ");
        print(aq_col);
        single_cout_nonl("aq: ");
        print(ad_col);
        single_cout_nonl("aq: ");
        print(ap_col);
    }

    /*
    Testing correcntess here is difficult. We operate on a table that is logically twice
    as large as either party's actual input table. However, the SQLite table has to have
    the smaller size so the parties can run their plaintext computation.
    */

    sqlite3_finalize(stmt);

    sqlite3_close(sqlite_db);
}