/**
 * @file q17.cpp
 * @brief TPCH Query 17
 *
 * Equivalent SQL:
 *  select
 *      sum(l_extendedprice) / 7.0 as avg_yearly
 *  from
 *      lineitem,
 *      part
 *  where
 *      p_partkey = l_partkey
 *      and p_brand = '[BRAND]'
 *      and p_container = '[CONTAINER]'
 *      and l_quantity < (
 *          select
 *              0.2 * avg(l_quantity)
 *          from
 *              lineitem
 *          where
 *              l_partkey = p_partkey
 *      );
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

    // TPCH Q17 query parameters
    const int BRAND = 10;
    const int CONTAINER = 10;

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

    ////////////////////////////////////////////////////////////////
    // Database Initialization

    // Query DB setup
    auto db = TPCDatabase<T>(sf, sqlite_db);

    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    single_cout("Q17 SF " << db.scaleFactor);

    ////////////////////////////////////////////////////////////////
    // Query

    auto LineItem = db.getLineitemTable();
    auto Part = db.getPartTable();

    LineItem.project({"[PartKey]", "Quantity", "ExtendedPrice"});
    Part.project({"[PartKey]", "[Brand]", "[Container]"});

#ifdef PRINT_TABLES
    single_cout("LINEITEM, size " << db.lineitemsSize());
    print_table(LineItem.open_with_schema(), pid);

    single_cout("PART, size " << db.partSize());
    print_table(Part.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // [SQL] p_partkey = l_partkey
    auto PartLineItemJoin =
        Part.inner_join(LineItem, {"[PartKey]"},
                        {{"[Brand]", "[Brand]", copy<B>}, {"[Container]", "[Container]", copy<B>}});
    // [SQL] l_partkey = p_partkey (Subquery)
    auto SubQueryJoin = PartLineItemJoin.deepcopy();

    stopwatch::timepoint("Join");

    // [SQL] and p_brand = '[BRAND]'
    // [SQL] and p_container = '[CONTAINER]'
    PartLineItemJoin.filter(PartLineItemJoin["[Brand]"] == BRAND);
    PartLineItemJoin.filter(PartLineItemJoin["[Container]"] == CONTAINER);

    stopwatch::timepoint("Part filters");

    // [SQL] 0.2 * avg(l_quantity)
    std::vector<std::string> sub_join_extra_columns = {"SumQuantity", "CountQuantity",
                                                       "[AvgQuantity]"};
    SubQueryJoin.addColumns(sub_join_extra_columns, SubQueryJoin.size());
    SubQueryJoin.aggregate({"[PartKey]"}, {{"Quantity", "SumQuantity", sum<A>},
                                           {"Quantity", "CountQuantity", count<A>}});
    // After aggregation, SubQuery join has at most one row per Part. This will
    // do orders of magnitude fewer divisions below.
    SubQueryJoin.sort({ENC_TABLE_VALID});
    SubQueryJoin.tail(Part.size());

    SubQueryJoin["SumQuantity"] = SubQueryJoin["SumQuantity"] / 5;
    SubQueryJoin["[AvgQuantity]"] = SubQueryJoin["SumQuantity"] / SubQueryJoin["CountQuantity"];

    stopwatch::timepoint("Subquery Agg");

    // [SQL] Subquery constructed as a group-by + join to replicate semantics of the original query
    auto PartSubQueryJoin = SubQueryJoin.inner_join(PartLineItemJoin, {"[PartKey]"},
                                                    {{"[AvgQuantity]", "[AvgQuantity]", copy<B>}});

    stopwatch::timepoint("Subquery Join");

    std::vector<std::string> join_extra_columns = {"[Quantity]", "SumExtendedPrice"};
    PartSubQueryJoin.addColumns(join_extra_columns, PartSubQueryJoin.size());
    PartSubQueryJoin.convert_a2b("Quantity", "[Quantity]");

    // [SQL] and l_quantity < (Subquery)
    PartSubQueryJoin.filter(PartSubQueryJoin["[Quantity]"] < PartSubQueryJoin["[AvgQuantity]"]);

    stopwatch::timepoint("Join filters");

    // [SQL] sum(l_extendedprice) / 7.0 as avg_yearly
    PartSubQueryJoin.convert_b2a_bit(ENC_TABLE_VALID, "SumExtendedPrice");
    PartSubQueryJoin["SumExtendedPrice"] =
        PartSubQueryJoin["SumExtendedPrice"] * PartSubQueryJoin["ExtendedPrice"];
    PartSubQueryJoin.prefix_sum("SumExtendedPrice");
    PartSubQueryJoin.tail(1);
    PartSubQueryJoin["SumExtendedPrice"] = PartSubQueryJoin["SumExtendedPrice"] / 7;

    stopwatch::timepoint("Final Agg");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    PartSubQueryJoin.finalize();
#endif

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    ////////////////////////////////////////////////////////////////
    // Correctness Test

#ifndef QUERY_PROFILE

    auto result_column =
        PartSubQueryJoin.get_column(PartSubQueryJoin.open_with_schema(false), "SumExtendedPrice");
    size_t result = 0;
    if (result_column.size() > 0) {
        result = result_column[0];
    }

    if (pid == 0) {
        // Fetch Q17 SQL result to validate
        int ret;
        const char* query = R"sql(
            select
                sum(l.ExtendedPrice) / 7 as avg_yearly
            from
                LINEITEM l,
                PART p
            where
                p.PartKey = l.PartKey
                and p.Brand = ?
                and p.Container = ?
                and l.Quantity < (
                    select
                        CAST((0.2 * avg(l2.Quantity)) AS INTEGER)
                    from
                        LINEITEM as l2
                    where
                        l2.PartKey = p.PartKey
                );
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, BRAND);
        sqlite3_bind_int(stmt, 2, CONTAINER);

        size_t sqlResult;
        ret = sqlite3_step(stmt);
        if (ret != SQLITE_ROW) {
            std::cerr << "Execution failed: " << sqlite3_errmsg(sqlite_db) << std::endl;
        } else {
            sqlResult = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);

        // single_cout(std::endl);
        // single_cout("Calculated result | Average yearly: " << result);
        // single_cout("SQL Query result  | Average yearly: " << sqlResult << std::endl);

        ASSERT_SAME(sqlResult, result);
    }

#endif

    // Close SQLite DB
    sqlite3_close(sqlite_db);

    return 0;
}
