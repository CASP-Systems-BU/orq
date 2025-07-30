/**
 * @file q22.cpp
 * @brief Implements TPCH Query 22
 * @date 2024-10-18
 * 
 * Equivalent SQL:
 *  select
 *  	cntrycode,
 *  	count(*) as numcust,
 *  	sum(c_acctbal) as totacctbal
 *  from (
 *  	select
 *  		substring(c_phone from 1 for 2) as cntrycode,
 *  		c_acctbal
 *  	from
 *  		customer
 *  	where
 *  		substring(c_phone from 1 for 2) in
 *  		('[I1]','[I2]','[I3]','[I4]','[I5]','[I6]','[I7]')
 *  		and c_acctbal > (
 *  			select
 *  				avg(c_acctbal)
 *  			from
 *  				customer
 *  			where
 *  				c_acctbal > 0.00
 *  				and substring (c_phone from 1 for 2) in
 *  					('[I1]','[I2]','[I3]','[I4]','[I5]','[I6]','[I7]')
 *  		)
 *  		and not exists (
 *  			select
 *  				*
 *  			from
 *  				orders
 *  			where
 *  				o_custkey = c_custkey
 *  		)
 *  	) as custsale
 *  group by
 *  	cntrycode
 *  order by
 *  	cntrycode;
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

    // Setup SQLite DB for output validation
    sqlite3 *sqlite_db = nullptr;
#ifndef QUERY_PROFILE
    if (pid == 0) {
        int err = sqlite3_open(NULL, &sqlite_db);
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

    single_cout("Q22 SF " << db.scaleFactor);

    auto Customer = db.getCustomersTable();
    auto Order = db.getOrdersTable();

    Customer.project({"[CustKey]", "[CntryCode]", "[AcctBal]", "AcctBal"});
    Order.project({"[CustKey]"});

    // Country codes 
    const int64_t I1 = 13, I2 = 31, I3 = 23,
        I4 = 29, I5 = 30, I6 = 18, I7 = 17;

#ifdef PRINT_TABLES
    single_cout("CUSTOMER, size " << db.customersSize());
    print_table(Customer.open_with_schema(), pid);

    single_cout("ORDER, size " << db.ordersSize());
    print_table(Order.open_with_schema(), pid);
#endif

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    // Select the customers with the given country codes
    // CntryCode in ('[I1]','[I2]','[I3]','[I4]','[I5]','[I6]','[I7]')
    Customer.filter(Customer["[CntryCode]"] == I1 |
                    Customer["[CntryCode]"] == I2 |
                    Customer["[CntryCode]"] == I3 |
                    Customer["[CntryCode]"] == I4 |
                    Customer["[CntryCode]"] == I5 |
                    Customer["[CntryCode]"] == I6 |
                    Customer["[CntryCode]"] == I7);

    // c_acctbal > 0.00
    Customer.filter(Customer["[AcctBal]"] > 0);

    // Generate the aggregation avg(c_acctbal)
    Customer.addColumns({"TotalAcctBal", "CountAcctBal"}, Customer.size());
    Customer.aggregate({ENC_TABLE_VALID}, {
        {"AcctBal", "TotalAcctBal", sum<A>},
        {"AcctBal", "CountAcctBal", count<A>}
        }, {
        .mark_valid = false
    });
    Customer.aggregate({ENC_TABLE_VALID}, {
        {"TotalAcctBal", "TotalAcctBal", copy<A>},
        {"CountAcctBal", "CountAcctBal", copy<A>}
    }, {
        .reverse = true,
        .do_sort = false,
        .mark_valid = false
    });


    // TODO: the next 3 steps should be wrapped under some functionality.
    // 1- Extract and convert the aggregation results
    A extracted(2);
    extracted.simple_subset_reference(0, 1, 0) = ((A*)Customer["TotalAcctBal"].contents.get())->simple_subset_reference(Customer.size() - 1, 1, Customer.size() - 1);
    extracted.simple_subset_reference(1, 1, 1) = ((A*)Customer["CountAcctBal"].contents.get())->simple_subset_reference(Customer.size() - 1, 1, Customer.size() - 1);

    // 2- Conversion and Division
    B extracted_b = extracted.a2b();
    B avg_b = extracted_b.simple_subset_reference(0, 1, 0) / extracted_b.simple_subset_reference(1, 1, 1);

    // 3- Putting result back to table
    Customer.addColumns({"[AvgAcctBal]"}, Customer.size());
    *((B*)Customer["[AvgAcctBal]"].contents.get()) = avg_b.cyclic_subset_reference(Customer.size());

    // Filter c_acctbal > avg(c_acctbal)
    Customer.filter(Customer["[AcctBal]"] > Customer["[AvgAcctBal]"]);

    // // Alternative way to filter c_acctbal > avg(c_acctbal)
    // // Evaulating condition c_acctbal > avg(c_acctbal)
    // // c_acctbal > avg(c_acctbal) = TotalAcctBal / CountAcctBal
    // // c_acctbal > TotalAcctBal / CountAcctBal
    // // c_acctbal * CountAcctBal > TotalAcctBal
    // // c_acctbal * CountAcctBal - TotalAcctBal > 0
    // Customer.addColumns({"AcctBalDiff", "[AcctBalDiff]"}, Customer.size());
    // Customer["AcctBalDiff"] = Customer["AcctBal"] * Customer["CountAcctBal"] - Customer["TotalAcctBal"];
    // Customer.convert_a2b("AcctBalDiff", "[AcctBalDiff]");
    // Customer.filter(Customer["[AcctBalDiff]"] > 0);

    // Remove intermeidate columns
    Customer.project({"[CustKey]", "[CntryCode]", "AcctBal"});

    // Evaluate the anti join with orders on custkey
    // not exists ( select * from orders where o_custkey = c_custkey )
    auto CustomerAJ = Customer.anti_join(Order, {"[CustKey]"});

    // Evaluate the {count(*) as numcust, sum(c_acctbal) as totacctbal} aggregation
    CustomerAJ.addColumns({"NumCust", "TotAcctBal"}, CustomerAJ.size());
    CustomerAJ.aggregate({ENC_TABLE_VALID, "[CntryCode]"}, {
        {"AcctBal", "NumCust", count<A>},
        {"AcctBal", "TotAcctBal", sum<A>}
    });

    // Final projection
    CustomerAJ.project({"[CntryCode]", "NumCust", "TotAcctBal"});

#ifdef QUERY_PROFILE
    // Include the final mask and shuffle in benchmarking time
    CustomerAJ.finalize();
#endif

    stopwatch::done(); // print wall clock time
    stopwatch::profile_done(); // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

#ifndef QUERY_PROFILE

    auto CustomerOpen = CustomerAJ.open_with_schema();
    print_table(CustomerOpen, pid);

    auto out_cntry_code = Customer.get_column(CustomerOpen, "[CntryCode]");
    auto out_num_cust = Customer.get_column(CustomerOpen, "NumCust");
    auto out_tot_acct_bal = Customer.get_column(CustomerOpen, "TotAcctBal");

    // Note: We consider the function `substring(c_phone from 1 for 2)` as preprocessing
    // It is evaluated and saved in the column "CntryCode".
    if (pid == 0) {
        const char * query = R"sql(
            select
                cntrycode,
                count(*) as numcust,
                sum(acctbal) as totacctbal
            from (
                select
                    cntrycode,
                    acctbal
                from
                    customer
                where
                    cntrycode in
                    (?,?,?,?,?,?,?)
                    and acctbal > (
                        select
                            avg(acctbal)
                        from
                            customer
                        where
                            acctbal > 0.00
                            and cntrycode in
                                (?,?,?,?,?,?,?)
                    )
                    and not exists (
                        select
                            *
                        from
                            orders
                        where
                            orders.custkey = customer.custkey
                    )
                ) as custsale
            group by
                cntrycode
            order by
                cntrycode;
        )sql";

        sqlite3_stmt * stmt;
        auto err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, NULL);
        if (err) {
            throw std::runtime_error(sqlite3_errmsg(sqlite_db));
        }

        sqlite3_bind_int(stmt, 1, I1);
        sqlite3_bind_int(stmt, 2, I2);
        sqlite3_bind_int(stmt, 3, I3);
        sqlite3_bind_int(stmt, 4, I4);
        sqlite3_bind_int(stmt, 5, I5);
        sqlite3_bind_int(stmt, 6, I6);
        sqlite3_bind_int(stmt, 7, I7);
        sqlite3_bind_int(stmt, 8, I1);
        sqlite3_bind_int(stmt, 9, I2);
        sqlite3_bind_int(stmt, 10, I3);
        sqlite3_bind_int(stmt, 11, I4);
        sqlite3_bind_int(stmt, 12, I5);
        sqlite3_bind_int(stmt, 13, I6);
        sqlite3_bind_int(stmt, 14, I7);
        
        int i;
        for (i = 0; sqlite3_step(stmt) == SQLITE_ROW; i++) {
            auto cntry_code = sqlite3_column_int(stmt, 0);
            auto num_cust = sqlite3_column_int(stmt, 1);
            auto tot_acct_bal = sqlite3_column_int(stmt, 2);

            ASSERT_SAME(cntry_code, out_cntry_code[i]);
            ASSERT_SAME(num_cust, out_num_cust[i]);
            ASSERT_SAME(tot_acct_bal, out_tot_acct_bal[i]);
        }
        ASSERT_SAME(i, out_cntry_code.size());
        if (i == 0) {single_cout("Empty result");}
        single_cout(i << " rows OK");
    }
#endif
    sqlite3_close(sqlite_db);

    MPI_Finalize();

    return 0;
}