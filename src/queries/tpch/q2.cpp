/**
 * @file q2.cpp
 * @brief Implements TPCH Query 2
 * @date 2024-08-15
 * 
 * Equivalent SQL:
 * select
 *     s_acctbal,
 *     s_name,
 *     n_name,
 *     p_partkey,
 *     p_mfgr,
 *     s_address,
 *     s_phone,
 *     s_comment
 * from
 *     part,
 *     supplier,
 *     partsupp,
 *     nation,
 *     region
 * where
 *     p_partkey = ps_partkey
 *     and s_suppkey = ps_suppkey
 *     and p_size = [SIZE]
 *     and p_type like '%[TYPE]'
 *     and s_nationkey = n_nationkey
 *     and n_regionkey = r_regionkey
 *     and r_name = '[REGION]'
 *     and ps_supplycost = (
 *         select
 *             min(ps_supplycost)
 *         from
 *             partsupp, supplier,
 *             nation, region
 *         where
 *             p_partkey = ps_partkey
 *             and s_suppkey = ps_suppkey
 *             and s_nationkey = n_nationkey
 *             and n_regionkey = r_regionkey
 *             and r_name = '[REGION]'
 *     )
 * order by
 *     s_acctbal desc,
 *     n_name,
 *     s_name,
 *     p_partkey;
 *
 * Ignores s_address, s_phone, s_comment, and p_mfgr because they aren't semantically interesting
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
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    // TPCH Q2 query parameters
    const int SIZE = 4;
    const int TYPE = 5;
    const int REGION = 1;

    // Setup SQLite DB for output validation
    sqlite3 *sqlite_db = nullptr;
#ifndef QUERY_PROFILE
    if (pid == 0) {
        int err = sqlite3_open(NULL, &sqlite_db); // NULL -> Create in-memory database
        if(err) {
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

    single_cout("Q2 SF " << db.scaleFactor);

    auto Part = db.getPartTable();
    auto Supplier = db.getSupplierTable();
    auto PartSupp = db.getPartSuppTable();
    auto Nation = db.getNationTable();
    auto Region = db.getRegionTable();  

    Part.project({"[PartKey]", "[Size]", "[Type]"});
    Supplier.project({"[SuppKey]", "[NationKey]", "[AcctBal]", "[Name]"});
    PartSupp.project({"[PartKey]", "[SuppKey]", "[SupplyCost]"});
    Nation.project({"[NationKey]", "[RegionKey]", "[Name]"});
    Region.project({"[RegionKey]", "[Name]"});

#ifdef PRINT_TABLES
    single_cout("PART, size " << db.partSize());
    print_table(Part.open_with_schema(), pid);

    single_cout("SUPPLIER, size " << db.supplierSize());
    print_table(Supplier.open_with_schema(), pid);

    single_cout("PARTSUPP, size " << db.partSuppSize());
    print_table(PartSupp.open_with_schema(), pid);

    single_cout("NATION, size " << db.nationSize());
    print_table(Nation.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // and p_size = [SIZE]
    // and p_type like '%[TYPE]'
    Part.filter(Part["[Size]"] == SIZE & Part["[Type]"] == TYPE);
    Part.deleteColumns({"[Size]", "[Type]"});

    // and r_name = '[REGION]'
    Region.filter(Region["[Name]"] == REGION);

    // and n_regionkey = r_regionkey
    auto SelectedNations = Region.inner_join(Nation, {"[RegionKey]"});
    SelectedNations.project({"[NationKey]", "[Name]"});

    stopwatch::timepoint("Filters");

    SelectedNations.addColumn("[N_Name]", Nation.size());
    auto SelectedSuppliers = SelectedNations.inner_join(Supplier, {"[NationKey]"}, {
        {"[Name]", "[N_Name]", copy<B>}
    });
    SelectedSuppliers.deleteColumns({"[NationKey]"});

    auto SelectedPartSupplies = SelectedSuppliers.inner_join(PartSupp, {"[SuppKey]"}, {
            {"[N_Name]", "[N_Name]", copy<B>}, // Nation name
            {"[Name]", "[Name]", copy<B>}, // Supplier name
            {"[AcctBal]", "[AcctBal]", copy<B>}
        });
    SelectedPartSupplies.deleteColumns({"[SuppKey]"});

    auto GroupedByPart = Part.inner_join(SelectedPartSupplies, {"[PartKey]"});

    stopwatch::timepoint("Joins");

    // sort on valid to move invalid columns to the bottom
    GroupedByPart.sort({ENC_TABLE_VALID}, DESC);

    GroupedByPart.addColumn("[MinSupplyCost]", GroupedByPart.size());
    GroupedByPart.aggregate({"[PartKey]"}, {
        {"[SupplyCost]", "[MinSupplyCost]", min<B>}
    }, {
        .reverse = false,
        .do_sort = false,
        .mark_valid = false
    });

    GroupedByPart.aggregate({"[PartKey]"}, {
        {"[MinSupplyCost]", "[MinSupplyCost]", copy<B>}
    }, {
        .reverse = true,
        .do_sort = false
    });

    GroupedByPart.filter(GroupedByPart["[SupplyCost]"] == GroupedByPart["[MinSupplyCost]"]);
    GroupedByPart.deleteColumns({"[SupplyCost]", "[MinSupplyCost]"});

    stopwatch::timepoint("Min Aggregation + Filter");

    GroupedByPart.sort({
            std::make_pair(ENC_TABLE_VALID, DESC),
            std::make_pair("[AcctBal]", DESC),
            std::make_pair("[N_Name]", ASC),
            std::make_pair("[Name]", ASC),
            std::make_pair("[PartKey]", ASC)
        });

    GroupedByPart.head(100);

    stopwatch::timepoint("Sort");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    GroupedByPart.finalize();
#endif

    stopwatch::done(); // print wall clock time
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE
    
    auto resultOpened = GroupedByPart.open_with_schema();
    auto acctbal_col = GroupedByPart.get_column(resultOpened, "[AcctBal]");
    auto supp_name_col = GroupedByPart.get_column(resultOpened, "[Name]");
    auto nation_name_col = GroupedByPart.get_column(resultOpened, "[N_Name]");
    auto partkey_col = GroupedByPart.get_column(resultOpened, "[PartKey]");

    // print_table(resultOpened, pid);

    if (pid == 0) {
        // Run Q2 through SQL to verify result
        int ret;
        const char* query = R"sql(
            select
                s.AcctBal,
                s.Name,
                n.Name,
                p.PartKey
            from
                PART as p,
                SUPPLIER as s,
                PARTSUPP as ps,
                NATION as n,
                REGION as r
            where
                p.PartKey = ps.PartKey
                and s.SuppKey = ps.SuppKey
                and p.Size = ?
                and p.Type = ?
                and s.NationKey = n.NationKey
                and n.RegionKey = r.RegionKey
                and r.Name = ?
                and ps.SupplyCost = (
                    select
                        min(ps.SupplyCost)
                    from
                        PARTSUPP as ps, SUPPLIER as s,
                        NATION as n, REGION as r
                    where
                        p.PartKey = ps.PartKey
                        and s.SuppKey = ps.SuppKey
                        and s.NationKey = n.NationKey
                        and n.RegionKey = r.RegionKey
                        and r.Name = ?
                )
            order by
                s.AcctBal desc,
                n.Name,
                s.Name,
                p.PartKey
            limit 100
        )sql";
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, SIZE);
        sqlite3_bind_int(stmt, 2, TYPE);
        sqlite3_bind_int(stmt, 3, REGION);
        sqlite3_bind_int(stmt, 4, REGION);

        // Assert result against SQL result
        auto res = sqlite3_step(stmt);
        auto i = 0;
        while (res == SQLITE_ROW) {
            int sql_acctbal = sqlite3_column_int(stmt, 0);
            int sql_supp_name = sqlite3_column_int(stmt, 1);
            int sql_nation_name = sqlite3_column_int(stmt, 2);
            int sql_partkey = sqlite3_column_int(stmt, 3);

            ASSERT_SAME (sql_acctbal, acctbal_col[i]);
            ASSERT_SAME (sql_supp_name, supp_name_col[i]);
            ASSERT_SAME (sql_nation_name, nation_name_col[i]);
            ASSERT_SAME (sql_partkey, partkey_col[i]);

            res = sqlite3_step(stmt);
            ++i;
        }
        if (res == SQLITE_ERROR) {;
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
        if (res != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }
        ASSERT_SAME(i, acctbal_col.size());
        if (i == 0) {single_cout("Empty result");}

        std::cout << i << " rows OK\n";
    }

    // Close SQLite DB
    sqlite3_close(sqlite_db);
#endif

    MPI_Finalize();
    return 0;
}
