/**
 * @file sf-q12.cpp
 * @brief Implement TPCH query 12 as seen in secret flow.
 * Party 0 has lineitem table while party 1 has orders table.
 * @date 2024-08-05
 * 
 * Equivalent SQL:
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
 * 
 * 
 * 
 * SF SQL:
 * select
 *      ant.l_shipmode,
 *      sum( case
 *           when isv.o_orderpriority = '1-URGENT'
 *           or isv.o_orderpriority = '2-HIGH'
 *           then 1
 *           else 0
 *       end ) as high_line_count,
 *      sum( case
 *           when isv.o_orderpriority <> '1-URGENT'
 *           and isv.o_orderpriority <> '2-HIGH'
 *           then 1
 *           else 0
 *       end ) as low_line_count
 * from (
 *          select *
 *          from alice_lineitem
 *          limit 1000
 *      ) as ant,
 *      (
 *          select *
 *          from bob_orders
 *          limit 1000
 *      ) as isv
 * where 
 *      isv.o_orderkey = ant.l_orderkey
 *      and ant.l_shipmode1 in ('AIR', 'SHIP', 'RAIL')
 *      and ant.l_commitdate < ant.l_receiptdate
 *      and ant.l_shipdate < ant.l_commitdate
 *      and ant.l_receiptdate >= STR_TO_DATE('1995-02-06', '%Y-%m-%d')
 *      and ant.l_receiptdate < ADDDATE(
 *       STR_TO_DATE('1995-01-01', '%Y-%m-%d'),
 *       interval 1 year
 *   )
 * group by ant.l_shipmode;
 * 
 * 
 * 
 * 
 * Optimized and Implemented SQL:
 * (
 *      select ant.l_shipmode, isv.o_orderpriority
 *      from (
 *          select *
 *          from (
 *             select *
 *             from alice_lineitem
 *             limit 1000)
 *          where l_shipmode1 in ('AIR', 'SHIP', 'RAIL')
 *          and l_commitdate < l_receiptdate
 *          and l_shipdate < l_commitdate
 *          and l_receiptdate >= STR_TO_DATE('1995-02-06', '%Y-%m-%d')
 *          and l_receiptdate < ADDDATE(
 *          STR_TO_DATE('1995-01-01', '%Y-%m-%d'), interval 1 year )
 *      ) as ant,
 *      (
 *          select *
 *          from bob_orders
 *          limit 1000
 *      ) as isv
 * where isv.o_orderkey = ant.l_orderkey
 * ) as ant_isv_join
 * 
 * 
 * # hacky way for evaluating conditions
 * ant_isv_join[high_line_count] = (isv.o_orderpriority == '1-URGENT') or (isv.o_orderpriority = '2-HIGH');
 * convert ant_isv_join[high_line_count] to arithmetic sharing
 * ant_isv_join[low_line_count] = 1 - ant_isv_join[high_line_count];
 * 
 * 
 * select 
 *      ant.l_shipmode, 
 *      sum(high_line_count) as high_line_count,
 *      sum(low_line_count) as low_line_count
 * from ant_isv_join
 * group by ant.l_shipmode;
 */


#include "../../../include/benchmark/stopwatch.h"
#include "../../../include/secrecy.h"
#include "../tpch/tpch_dbgen.h"

#define PRINT_TABLES
// #define QUERY_PROFILE
#define TRUNCATE_TABLES_AFTER_FILTER

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using namespace secrecy::aggregators;
using namespace secrecy::benchmarking;

using T = int32_t;
using A = ASharedVector<T>;
using B = BSharedVector<T>;

