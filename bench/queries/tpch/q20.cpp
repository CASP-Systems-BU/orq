/**
 * @file q20.cpp
 * @brief Implements TPCH Q20
 * @date 2024-11-06
 * select
 *   s_name,
 * 	 s_address
 * from
 * 	 supplier, nation
 * where
 * 	 s_suppkey in (
 *     select
 * 	     ps_suppkey
 * 	   from
 *       partsupp
 * 	   where
 *       ps_partkey in (
 *         select
 *           p_partkey
 *         from
 *           part
 *         where
 *           p_name like '[COLOR]%'
 *       )
 *       and ps_availqty > (
 *         select
 *           0.5 * sum(l_quantity)
 *         from
 *           lineitem
 *         where
 *           l_partkey = ps_partkey
 *           and l_suppkey = ps_suppkey
 *           and l_shipdate >= date('[DATE]')
 *           and l_shipdate < date('[DATE]') + interval '1' year
 *     )
 * 	 )
 * 	 and s_nationkey = n_nationkey
 * 	 and n_name = '[NATION]'
 * order by
 * 	 s_name;
 */

#include "orq.h"
#include "tpch_dbgen.h"

// #define PRINT_TABLES

// #define QUERY_PROFILE

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using T = int64_t;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char **argv) {
    orq_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    const int NATION = 7;
    const int COLOR = 11;

    // arbitrary
    const int DATE = 42;
    // selected to give ~ correct output cardinality
    const int DATE_INTERVAL = 17;

    ////////////////////////////////////////////////////////////////
    // Database Initialization

    sqlite3 *sqlite_db = nullptr;
#ifndef QUERY_PROFILE
    if (pid == 0) {
        int err = sqlite3_open(NULL, &sqlite_db);
        if (err) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
    }
#endif

    auto db = TPCDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    single_cout("Q20 SF " << db.scaleFactor);

    ////////////////////////////////////////////////////////////////
    // Query

    auto S = db.getSupplierTable();
    auto PS = db.getPartSuppTable();
    auto P = db.getPartTable();
    auto N = db.getNationTable();
    auto L = db.getLineitemTable();

    S.project({"[SuppKey]", "[Name]", "[NationKey]", "[Name]", "[Address]"});
    PS.project({"AvailQty", "[PartKey]", "[SuppKey]"});
    L.project({"[ShipDate]", "[PartKey]", "[SuppKey]", "Quantity"});
    P.project({"[Name]", "[PartKey]"});
    N.project({"[NationKey]", "[Name]"});

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // => p_name like 'COLOR%'
    // for now, implement this as equality
    P.filter(P["[Name]"] == COLOR);
    P.project({"[PartKey]"});

    // => l_shipdate >= date('[DATE]') and l_shipdate < date('[DATE]') + interval
    L.filter(L["[ShipDate]"] >= DATE);
    L.filter(L["[ShipDate]"] < DATE + DATE_INTERVAL);
    L.deleteColumns({"[ShipDate]"});

    // => n_name = '[NATION]'
    N.filter(N["[Name]"] == NATION);
    N.project({"[NationKey]"});

    stopwatch::timepoint("filter");

    // PartSupp PK is compound: (PartKey, SuppKey)
    auto PSL = PS.inner_join(L, {"[PartKey]", "[SuppKey]"},
                             {{"AvailQty", "AvailQty", copy<A>}, {"Quantity", "Quantity", sum<A>}});

    stopwatch::timepoint("innerjoin");

    PSL.addColumns({"[Quantity]", "[AvailQty]"});
    PSL.convert_a2b("Quantity", "[Quantity]");
    PSL.convert_a2b("AvailQty", "[AvailQty]");
    PSL.deleteColumns({"AvailQty", "Quantity"});

    // AvailQty *= 2 (rather than doing division)
    PSL["[AvailQty]"] = PSL["[AvailQty]"] << 1;

    PSL.filter(PSL["[AvailQty]"] > PSL["[Quantity]"]);

    stopwatch::timepoint("quantity");

    // Run semijoins:

    // => s_nationkey = n_nationkey
    auto SN = S.semi_join(N, {"[NationKey]"});
    SN.deleteColumns({"[NationKey]"});

    // => s_suppkey in (select ... where ps_partkey in (select ...))
    auto T1 = PSL.semi_join(P, {"[PartKey]"});
    T1.project({"[SuppKey]"});
    // Breaking apart compound key, so may now have duplicate suppliers. Run
    // distinct to get back to unqiue key.
    T1.distinct({"[SuppKey]"});
    auto T2 = SN.semi_join(T1, {"[SuppKey]"});
    T2.deleteColumns({"[SuppKey]"});

    stopwatch::timepoint("semijoins");

    T2.sort({"[Name]"});
    stopwatch::timepoint("final sort");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    T2.finalize();
#endif

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    ////////////////////////////////////////////////////////////////
    // Correctness Test

#ifndef QUERY_PROFILE

    auto result = T2.open_with_schema();
    print_table(result, pid);

    if (pid == 0) {
        const char *query = R"sql(
        select
            s.name,
            s.address
        from
            supplier as s, nation as n
        where
            s.suppkey in (
                select
                    ps.suppkey
                from
                    partsupp as ps
                where
                    ps.partkey in (
                        select
                            partkey
                        from
                            part
                        where
                            part.name = ?
                    )
                and availqty > (
                    select
                        0.5 * sum(quantity)
                    from
                        lineitem as l
                    where
                        l.partkey = ps.partkey
                        and l.suppkey = ps.suppkey
                        and shipdate >= ?
                        and shipdate < ? + ?
                )
            )
            and s.nationkey = n.nationkey
            and n.name = ?
        order by
            s.name;
        )sql";

        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL)) {
            std::cerr << "sqlite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }

        sqlite3_bind_int(stmt, 1, COLOR);
        sqlite3_bind_int(stmt, 2, DATE);
        sqlite3_bind_int(stmt, 3, DATE);
        sqlite3_bind_int(stmt, 4, DATE_INTERVAL);
        sqlite3_bind_int(stmt, 5, NATION);

        auto name_col = T2.get_column(result, "[Name]");
        auto addr_col = T2.get_column(result, "[Address]");

        int i, err;
        for (i = 0; (err = sqlite3_step(stmt)) == SQLITE_ROW; i++) {
            int s_name = sqlite3_column_int(stmt, 0);
            int s_addr = sqlite3_column_int(stmt, 1);

            // std::cout << "a=" << s_addr << " n=" << s_name << "\n";
            assert(s_name == name_col[i]);
            assert(s_addr == addr_col[i]);
        }

        if (err != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        } else {
            assert(i == result.first[0].size());
            if (i == 0) {
                single_cout("Empty result");
            }
            std::cout << i << " rows OK\n";
        }
    }
#endif

    sqlite3_close(sqlite_db);
}