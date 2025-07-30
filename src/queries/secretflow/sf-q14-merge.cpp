/**
 * @file q14.cpp
 * @brief TPCH Query 14
 *
 *
 * Equivalent SQL:
 *  select
 *      100.00 * sum(case
 *                  when p_type like 'PROMO%'
 *                  then l_extendedprice*(1-l_discount)
 *                  else 0
 *              end) / sum(l_extendedprice * (1 - l_discount)) as promo_revenue
 *  from
 *      lineitem,
 *      part
 *  where
 *      l_partkey = p_partkey
 *      and l_shipdate >= date '[DATE]'
 *      and l_shipdate < date '[DATE]' + interval '1' month
 *
 *
 *
 *
 * SF SQL:
 * select
 *      100.00 * sum( case
 *                  when isv.type_promo
 *                  then ant.l_extendedprice *(1 - ant.l_discount)
 *                  else 0.0
 *              end) / sum( ant.l_extendedprice * (1 - ant.l_discount)) as promo_revenue
 * from
 *      (
 *          select *
 *          from alice_lineitem
 *          limit 1000
 *      ) as ant
 *     join
 *      (
 *          select p_partkey,
 *              p_type LIKE 'PROMO%' as type_promo
 *          from bob_part
 *          limit 1000
 *      ) as isv
 * where ant.l_partkey = isv.p_partkey
 *     and ant.l_shipdate >= STR_TO_DATE('1995-02-06', '%Y-%m-%d')
 *     and ant.l_shipdate < ADDDATE(
 *         STR_TO_DATE('1996-01-01', '%Y-%m-%d'),
 *         interval 11 MONTH );
 *
 *
 *
 * Optimized and Implemented SQL:
 * select
 *      100.00 * sum( case
 *                  when isv.type_promo
 *                  then ant.l_extendedprice *(1 - ant.l_discount)
 *                  else 0.0
 *              end) / sum( ant.l_extendedprice * (1 - ant.l_discount)) as promo_revenue
 * from
 *      (
 *          select
 *              l_partkey,
 *              sum (l_extendedprice *(1 - ant.l_discount)) as keyed_promo_revenue,
 *              sum (l_extendedprice *(1 - ant.l_discount)) as promo_revenue,
 *          from (
 *                  select *
 *                  from alice_lineitem
 *                  limit 1000
 *              )
 *          where l_shipdate >= STR_TO_DATE('1995-02-06', '%Y-%m-%d')
 *              and l_shipdate < ADDDATE(
 *                  STR_TO_DATE('1996-01-01', '%Y-%m-%d'),
 *                  interval 11 MONTH )
 *          group by l_partkey
 *      )as ant
 *     join
 *      (
 *          select p_partkey,
 *              p_type LIKE 'PROMO%' as type_promo
 *          from bob_part
 *          limit 1000
 *      ) as isv
 * where ant.l_partkey = isv.p_partkey
 */

// We *must* use PPA for this query because it performs a single division,
// which does not amortize enough to use RCA efficiently.
#define USE_PARALLEL_PREFIX_ADDER 1

#include "../../../include/benchmark/stopwatch.h"
#include "../../../include/secrecy.h"
#include "../tpch/tpch_dbgen.h"

// #define PRINT_TABLES
// #define QUERY_PROFILE
#define TRUNCATE_TABLES_AFTER_FILTER

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

using T = int32_t;
using A = ASharedVector<T>;
using B = BSharedVector<T>;

using sec = duration<float, seconds::period>;

int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
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

    // TPCH Q14 query parameters
    const int DATE = 36;
    const int DATE_INTERVAL = 365;  // Arbitrary date interval to account for date format
    const T maxIDValue = std::numeric_limits<T>::max();

    // Alice
    const int T1PartyID = 0;
    // Bob
    const int T2PartyID = 0;

    ////////////////////////////////////////////////////////////
    ////////////////////// DB Initialize ///////////////////////
    ////////////////////////////////////////////////////////////
    auto db = TPCDatabase<T>(sf, sqlite_db);
    single_cout("Q14 SF " << db.scaleFactor);

    // TPCH Generator
    // auto LineItem = db.getLineitemTable();
    // auto Part = db.getPartTable();

    // SecretFlow Generator, TPCH Ratios
    // auto LineItem = db.getLineitemTableSecretFlow(sf * 6'000'000);
    // auto Part = db.getPartTableSecretFlow(sf * 200'000);

    // SecretFlow Generator, Secretflow Ratios
    auto LineItem = db.getLineitemTableSecretFlow(sf * 1'048'576);
    auto Part = db.getPartTableSecretFlow(sf * 1'048'576);

    auto LineItemSize = LineItem.size();
    auto PartSize = Part.size();

    single_cout("\nLINEITEM size: " << LineItemSize);
    single_cout("PART size: " << PartSize << std::endl);

