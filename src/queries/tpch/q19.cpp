/**
 * @file q19.cpp
 * @brief Implement TPCH query 19
 * @date 2024-08-12
 * 
 * Equivalent SQL:
 * 
 * select
 *  sum(l_extendedprice * (1 - l_discount) ) as revenue
 * from
 * 	lineitem,
 * 	part
 * where
 * 	(
 * 		p_partkey = l_partkey
 * 		and p_brand = ‘[BRAND1]’
 * 		and p_container in ( ‘SM CASE’, ‘SM BOX’, ‘SM PACK’, ‘SM PKG’)
 * 		and l_quantity >= [QUANTITY1] and l_quantity <= [QUANTITY1] + 10
 * 		and p_size between 1 and 5
 * 		and l_shipmode in (‘AIR’, ‘AIR REG’)
 * 		and l_shipinstruct = ‘DELIVER IN PERSON’
 * 	)
 * 	or
 * 	(
 * 		p_partkey = l_partkey
 * 		and p_brand = ‘[BRAND2]’
 * 		and p_container in (‘MED BAG’, ‘MED BOX’, ‘MED PKG’, ‘MED PACK’)
 * 		and l_quantity >= [QUANTITY2] and l_quantity <= [QUANTITY2] + 10
 * 		and p_size between 1 and 10
 * 		and l_shipmode in (‘AIR’, ‘AIR REG’)
 * 		and l_shipinstruct = ‘DELIVER IN PERSON’
 * 	)
 * 	or
 * 	(
 * 		p_partkey = l_partkey
 * 		and p_brand = ‘[BRAND3]’
 * 		and p_container in ( ‘LG CASE’, ‘LG BOX’, ‘LG PACK’, ‘LG PKG’)
 * 		and l_quantity >= [QUANTITY3] and l_quantity <= [QUANTITY3] + 10
 * 		and p_size between 1 and 15
 * 		and l_shipmode in (‘AIR’, ‘AIR REG’)
 * 		and l_shipinstruct = ‘DELIVER IN PERSON’
 * 	);
 * 
 */

#include "../../../include/secrecy.h"
#include "../../../include/benchmark/stopwatch.h"

#include "tpch_dbgen.h"

// #define PRINT_TABLES

// #define QUERY_PROFILE

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using namespace secrecy::aggregators;

using T = int64_t;

using A = ASharedVector<T>;
using B = BSharedVector<T>;