int main(int argc, char** argv) {
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    sqlite3 *sqlite_db = nullptr;
    int err = sqlite3_open(NULL, &sqlite_db);
    if(err) {
        throw std::runtime_error(sqlite3_errmsg(sqlite_db));
    }

    // arbitrary
    const int SHIPMODE1 = 0;
    const int SHIPMODE2 = 1;
    const int SHIPMODE3 = 2;
    const int MAX_SHIPMODE = SHIPMODE3;
    const int DATE_INPUT = 37;
    const T maxIDValue = std::numeric_limits<T>::max();

    // Specify which MPC party has which table. Set to `0,0` for correctness check
    // Alice Party
    const int T1PartyID = 0;
    // Bob Party
    const int T2PartyID = 1;

    ////////////////////////////////////////////////////////////
    ////////////////////// DB Initialize ///////////////////////
    ////////////////////////////////////////////////////////////
    auto db = TPCDatabase<T>(sf, sqlite_db);
    single_cout("Q12 SF " << db.scaleFactor);

    // TPCH Generator
    // auto L = db.getLineitemTable();
    // auto O = db.getOrdersTable();

    // SecretFlow Generator, TPCH Ratios
    // auto L = db.getLineitemTableSecretFlow(sf * 6'000'000);
    // auto O = db.getOrdersTableSecretFlow(sf * 1'500'000);

    // SecretFlow Generator, Secretflow Ratios
    auto L = db.getLineitemTableSecretFlow(sf * 1'048'576);
    auto O = db.getOrdersTableSecretFlow(sf * 1'048'576);

    auto L_SIZE = L.size();
    auto O_SIZE = O.size();

    single_cout("LINEITEM size: " << L_SIZE);
    single_cout("ORDERS size: " << O_SIZE << std::endl);

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    ////////////////////////////////////////////////////////////
    ////////////////////// T1(ant) local ///////////////////////
    ////////////////////////////////////////////////////////////
    
    // OrderKey, ShipMode
    std::vector<T> OrderKeyT1(L_SIZE);
    std::vector<T> ShipModeT1(L_SIZE);

    {
        // TODO: SF has 3 shipmode
        const char* t1_local_query = R"sql(
            select OrderKey, ShipMode
            from lineitem
            where
                shipmode in (?, ?, ?)
                and commitdate < receiptdate
                and shipdate < commitdate
                and receiptdate >= ?
                and receiptdate < ? + 365
            order by OrderKey, ShipMode
        )sql";

        sqlite3_stmt* stmt;
        auto ret = sqlite3_prepare_v2(sqlite_db, t1_local_query, -1, &stmt, NULL);

        sqlite3_bind_int(stmt, 1, SHIPMODE1);
        sqlite3_bind_int(stmt, 2, SHIPMODE2);
        sqlite3_bind_int(stmt, 3, SHIPMODE3);
        sqlite3_bind_int(stmt, 4, DATE_INPUT);
        sqlite3_bind_int(stmt, 5, DATE_INPUT);
        
        int i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            OrderKeyT1[i] = sqlite3_column_int(stmt, 0);
            ShipModeT1[i] = sqlite3_column_int(stmt, 1);
            i++;
        }
        if (i == 0) {
            single_cout("Empty result");
        }
        if (ret != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }

        // Padding for merge to work
        auto paddingSize = 1 << (int)ceil(log2(i));
        OrderKeyT1.resize(paddingSize);
        ShipModeT1.resize(paddingSize);
        for(; i < paddingSize; i++) {
            OrderKeyT1[i] = maxIDValue;
            ShipModeT1[i] = 0;
        }

#ifdef TRUNCATE_TABLES_AFTER_FILTER
        // Truncate tables after filtering
        if (pid == T1PartyID) {
            for(int j = 1; j < runTime->getNumParties(); j++) {
                runTime->comm0()->sendShare(i, j);
            }
        }else {
            runTime->comm0()->receiveShare(i, T1PartyID - pid);
        }

        single_cout("New LINEITEM, size " << i);

        // Resize vectors to the number of rows returned
        OrderKeyT1.resize(i);
        ShipModeT1.resize(i);
#endif
    }

    std::vector<Vector<T>> DataT1{OrderKeyT1, ShipModeT1};
    std::vector<std::string> SchemaT1{"[OrderKey]", "[ShipMode]"};


    ////////////////////////////////////////////////////////////
    ////////////////////// T2(isv) local ///////////////////////
    ////////////////////////////////////////////////////////////
    
    // OrderKey, low_line_count, high_line_count
    std::vector<T> OrderKeyT2(O_SIZE);
    std::vector<T> HighLineCountT2(O_SIZE);
    std::vector<T> LowLineCountT2(O_SIZE);
    
    {
        // TODO: removed group by OrderKey
        const char* t2_local_query = R"sql(
            select 
                OrderKey,
                sum(case
                    when orderpriority = 1
                    or orderpriority = 2
                    then 1
                    else 0
                end) as high_line_count,
                sum(case
                    when orderpriority <> 1
                    and orderpriority <> 2
                    then 1
                    else 0
                end) as low_line_count
            from ORDERS
            group by OrderKey
            order by OrderKey
        )sql";

        sqlite3_stmt* stmt;
        auto ret = sqlite3_prepare_v2(sqlite_db, t2_local_query, -1, &stmt, NULL);
        
        int i = 0;

        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            OrderKeyT2[i] = sqlite3_column_int(stmt, 0);
            HighLineCountT2[i] = sqlite3_column_int(stmt, 1);
            LowLineCountT2[i] = sqlite3_column_int(stmt, 2);
            i++;
        }
        if (i == 0) {
            single_cout("Empty result");
        }
        if (ret != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }

        // Padding for merge to work
        auto paddingSize = 1 << (int)ceil(log2(i));
        OrderKeyT2.resize(paddingSize);
        HighLineCountT2.resize(paddingSize);
        LowLineCountT2.resize(paddingSize);
        for(; i < paddingSize; i++) {
            OrderKeyT2[i] = maxIDValue;
            HighLineCountT2[i] = 0;
            LowLineCountT2[i] = 0;
        }