#ifdef PRINT_TABLES
    single_cout("LINEITEM, size " << db.lineitemsSize());
    print_table(LineItem.open_with_schema(), pid);

    single_cout("PART, size " << db.partSize());
    print_table(Part.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    ////////////////////////////////////////////////////////////
    ////////////////////// T1(ant) local ///////////////////////
    ////////////////////////////////////////////////////////////

    // partkey, keyed_promo_revenue, promo_revenue
    std::vector<T> PartKeyT1(LineItemSize);
    std::vector<T> KeyedPromoRevenueT1(LineItemSize);
    std::vector<T> PromoRevenueT1(LineItemSize);

    {
        const char* t1_local_query = R"sql(
            select 
                PartKey,
                ExtendedPrice*(100 - Discount)/100 as promo_revenue
            from lineitem
            where
                ShipDate >= ?
                and ShipDate < (? + ?)
            order by PartKey
        )sql";

        sqlite3_stmt* stmt;
        auto ret = sqlite3_prepare_v2(sqlite_db, t1_local_query, -1, &stmt, NULL);

        sqlite3_bind_int(stmt, 1, DATE);
        sqlite3_bind_int(stmt, 2, DATE);
        sqlite3_bind_int(stmt, 3, DATE_INTERVAL);

        std::unordered_map<T, T> promo_revenue_map;

        int i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            auto partKey = sqlite3_column_int(stmt, 0);
            promo_revenue_map[partKey] += sqlite3_column_int(stmt, 1);
            i++;
        }
        if (i == 0) {
            single_cout("Empty result");
        }
        if (ret != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }

        i = 0;
        for (const auto& it : promo_revenue_map) {
            PartKeyT1[i] = it.first;
            PromoRevenueT1[i] = it.second;
            KeyedPromoRevenueT1[i] = 100 * it.second;
            i++;
        }

        // Padding for merge to work
        auto paddingSize = 1 << (int)ceil(log2(i));
        PartKeyT1.resize(paddingSize);
        KeyedPromoRevenueT1.resize(paddingSize);
        PromoRevenueT1.resize(paddingSize);
        for (; i < paddingSize; i++) {
            PartKeyT1[i] = maxIDValue;
            KeyedPromoRevenueT1[i] = 0;
            PromoRevenueT1[i] = 0;
        }

#ifdef TRUNCATE_TABLES_AFTER_FILTER
        // Truncate tables after filtering
        if (pid == T1PartyID) {
            for (int j = 1; j < runTime->getNumParties(); j++) {
                runTime->comm0()->sendShare(i, j);
            }
        } else {
            runTime->comm0()->receiveShare(i, T1PartyID - pid);
        }

        single_cout("New LINEITEM, size " << i);

        // Resize vectors to the number of rows returned
        PartKeyT1.resize(i);
        KeyedPromoRevenueT1.resize(i);
        PromoRevenueT1.resize(i);
#endif
    }

    std::vector<Vector<T>> DataT1{PartKeyT1, KeyedPromoRevenueT1, PromoRevenueT1};
    std::vector<std::string> SchemaT1{"[PartKey]", "KeyedPromoRevenue", "PromoRevenue"};

    ////////////////////////////////////////////////////////////
    ////////////////////// T2(isv) local ///////////////////////
    ////////////////////////////////////////////////////////////

    // partkey, type_promo
    std::vector<T> PartKeyT2(PartSize);
    std::vector<T> HasPromoT2(PartSize);

    {
        const char* t2_local_query = R"sql(
            select 
                PartKey,
                (case when P.Type = 1 then 1 else 0 end) as HasPromo
            from PART P
            order by PartKey
        )sql";

        sqlite3_stmt* stmt;
        auto ret = sqlite3_prepare_v2(sqlite_db, t2_local_query, -1, &stmt, NULL);

        int i = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            PartKeyT2[i] = sqlite3_column_int(stmt, 0);
            HasPromoT2[i] = sqlite3_column_int(stmt, 1);
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
        PartKeyT2.resize(paddingSize);
        HasPromoT2.resize(paddingSize);
        for (; i < paddingSize; i++) {
            PartKeyT2[i] = maxIDValue;
            HasPromoT2[i] = 0;
        }