int main(int argc, char ** argv) {
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    const int BRAND1 = 5, BRAND2 = 12, BRAND3 = 24;
    const int QUANTITY1 = 8, QUANTITY2 = 19, QUANTITY3 = 22;

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

    single_cout("Q19 SF " << db.scaleFactor);

    auto L = db.getLineitemTable();
    auto P = db.getPartTable();

    L.project({"[ShipMode]", "[ShipInstruct]", "[PartKey]", "ExtendedPrice",
        "Discount", "Quantity"});
    P.project({"[Size]", "[PartKey]", "[Brand]", "[Container]"});

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // Common Filters: shipmode == AIR + shipinstruct == DELIVER; nonzero size
    L.filter((L["[ShipMode]"] == 1) & (L["[ShipInstruct]"] == 0));
    P.filter(P["[Size]"] >= 1);

    L.deleteColumns({"[ShipMode]", "[ShipInstruct]"});

    stopwatch::timepoint("filter1");

    auto PL = P.inner_join(L, {"[PartKey]"}, {
        {"[Brand]",     "[Brand]",      copy<B>},
        {"[Container]", "[Container]",  copy<B>},
        {"[Size]",      "[Size]",       copy<B>},
    });

    stopwatch::timepoint("join");

    PL.addColumns({"[OrFilter]", "Revenue", "[Quantity]", "SumRevenue"});

    PL.convert_a2b("Quantity", "[Quantity]");

    // Containers filtered on 4 each, mask down to 2 LSB. Change propagates to
    // original table since we operate on the underlying vector storage.
    auto cntr_mask = ((BSharedVector<T>*) PL["[Container]"].contents.get());
    cntr_mask->mask(0x3);
    
    // NOTE: these SIZE checks can be implemented with binary subtraction and
    // LTZ check
    PL["[OrFilter]"] = ((
        (PL["[Brand]"] == BRAND1) &
        (PL["[Container]"] == 0) &
        (PL["[Quantity]"] >= QUANTITY1) &
        (PL["[Quantity]"] <= QUANTITY1 + 10) &
        (PL["[Size]"] <= 5)
    ) | (
        (PL["[Brand]"] == BRAND2) &
        (PL["[Container]"] == 1) &
        (PL["[Quantity]"] >= QUANTITY2) &
        (PL["[Quantity]"] <= QUANTITY2 + 10) &
        (PL["[Size]"] <= 10)
    ) | (
        (PL["[Brand]"] == BRAND3) &
        (PL["[Container]"] == 2) &
        (PL["[Quantity]"] >= QUANTITY3) &
        (PL["[Quantity]"] <= QUANTITY3 + 10) &
        (PL["[Size]"] <= 15)
    ));

    PL.filter(PL["[OrFilter]"]);

    stopwatch::timepoint("filter2");

    PL["Revenue"] = PL["ExtendedPrice"] * (-PL["Discount"] + 100) / 100;

    PL.deleteColumns({"ExtendedPrice", "Discount", "[Brand]", "[Container]",
        "Quantity", "[Quantity]", "[Size]", "[OrFilter]", "[PartKey]"});

    PL.convert_b2a_bit(ENC_TABLE_VALID, "SumRevenue");
    PL["SumRevenue"] *= PL["Revenue"];
    PL.prefix_sum("SumRevenue");
    PL.tail(1);

    stopwatch::timepoint("agg");

    stopwatch::done(); // print wall clock time
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE
    auto result = PL.open_with_schema(false);

    // sql
    if (pid == 0) {
        print_table(result, pid);
        auto rev = PL.get_column(result, "SumRevenue")[0];

        const char * query = R"sql(
        select
            sum(L.extendedprice * (100 - L.discount) / 100) as revenue
        from
            lineitem as L,
            part as P
        where
            P.partkey = L.partkey
            and L.shipmode = 1 
            and L.shipinstruct = 0
            and ((

                P.brand = ?
                and (P.container & 3) = 0
                and L.quantity >= ? and L.quantity <= ? + 10
                and P.size between 1 and 5
            )
            or
            (
                P.brand = ?
                and (P.container & 3) = 1
                and L.quantity >= ? and L.quantity <= ? + 10
                and P.size between 1 and 10
            )
            or
            (
                P.brand = ?
                and (P.container & 3) = 2
                and L.quantity >= ? and L.quantity <= ? + 10
                and P.size between 1 and 15
            ));
        )sql";

        sqlite3_stmt * stmt;
        auto err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        if (err) {
            std::cerr << "sqlite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }

        sqlite3_bind_int(stmt, 1, BRAND1);
        sqlite3_bind_int(stmt, 2, QUANTITY1);
        sqlite3_bind_int(stmt, 3, QUANTITY1);
        sqlite3_bind_int(stmt, 4, BRAND2);
        sqlite3_bind_int(stmt, 5, QUANTITY2);
        sqlite3_bind_int(stmt, 6, QUANTITY2);
        sqlite3_bind_int(stmt, 7, BRAND3);
        sqlite3_bind_int(stmt, 8, QUANTITY3);
        sqlite3_bind_int(stmt, 9, QUANTITY3);

        sqlite3_step(stmt);
        auto sqlRev = sqlite3_column_int(stmt, 0);

        // std::cout << sqlRev << "\n";
        ASSERT_SAME(sqlRev, rev);
        single_cout("SQL OK");
    }
#endif

    sqlite3_close(sqlite_db);

    MPI_Finalize();
}