#ifdef TRUNCATE_TABLES_AFTER_FILTER
        // Truncate tables after filtering
        if (pid == T2PartyID) {
            for(int j = 1; j < runTime->getNumParties(); j++) {
                runTime->comm0()->sendShare(i, j);
            }
        }else {
            runTime->comm0()->receiveShare(i, T2PartyID - pid);
        }

        single_cout("New ORDERS, size " << i);

        // Resize vectors to the number of rows returned
        OrderKeyT2.resize(i);
        HighLineCountT2.resize(i);
        LowLineCountT2.resize(i);
#endif
    }

    std::vector<Vector<T>> DataT2{OrderKeyT2, HighLineCountT2, LowLineCountT2};
    std::vector<std::string> SchemaT2{"[OrderKey]", "HighLineCount", "LowLineCount"};

    ////////////////////////////////////////////////////////////
    ////////////////////// Secure Tables ///////////////////////
    ////////////////////////////////////////////////////////////
    stopwatch::timepoint("Plain Text");
    EncodedTable<T> L2  = secret_share<T>(DataT1, SchemaT1, T1PartyID);
    EncodedTable<T> O2  = secret_share<T>(DataT2, SchemaT2, T2PartyID);  // TODO: make party sharing = 1

    auto tj = O2.uu_join(L2, {"[OrderKey]"}, {
        {"HighLineCount", "HighLineCount", copy<A>},
        {"LowLineCount", "LowLineCount", copy<A>},
    }, {.trim_invalid = false}, secrecy::SortingProtocol::BITONICMERGE);
    tj.deleteColumns({"[OrderKey]"});

    stopwatch::timepoint("Join");

    tj.addColumn("V");
    tj.convert_b2a_bit(ENC_TABLE_VALID, "V");
    tj["HighLineCount"] *= tj["V"];
    tj["LowLineCount"] *= tj["V"];
    stopwatch::timepoint("Mask");

    tj.shuffle();
    stopwatch::timepoint("Shuffle");

    // Alice party knows ShipMode in the clear, so after shuffling it is still
    // safe to open, since we don't mask ShipMode column with VALID. Alice can
    // then build a selection vector to compute the respective sums.
    //
    // NOTE: our `open` function opens to all parties; should technically only
    // be Alice, but this won't effect time.
    auto openShipMode = tj.asBSharedVector("[ShipMode]").open();

    Vector<T> lineLow(MAX_SHIPMODE), lineHigh(MAX_SHIPMODE);

    for (int s = 0; s <= MAX_SHIPMODE; s++) {
        // selection vector of {0, 1}* known to Alice
        auto sel = (openShipMode == s);
        A sel_a = secret_share_a(sel, T1PartyID);

        ASharedVector h_count = tj.asASharedVector("HighLineCount") * sel_a;
        h_count.prefix_sum();
        h_count.tail(1);
        lineHigh[s] = h_count.open()[0];

        ASharedVector l_count = tj.asASharedVector("LowLineCount") * sel_a;
        l_count.prefix_sum();
        l_count.tail(1);
        lineLow[s] = l_count.open()[0];
    }

    // Replaces:
    // T.aggregate({"[ShipMode]"}, {
    //     {"HighLineCount", "HighLineCount", sum<A>},
    //     {"LowLineCount", "LowLineCount", sum<A>},
    // });

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
    if (pid == 0 && T1PartyID == T2PartyID) {
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
            and L.shipmode in (?, ?, ?)
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
        sqlite3_bind_int(stmt, 3, SHIPMODE3);
        sqlite3_bind_int(stmt, 4, DATE_INPUT);
        sqlite3_bind_int(stmt, 5, DATE_INPUT);

        int i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            auto shipMode = sqlite3_column_int(stmt, 0);
            auto highCount = sqlite3_column_int(stmt, 1);
            auto lowCount = sqlite3_column_int(stmt, 2);

            auto L = lineLow[shipMode];
            auto H = lineHigh[shipMode];
            
            assert((L == lowCount));
            assert((H == highCount));

            // std::cout << std::format("ShipMode {}: Low {} High {}\n", shipMode, L, H);
            i++;
        }

#ifdef TRUNCATE_TABLES_AFTER_FILTER
        // Correct only when truncating, otherwise SQL adds wrong keys (0)
        // ASSERT_SAME(i, mode.size());
#endif
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