/**
 * @file q21.cpp
 * @brief Implements TPCH Q21
 * @date 2024-09-06
 *
 * Equivalent SQL:
 *
 *
 * select
 *   s_name,
 *   count(*) as numwait
 * from
 *   supplier,
 *   lineitem l1,
 *   orders,
 *   nation
 * where
 *   s_suppkey = l1.l_suppkey
 *   and o_orderkey = l1.l_orderkey
 *   and o_orderstatus = 'F'
 *   and l1.l_receiptdate > l1.l_commitdate
 *   and exists (
 *     select
 *       *
 *     from
 *       lineitem l2
 *     where
 *       l2.l_orderkey = l1.l_orderkey
 *       and l2.l_suppkey <> l1.l_suppkey
 *      )
 *   and not exists (
 *     select
 *       *
 *     from
 *       lineitem l3
 *     where
 *       l3.l_orderkey = l1.l_orderkey
 *       and l3.l_suppkey <> l1.l_suppkey
 *       and l3.l_receiptdate > l3.l_commitdate
 *     )
 *     and s_nationkey = n_nationkey
 *     and n_name = '[NATION]'
 * group by
 *   s_name
 * order by
 *   numwait desc,
 *   s_name;
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

    // arbitrary nation. this needs to be odd lol
    const int NATION = 9;

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

    single_cout("Q21 SF " << db.scaleFactor);

    ////////////////////////////////////////////////////////////////
    // Query

    auto S = db.getSupplierTable();
    auto L1 = db.getLineitemTable();
    auto L2 = L1.deepcopy();
    auto O = db.getOrdersTable();
    auto N = db.getNationTable();

    S.project({"[Name]", "[SuppKey]", "[NationKey]"});
    L1.project({"[SuppKey]", "[OrderKey]", "[ReceiptDate]", "[CommitDate]"});
    L2.project({"[SuppKey]", "[OrderKey]", "[ReceiptDate]", "[CommitDate]"});
    O.project({"[OrderKey]", "[OrderStatus]"});
    N.project({"[NationKey]", "[Name]"});

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // Filters //
    L1.filter(L1["[ReceiptDate]"] > L1["[CommitDate]"]);
    O.filter(O["[OrderStatus]"] == 0);
    N.filter(N["[Name]"] == NATION);

    stopwatch::timepoint("filters");

    L1.project({"[SuppKey]", "[OrderKey]"});
    O.project({"[OrderKey]"});
    N.project({"[NationKey]"});
    S.project({"[Name]", "[SuppKey]", "[NationKey]"});

    stopwatch::timepoint("project");

    // Joins //
    // Chain each join together by a PK-FK relationship
    auto OL = O.inner_join(L1, {"[OrderKey]"});
    auto NS = N.inner_join(S, {"[NationKey]"});

    // NS has s_name and s_nationkey, which need to be propagated
    auto NSOL =
        NS.inner_join(OL, {"[SuppKey]"},
                      {{"[NationKey]", "[NationKey]", copy<B>}, {"[Name]", "[Name]", copy<B>}});

    NSOL.deleteColumns({"[NationKey]"});

    NSOL.addColumns({"OrderCount"});

    // Count the number of orders this supplier was late on
    NSOL.aggregate({"[OrderKey]", "[SuppKey]"}, {{"OrderCount", "OrderCount", count<A>}});

    stopwatch::timepoint("joins");

    // exists / not exists //
    // find suppliers who were the ONLY late supplier in their order
    L2.addColumn("[Late]");
    L2["[Late]"] = L2["[ReceiptDate]"] > L2["[CommitDate]"];
    L2.project({"[Late]", "[SuppKey]", "[OrderKey]"});

    L2.sort({ENC_TABLE_VALID, "[OrderKey]", "[SuppKey]"});
    L2.addColumn("[AnyLate]");

    // TODO: need to aggregate? maybe just SORT on late

    // Find suppliers who had any late lineitem for a given order (suppliers may
    // have multiple lineitems in a given order).
    // `bitmax` computes any(); i.e. bitwise OR over a group.
    L2.aggregate({"[OrderKey]", "[SuppKey]"}, {{"[Late]", "[AnyLate]", bit_or<B>}},
                 {.do_sort = false});

    L2.addColumns({"NumLate", "CountSupp", "AnyLate"});

    L2.convert_b2a_bit("[AnyLate]", "AnyLate");
    // 1 -> 0xffffffff so we can use as bitmask
    L2.extend_lsb("[AnyLate]");

    // Mark the late suppliers.
    L2["[SuppKey]"] &= L2["[AnyLate]"];

    L2.deleteColumns({"[AnyLate]"});

    // Only late suppliers have a nonzero MaskSuppKey, so compute bit_or to find
    // the single late supplier (in the case where this is the only one)
    // Semantically this is just a selection based on AnyLate, but this is more
    // efficient.
    //
    // Aggregate over OrderKey: for valid rows, SuppKey will be left with the
    // identity of the only late supplier.
    //
    // Self reference [SuppKey] is ok here because the aggregation is monotonic
    // and for valid rows will only ever go from 0 to a single SuppKey.
    L2.aggregate({"[OrderKey]"}, {
                                     {"AnyLate", "NumLate", sum<A>},
                                     {"CountSupp", "CountSupp", count<A>},
                                     {"[SuppKey]", "[SuppKey]", bit_or<B>},
                                 });

    L2.addColumns({"[CountSupp]", "[NumLate]"});
    L2.convert_a2b("CountSupp", "[CountSupp]");
    L2.convert_a2b("NumLate", "[NumLate]");

    // Extract suppliers who were the only late supplier
    L2.filter(L2["[NumLate]"] == 1 & L2["[CountSupp]"] > 1);

    L2.project({"[OrderKey]", "[SuppKey]"});

    stopwatch::timepoint("exists / not exists");

    // Find intersection (valid order-supplier pairs from NSOL filters above;
    // exists/noexists from L2).
    auto W = L2.inner_join(NSOL, {"[OrderKey]", "[SuppKey]"});

    W.addColumns({"NumWait", "[NumWait]"});

    // Counter the number of orders each supplier has waiting on them
    W.aggregate({"[Name]"}, {{"OrderCount", "NumWait", sum<A>}});

    // Group by supplier attribute: at most the size of supplier table.
    W.sort({ENC_TABLE_VALID});
    W.tail(S.size());

    W.deleteColumns({"OrderCount"});

    stopwatch::timepoint("final join");

    W.convert_a2b("NumWait", "[NumWait]");
    W.project({"[NumWait]", "[Name]"});
    W.sort({std::make_pair("[NumWait]", DESC), {"[Name]", ASC}});

    stopwatch::timepoint("sort");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    W.finalize();
#endif

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    ////////////////////////////////////////////////////////////////
    // Correctness Test

#ifndef QUERY_PROFILE

    auto result = W.open_with_schema();

    print_table(result, pid);

    if (pid == 0) {
        const char *query = R"sql(
        select
            supplier.name,
            count(*) as numwait
        from
            supplier,
            lineitem l1,
            orders,
            nation
        where
            supplier.suppkey = l1.suppkey
            and orders.orderkey = l1.orderkey
            and orderstatus = 0
            and l1.receiptdate > l1.commitdate
            and exists (
                select
                    *
                from
                    lineitem l2
                where
                    l2.orderkey = l1.orderkey
                    and l2.suppkey <> l1.suppkey
            )
            and not exists (
                select
                    *
                from
                    lineitem l3
                where
                    l3.orderkey = l1.orderkey
                    and l3.suppkey <> l1.suppkey
                    and l3.receiptdate > l3.commitdate
            )
            and supplier.nationkey = nation.nationkey
            and nation.name = ?
        group by
            supplier.name
        order by
            numwait desc,
            supplier.name;  
        )sql";

        sqlite3_stmt *stmt;
        auto err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);

        if (err) {
            std::cerr << "sqlite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        }

        sqlite3_bind_int(stmt, 1, NATION);

        auto name_col = W.get_column(result, "[Name]");
        auto numwait_col = W.get_column(result, "[NumWait]");

        int i;
        // std::cout << "Supp\tNumWait\n";
        for (i = 0; (err = sqlite3_step(stmt)) == SQLITE_ROW; i++) {
            int s_name = sqlite3_column_int(stmt, 0);
            int num_wait = sqlite3_column_int(stmt, 1);

            // std::cout << s_name << "\t" << num_wait << "\n";
            ASSERT_SAME(s_name, name_col[i]);
            ASSERT_SAME(num_wait, numwait_col[i]);
        }
        ASSERT_SAME(i, name_col.size());
        if (i == 0) {
            single_cout("Empty result");
        }

        if (err != SQLITE_DONE) {
            std::cerr << "SQLite error: " << sqlite3_errmsg(sqlite_db) << "\n";
        } else {
            std::cout << i << " rows OK\n";
        }
    }
#endif

    sqlite3_close(sqlite_db);
}