#ifdef TRUNCATE_TABLES_AFTER_FILTER
        // Truncate tables after filtering
        if (pid == T2PartyID) {
            for (int j = 1; j < runTime->getNumParties(); j++) {
                runTime->comm0()->sendShare(i, j);
            }
        } else {
            runTime->comm0()->receiveShare(i, T2PartyID - pid);
        }

        single_cout("New PART, size " << i);

        // Resize vectors to the number of rows returned
        PartKeyT2.resize(i);
        HasPromoT2.resize(i);
#endif
    }

    std::vector<Vector<T>> DataT2{PartKeyT2, HasPromoT2};
    std::vector<std::string> SchemaT2{"[PartKey]", "HasPromo"};

    ////////////////////////////////////////////////////////////
    ////////////////////// Secure Tables ///////////////////////
    ////////////////////////////////////////////////////////////
    stopwatch::timepoint("Plain Text");
    EncodedTable<T> LineItem2 = secret_share<T>(DataT1, SchemaT1, T1PartyID);
    EncodedTable<T> Part2 = secret_share<T>(DataT2, SchemaT2, T2PartyID);
    stopwatch::timepoint("Secret Share Table");

    // [SQL] p_partkey = l_partkey
    // Don't bother trimming invalid (save a sort), because global aggregation
    // below.
    auto Join = Part2.uu_join(LineItem2, {"[PartKey]"}, {{"HasPromo", "HasPromo", copy<A>}},
                              {.trim_invalid = false}, secrecy::SortingProtocol::BITONICMERGE);
    stopwatch::timepoint("Join");

    Join.addColumn("VA");
    Join.convert_b2a_bit(ENC_TABLE_VALID, "VA");

    // Aggregation Filters
    Join["PromoRevenue"] = Join["PromoRevenue"] * Join["VA"];
    Join["KeyedPromoRevenue"] = Join["KeyedPromoRevenue"] * Join["HasPromo"] * Join["VA"];
    stopwatch::timepoint("Aggregation Filters");

    // Convert to 64-bits
    ASharedVector<int64_t> kp(Join.size());
    kp = Join.asASharedVector("KeyedPromoRevenue");
    ASharedVector<int64_t> pr(Join.size());
    pr = Join.asASharedVector("PromoRevenue");

    kp.prefix_sum();
    pr.prefix_sum();

    kp.tail(1);
    pr.tail(1);

    stopwatch::timepoint("Aggregation");

    auto output = kp / pr;
    stopwatch::timepoint("Division");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    Join.finalize();
#endif

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    // Fetch result
    // ignore valid
    auto result_column = output->open();

    if (pid == 0 && T1PartyID == T2PartyID) {
        size_t result = 0;
        if (result_column.size() > 0) {
            result = result_column[0];
        }

        int ret;
        // Note: "Type == 1" used as a substitute for "p_type like 'PROMO%'""
        const char* query = R"sql(
            select 
                100 * sum(case
                            when p.Type = 1
                            then l.ExtendedPrice*(100 - l.Discount)/100
                            else 0
                        end) / sum(l.ExtendedPrice * (100 - l.Discount)/100) as promo_revenue
            from
                LINEITEM l,
                PART p
            where
                l.PartKey = p.PartKey
                and l.ShipDate >= ?
                and l.ShipDate < (? + ?)
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, DATE);
        sqlite3_bind_int(stmt, 2, DATE);
        sqlite3_bind_int(stmt, 3, DATE_INTERVAL);

        size_t sqlResult;
        ret = sqlite3_step(stmt);
        if (ret != SQLITE_ROW) {
            std::cerr << "Execution failed: " << sqlite3_errmsg(sqlite_db) << std::endl;
        } else {
            sqlResult = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);

        // single_cout(std::endl);
        single_cout("Calculated result | Promo revenue: " << result);
        single_cout("SQL Query result  | Promo revenue: " << sqlResult << std::endl);

        ASSERT_SAME(sqlResult, result);
        std::cout << "Correctness OK\n";
    }
#endif

    // Close SQLite DB
    sqlite3_close(sqlite_db);

    MPI_Finalize();

    return 0;
}
