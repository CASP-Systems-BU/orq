/**
 * @file q11.cpp
 * @brief TPCH Query 11
 *
 * Equivalent SQL:
 *  select
 *      ps_partkey,
 *      sum(ps_supplycost * ps_availqty) as value
 *  from
 *      partsupp,
 *      supplier,
 *      nation
 *  where
 *      ps_suppkey = s_suppkey
 *      and s_nationkey = n_nationkey
 *      and n_name = '[NATION]'
 *  group by
 *      ps_partkey
 *  having
 *      sum(ps_supplycost * ps_availqty) > (
 *          select
 *              sum(ps_supplycost * ps_availqty) * [FRACTION]
 *          from
 *              partsupp,
 *              supplier,
 *              nation
 *          where
 *              ps_suppkey = s_suppkey
 *              and s_nationkey = n_nationkey
 *              and n_name = '[NATION]'
 *      )
 *  order by
 *      value desc;
 *
 */

#include <sys/time.h>

#include "orq.h"
#include "profiling/stopwatch.h"
#include "tpch_dbgen.h"

// #define PRINT_TABLES

// #define QUERY_PROFILE

#ifndef PRINT_TABLES
#define print_table(...)
#endif

using namespace orq::debug;
using namespace orq::service;
using namespace orq::operators;
using namespace std::chrono;
using namespace orq::aggregators;
using namespace orq::benchmarking;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using T = int64_t;

using sec = duration<float, seconds::period>;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    // TPCH Q11 query parameters
    const int NATION_NAME = 3;
    const float FRACTION = 0.0001 / sf;  // As per spec

    // Value to divide by to get the equivalent of the required fraction multiplication.
    // Round to integer since we don't currently support fixed/floating point operations.
    const int DIVIDE = 1 / FRACTION;

    ////////////////////////////////////////////////////////////////
    // Database Initialization

    // Setup SQLite DB for output validation
    sqlite3* sqlite_db = nullptr;
#ifndef QUERY_PROFILE
    if (pid == 0) {
        int err = sqlite3_open(NULL, &sqlite_db);  // NULL -> Create in-memory database
        if (err) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        } else {
            single_cout("SQLite DB created");
        }
    }
#endif

    // Query DB setup
    auto db = TPCDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    single_cout("Q11 SF " << db.scaleFactor << " | FRACTION " << FRACTION << " | DIVIDE "
                          << DIVIDE);

    ////////////////////////////////////////////////////////////////
    // Query

    auto PartSupp = db.getPartSuppTable();
    auto Supplier = db.getSupplierTable();
    auto Nation = db.getNationTable();

    PartSupp.project({"[SuppKey]", "[PartKey]", "[SupplyCost]", "SupplyCost", "AvailQty"});
    Supplier.project({"[SuppKey]", "[NationKey]"});
    Nation.project({"[NationKey]", "[Name]"});

