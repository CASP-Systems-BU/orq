/**
 * @file q16.cpp
 * @brief Implements TPCH Query 16
 * 
 * Equivalent SQL:
 *  select
 *      p_brand,
 *      p_type,
 *      p_size,
 *   	count(distinct ps_suppkey) as supplier_cnt
 *  from
 *  	partsupp,
 *  	part
 *  where
 *  	p_partkey = ps_partkey
 *  	and p_brand <> '[BRAND]'
 *  	and p_type not like '[TYPE]%'
 *  	and p_size in ([SIZE1], [SIZE2], [SIZE3], [SIZE4], [SIZE5], [SIZE6], [SIZE7], [SIZE8])
 *  	and ps_suppkey not in (
 *  		select
 *  			s_suppkey
 *  		from
 *  			supplier
 *  		where
 *  			s_comment like '%Customer%Complaints%'
 *  	)
 *  	group by
 *  		p_brand,
 *  		p_type,
 *  		p_size
 *  	order by
 *  		supplier_cnt desc,
 *  		p_brand,
 *  		p_type,
 *  		p_size;
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
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);
    auto pid = runTime->getPartyID();

    float sf = 0.01;
    if (argc >= 5) {
        sf = strtod(argv[4], NULL);
    }

    // TPCH Q16 query parameters
    const int BRAND = 10;
    const int TYPE = 10;
    const int COMMENT = 0;
    const int SIZE1 = 1;
    const int SIZE2 = 2;
    const int SIZE3 = 3;
    const int SIZE4 = 4;
    const int SIZE5 = 5;
    const int SIZE6 = 6;
    const int SIZE7 = 7;
    const int SIZE8 = 8;

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

    single_cout("Q16 SF " << db.scaleFactor);

    auto Part = db.getPartTable();
    auto PartSupp = db.getPartSuppTable();
    auto Supplier = db.getSupplierTable();

    Part.project({"[PartKey]", "[Brand]", "[Type]", "[Size]"});
    PartSupp.project({"[SuppKey]", "[PartKey]"});
    Supplier.project({"[SuppKey]", "[Comment]"});

#ifdef PRINT_TABLES
    single_cout("PART, size " << db.partSize());
    print_table(Part.open_with_schema(), pid);

    single_cout("PARTSUPP, size " << db.partSuppSize());
    print_table(PartSupp.open_with_schema(), pid);

    single_cout("SUPPLIER, size " << db.supplierSize());
    print_table(Supplier.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // s_comment == COMMENT
    Supplier.filter(Supplier["[Comment]"] == COMMENT);
    // project, i.e. keep s_suppkey
    Supplier.deleteColumns({"[Comment]"});
    // no need to project any columns from left
    auto PartSuppSupplierJoin = PartSupp.anti_join(Supplier, {"[SuppKey]"});

    // p_size in ([SIZE1], [SIZE2], [SIZE3], [SIZE4], [SIZE5], [SIZE6], [SIZE7], [SIZE8])
    Part.filter(Part["[Size]"] == SIZE1 | 
                Part["[Size]"] == SIZE2 |
                Part["[Size]"] == SIZE3 |
                Part["[Size]"] == SIZE4 |
                Part["[Size]"] == SIZE5 |
                Part["[Size]"] == SIZE6 |
                Part["[Size]"] == SIZE7 |
                Part["[Size]"] == SIZE8);

    // p_brand <> '[BRAND]'
    Part.filter(Part["[Brand]"] != BRAND);

    // p_type <> '[TYPE]'
    Part.filter(Part["[Type]"] != TYPE);    

    stopwatch::timepoint("Filters");

    // p_partkey = ps_partkey
    auto PartPartSuppJoin = Part.inner_join(PartSuppSupplierJoin, {"[PartKey]"}, {
        {"[Brand]", "[Brand]", copy<B>},
        {"[Type]", "[Type]", copy<B>},
        {"[Size]", "[Size]", copy<B>}
    });

    stopwatch::timepoint("Join");
    PartPartSuppJoin.deleteColumns({"[PartKey]"});

    // distinct ps_suppkey
    PartPartSuppJoin.distinct({"[Brand]", "[Type]", "[Size]", "[SuppKey]"});
    stopwatch::timepoint("Distinct");

    // Adding extra columns for count
    PartPartSuppJoin.addColumns({"SupplierCount"}, PartPartSuppJoin.size());
    PartPartSuppJoin.aggregate({ENC_TABLE_VALID, "[Brand]", "[Type]", "[Size]"}, {
        {"SupplierCount", "SupplierCount", count<A>}});

    stopwatch::timepoint("Group by count");
    
    // convert SupplierCount to boolean for sort
    PartPartSuppJoin.addColumns({"[SupplierCount]"}, PartPartSuppJoin.size());
    PartPartSuppJoin.convert_a2b("SupplierCount", "[SupplierCount]");
    PartPartSuppJoin.deleteColumns({"SupplierCount"});

    stopwatch::timepoint("A2B SupplierCount");

    // order by supplier_cnt desc, p_brand, p_type, p_size
    // We don't need to re-sort on brand, type, size
    PartPartSuppJoin.sort({
        std::make_pair("[SupplierCount]", DESC)
    });

    stopwatch::timepoint("Order by");

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    PartPartSuppJoin.finalize();
#endif

    stopwatch::done(); // print wall clock time
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();
    
#ifndef QUERY_PROFILE

    auto result = PartPartSuppJoin.open_with_schema();
    auto out_brand = PartPartSuppJoin.get_column(result, "[Brand]");
    auto out_type = PartPartSuppJoin.get_column(result, "[Type]");
    auto out_size = PartPartSuppJoin.get_column(result, "[Size]");
    auto out_suppcnt = PartPartSuppJoin.get_column(result, "[SupplierCount]");

    if (pid == 0) {
        const char * query = R"sql(
        select
            p.Brand,
            p.Type,
            p.Size,
            count(distinct ps.SuppKey) as supplier_cnt
        from
            PARTSUPP as ps,
            PART as p
        where
            p.PartKey = ps.PartKey
            and p.Brand <> ?
            and p.Type <> ?
            and p.Size in (?, ?, ?, ?, ?, ?, ?, ?)
            and ps.SuppKey not in (
                select
                    SuppKey
                from
                    SUPPLIER
                where
                    Comment = ?
            )
            group by
                p.Brand,
                p.Type,
                p.Size
            order by
                supplier_cnt desc,
                p.Brand,
                p.Type,
                p.Size;
        )sql";

        sqlite3_stmt* stmt;
        int ret = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        
        if (ret) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }

        // Fill in query placeholders
        sqlite3_bind_int(stmt, 1, BRAND);
        sqlite3_bind_int(stmt, 2, TYPE);
        sqlite3_bind_int(stmt, 3, SIZE1);
        sqlite3_bind_int(stmt, 4, SIZE2);
        sqlite3_bind_int(stmt, 5, SIZE3);
        sqlite3_bind_int(stmt, 6, SIZE4);
        sqlite3_bind_int(stmt, 7, SIZE5);
        sqlite3_bind_int(stmt, 8, SIZE6);
        sqlite3_bind_int(stmt, 9, SIZE7);
        sqlite3_bind_int(stmt, 10, SIZE8);
        sqlite3_bind_int(stmt, 11, COMMENT);

        auto res = sqlite3_step(stmt);
        if (res != SQLITE_ROW) {
            std::cerr << "Execution failed: " << sqlite3_errmsg(sqlite_db) << std::endl;
        }
        
        auto i = 0;
        while (res == SQLITE_ROW) {
            auto sql_brand = sqlite3_column_int(stmt, 0);
            auto sql_type = sqlite3_column_int(stmt, 1);
            auto sql_size = sqlite3_column_int(stmt, 2);
            auto sql_suppcnt = sqlite3_column_int(stmt, 3);

            /*std::cout << "brand: " << sql_brand << " | " << "type: " << sql_type 
                << " | " << "size: " << sql_size << " | " << "suppcnt: " << sql_suppcnt  
                << std::endl;*/

            ASSERT_SAME(sql_brand, out_brand[i]);
            ASSERT_SAME(sql_type, out_type[i]);
            ASSERT_SAME(sql_size, out_size[i]);
            ASSERT_SAME(sql_suppcnt, out_suppcnt[i]);

            res = sqlite3_step(stmt);
            ++i;
        }
        ASSERT_SAME(i, out_brand.size());
        if (i == 0) {single_cout("Empty result");}

        if (res != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }

        single_cout(i << " rows OK");
    }
#endif

    // Close SQLite DB
    sqlite3_close(sqlite_db);

    MPI_Finalize();

    return 0;
}
