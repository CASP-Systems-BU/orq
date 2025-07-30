/**
 * @file q13.cpp
 * @brief Implements TPCH Query 13
 * @date 2024-04-01
 * 
 * Equivalent SQL:
 *  select
 *      c_count, count(*) as custdist
 *  from (
 *      select
 *          c_custkey,
 *          count(o_orderkey)
 *      from
 *          customer left outer join orders on
 *              c_custkey = o_custkey
 *              and o_comment not like ‘%[WORD1]%[WORD2]%’
 *      group by
 *          c_custkey
 *      ) as c_orders (c_custkey, c_count)
 *  group by
 *      c_count
 *  order by
 *      custdist desc,
 *      c_count desc;
 * 
 */

#include "../../../include/secrecy.h"
#include "../../../include/benchmark/stopwatch.h"

#include "tpch_dbgen.h"
#include <sys/time.h>

// #define PRINT_TABLES

// #define QUERY_PROFILE

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

using T = int64_t;

using sec = duration<float, seconds::period>;

int main(int argc, char** argv) {
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

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

    // TODO: expose this from runtime?
    using A = ASharedVector<T>;
    using B = BSharedVector<T>;

    single_cout("Q13 SF " << db.scaleFactor);

    auto C = db.getCustomersTable();
    auto O = db.getOrdersTable();

    C.project({"[CustKey]"});
    O.project({"[CustKey]", "[Comment]"});

    single_cout("C size: " << C.size());
    single_cout("O size: " << O.size());
    
    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    std::vector<std::string> extra_columns = {"Count"};
    O.addColumns(extra_columns, O.size());

    // [SQL] o_comment not like ‘%[WORD1]%[WORD2]%’
    O.filter(O["[Comment]"] != 0);

    O.deleteColumns({"[Comment]"});

    // print_table(O.open_with_schema(), pid);
    // print_table(C.open_with_schema(), pid);

    stopwatch::timepoint("Filter");

    /* [SQL]
     * select c_custkey, count(o_orderkey)
     * from customer join orders on c_custkey = o_custkey
     * 
     * NOTE: *outer* join handled below
     */
    auto T = C.left_outer_join(O, {"[CustKey]"}, {
        {"Count", "Count", count<A>},
    });

    stopwatch::timepoint("outer join");

    T.addColumns(std::vector<std::string>{"[Count]", "CustDist"}, T.size());
    T.convert_a2b("Count", "[Count]");

    // [SQL] select c_count, count(*) as custdist ... group by c_count
    auto F = T.aggregate({ENC_TABLE_VALID, "[Count]"}, {
        {"CustDist", "CustDist", count<A>}
    }); // don't reverse, do sort

    // TODO: subsume this functionality into aggregate
    F.addColumns({ENC_TABLE_UNIQ, "[CustDist]"});
    F.distinct({"[Count]"}, ENC_TABLE_UNIQ);
    F.filter(F[ENC_TABLE_UNIQ]);

    stopwatch::timepoint("Distribution");
    F.convert_a2b("CustDist", "[CustDist]");
    F.deleteColumns({"[CustKey]", "Count", ENC_TABLE_UNIQ, "CustDist"});

    F.sort({"[CustDist]", "[Count]"}, DESC);

    stopwatch::timepoint("Final sort");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    F.finalize();
#endif

    stopwatch::done();
    stopwatch::profile_done();

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    auto Fo = F.open_with_schema();
    print_table(Fo, pid);

    auto out_count = F.get_column(Fo, "[Count]");
    auto out_dist = F.get_column(Fo, "[CustDist]");

    if (pid == 0) {
        const char * query = R"sql(
        select
            OrderCount, count(*) as CustDist
        from (
            select
                c.custkey,
                count(o.orderkey) as OrderCount
            from
                customer c left outer join orders o on
                    c.custkey = o.custkey
                    and o.comment <> 0
            group by
                c.custkey
            )
        group by
            OrderCount
        order by
            Custdist desc,
            OrderCount desc;
        )sql";

        sqlite3_stmt * stmt;
        auto err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        if (err) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }

        int i;
        for (i = 0; sqlite3_step(stmt) == SQLITE_ROW; i++) {
            auto order_count = sqlite3_column_int(stmt, 0);
            auto distr = sqlite3_column_int(stmt, 1);

            ASSERT_SAME(distr, out_dist[i]);
            ASSERT_SAME(order_count, out_count[i]);
        }
        ASSERT_SAME(i, out_dist.size());
        if (i == 0) {single_cout("Empty result");}
        single_cout(i << " rows OK");
    }
#endif
    sqlite3_close(sqlite_db);

    MPI_Finalize();

    return 0;
}