#ifdef PRINT_TABLES
    single_cout("PARTSUPP, size " << db.partSuppSize());
    print_table(PartSupp.open_with_schema(), pid);

    single_cout("SUPPLIER, size " << db.supplierSize());
    print_table(Supplier.open_with_schema(), pid);

    single_cout("NATION, size " << db.nationSize());
    print_table(Nation.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // [SQL] and n_name = '[NATION]'
    Nation.filter(Nation["[Name]"] == NATION_NAME);

    stopwatch::timepoint("Nation filter");

    // [SQL] and s_nationkey = n_nationkey
    auto NationSupplier = Nation.inner_join(Supplier, {"[NationKey]"}, {});

    stopwatch::timepoint("NationKey Join");

    // [SQL] ps_suppkey = s_suppkey
    auto MainQuery = NationSupplier.inner_join(PartSupp, {"[SuppKey]"}, {});

    stopwatch::timepoint("SuppKey Join");

    MainQuery.addColumns({"Value"}, MainQuery.size());
    // [SQL] (ps_supplycost * ps_availqty)
    MainQuery["Value"] = MainQuery["SupplyCost"] * MainQuery["AvailQty"];
    // Deepcopy into SubQuery instead of performing the same join twice
    auto SubQuery = MainQuery.deepcopy();

    stopwatch::timepoint("Multiplication");

    // [SQL] sum(ps_supplycost * ps_availqty) * [FRACTION]
    SubQuery.addColumns({"SumValue", "SumFraction", "[SumFraction]"}, SubQuery.size());
    SubQuery.convert_b2a_bit(ENC_TABLE_VALID, "SumValue");
    SubQuery["SumValue"] *= SubQuery["Value"];
    SubQuery.prefix_sum("SumValue");

    // Global aggregation - only one row left.
    SubQuery.tail(1);
    SubQuery["SumFraction"] = SubQuery["SumValue"] / DIVIDE;
    SubQuery.convert_a2b("SumFraction", "[SumFraction]");
    SubQuery.project({"SumValue", "[SumFraction]"});
    SubQuery.configureValid();  // revalidate the result row.
    // print_table(SubQuery.open_with_schema(), pid);

    stopwatch::timepoint("SubQuery Agg");

    // [SQL] sum(ps_supplycost * ps_availqty) as value
    // [SQL] group by ps_partkey
    MainQuery.addColumns({"SumValue", "[SumValue]"}, MainQuery.size());
    MainQuery.aggregate({"[PartKey]"}, {{"Value", "SumValue", sum<A>}});
    MainQuery.convert_a2b("SumValue", "[SumValue]");
    MainQuery.project({"[PartKey]", "[SumValue]"});

    stopwatch::timepoint("MainQuery Agg");

    // [SQL] having
    // Join without a key (i.e. over ENC_TABLE_VALID) to copy the result of
    // the subquery to every row in the main query
    auto CompoundQuery =
        SubQuery.inner_join(MainQuery, {}, {{"[SumFraction]", "[SumFraction]", copy<B>}});

    stopwatch::timepoint("HAVING Join");

    // [SQL] sum(...) > (...)
    CompoundQuery.filter(CompoundQuery["[SumValue]"] > CompoundQuery["[SumFraction]"]);

    stopwatch::timepoint("HAVING Filter");

    // [SQL] order by value desc
    CompoundQuery.sort({"[SumValue]"}, DESC);

    stopwatch::timepoint("Order by");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    CompoundQuery.finalize();
#endif

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    ////////////////////////////////////////////////////////////////
    // Correctness Test

#ifndef QUERY_PROFILE

    auto result = CompoundQuery.open_with_schema();
    auto partKey = CompoundQuery.get_column(result, "[PartKey]");
    auto value = CompoundQuery.get_column(result, "[SumValue]");

    if (pid == 0) {
        // Fetch Q11 SQL result to validate
        // Note: Dividing by DIVIDE instead of multiplying by FRACTION to match the
        // MPC implementation (especially rounding behavior).
        int ret;
        const char* query = R"sql(
            select
                ps.partkey,
                sum(ps.supplycost * ps.availqty) as value
            from
                partsupp as ps,
                supplier as s,
                nation as n
            where
                ps.suppkey = s.suppkey
                and s.nationkey = n.nationkey
                and n.name = ?
            group by
                ps.partkey 
            having
                sum(ps.supplycost * ps.availqty) > (
                    select
                        sum(ps.supplycost * ps.availqty) / ?
                    from
                        partsupp as ps,
                        supplier as s,
                        nation as n
                    where
                        ps.suppkey = s.suppkey
                        and s.nationkey = n.nationkey
                        and n.name = ?
                )
            order by
                value desc;
        )sql";

        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, NATION_NAME);
        sqlite3_bind_double(stmt, 2, DIVIDE);
        sqlite3_bind_int(stmt, 3, NATION_NAME);

        int result_idx = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
            int sqlPartKey = sqlite3_column_int(stmt, 0);
            int sqlValue = sqlite3_column_int(stmt, 1);

            // std::cout << "PartKey: " << sqlPartKey << " | " << "Value: " << sqlValue <<
            // std::endl; std::cout << "PartKey: " << partKey[result_idx] << " | " << "Value: " <<
            // value[result_idx] << std::endl; std::cout << std::endl;

            ASSERT_SAME(partKey[result_idx], sqlPartKey);
            ASSERT_SAME(value[result_idx], sqlValue);

            result_idx++;
        }

        // Since the above assertions only check the number of rows returned by SQL,
        // also assert the two result sizes
        ASSERT_SAME(result_idx, partKey.size());
        if (result_idx == 0) {
            single_cout("Empty result");
        }

        single_cout("Result assertions successful");

        // single_cout("Calculated result size: " << result_idx);
        // single_cout("SQL query result  size: " << partKey.size() << std::endl);

        if (ret != SQLITE_DONE) {
            single_cout("Error executing statement: " << sqlite3_errmsg(sqlite_db));
        }
    }

#endif
    // Close SQLite DB
    sqlite3_close(sqlite_db);

    return 0;
}
