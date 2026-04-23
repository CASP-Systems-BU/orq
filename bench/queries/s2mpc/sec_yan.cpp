/**
 * @file sec_yan.cpp
 * @brief Implements Custom Aggregation query from "Secure Yannakakis" (SIGMOD 2021)
 *      https://dl.acm.org/doi/10.1145/3448016.3452808
 * @date 2024-10-20
 *
 * This query computes the aggregate cost paid by insurance plans, grouped by disease class.
 *
 * Equivalent SQL:
 *   SELECT class_, SUM(cost * (1 - coinsurance))
 *   FROM T1, T2, T3
 *   WHERE T1.person=T2.person AND T2.disease=T3.disease
 *   GROUP BY class_
 *
 * Note: we added an underscore because `class` is a reserved word in SQL.
 *
 * The published version of this query uses randomly-generated data. For the tutorial, we will use
 * CSV files to input each data owner's contribution.
 *
 * Fill out each `#error TODO` to complete the query.
 *
 * See `sec_yan_solution.cpp` for a working implementation.
 */

// To run correctness tests
#include <sqlite3.h>

// Include the ORQ library
#include "orq.h"

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto partyID = runTime->getPartyID();

    using A = ASharedVector<int>;
    using B = BSharedVector<int>;

    // Load tables from respective CSVs

    ////////////////////////////////////
    //// Table T1: Owned by Party 0 ////
    const size_t t1_size = 64;

    // By default, tables are arithmetic shared (using addition to reconstruct)
    // Using [ ... ] in the schema specifies boolean shared data (using XOR to reconstruct)
    // Different operations are possible (or more efficient) in each sharing scheme.
    // Sometimes it is useful to secret-share data in both domains.
    //
    // Schema:
    // [person]:        a random patient ID.
    // coinsurance:     the patient's coinsurance, as a percentage * 100 (integer)
    // [state]:         which state they live in.
    //
    // We imagine this data is input by an insurance company.
    std::vector<std::string> t1_schema = {"[person]", "coinsurance", "[state]"};

    // Make an empty table.
    EncodedTable<int> T1("T1", t1_schema, t1_size);

    // Load the CSV.
    // Relative path assumes this is being executed from the `build/` folder.
    // The second argument specifies that party 0 is the data owner, and has access to the plaintext
    // data. Party 0 will secret share its data and distribute the shares to the other parties.
    T1.inputCSVTableData("../examples/data/data-owner-0/p0-t1.csv", 0);

    ////////////////////////////////////
    //// Table T2: Owned by Party 1 ////
    const size_t t2_size = 32;

    // Schema:
    // [person]:    a random patient ID, as in T1.
    // [disease]:   a disease ID
    // cost:        the cost of the treatment for this patient and the given disease
    //
    // We imagine this data is input by one or more hospitals (patients may seek treatment at
    // multiple locations)
    std::vector<std::string> t2_schema = {"[person]", "[disease]", "cost"};

    EncodedTable<int> T2("T2", t2_schema, t2_size);
    T2.inputCSVTableData("../examples/data/data-owner-1/p1-t2.csv", 1);

    ////////////////////////////////////
    //// Table T3: Owned by Party 2 ////
    const size_t t3_size = 32;

    // Schema:
    // [disease]:   a disease ID, as above
    // [class]:     the disease class this disease belongs to
    //
    // Admittedly, this should probably be a public database. But we could imagine this contains
    // insurance company proprietary data, for example.
    std::vector<std::string> t3_schema = {"[disease]", "[class]"};

    EncodedTable<int> T3("T3", t3_schema, t3_size);
    T3.inputCSVTableData("../examples/data/data-owner-2/p2-t3.csv",
#error TODO
    );

    //////////////////////////////////////////////////////////////////
    // At this point, all parties hold secret shares of all tables. //
    //////////////////////////////////////////////////////////////////

    // ORQ supports many standard relational operators.
    // For this query, we don't need all of the columns in the table, and project out the two
    // columns we need.
    // For many operators (such as shuffle and shorting), extra columns incur extra overhead, so we
    // try to keep the working set of data as small as possible.
    T1.project({"[person]", "coinsurance"});

    // Start the stopwatch.
    stopwatch::timepoint("Start");

    // Add a new column 'percentage' to T1
    T1.addColumns({"insurance_percent"});

    // We want to compute `1 - coinsurance`, as specified in the query.
    // However, the current public version of ORQ does not support decimals (we will push support
    // soon). Therefore, we use scaled-up percentages, and subtract from 100. (In the current
    // version, we can't write `100 - ...` because of a C++ quirk, but this will also be fixed
    // soon.)
    T1["insurance_percent"] = 
#error TODO
    ;

    // We no longer need the coinsurance column.
    T1.deleteColumns({"coinsurance"});

    // Sum percentages per person.
    // Since the aggregation function is decomposable, and insurance_percent is specific to each
    // person,
    //   SUM(cost * insurance)
    // is equivalent to
    //   SUM(cost * SUM(insurance))
    // under the appropriate grouping keys.

    // By default, the `aggregate` function will mask out intermediate rows, leaving one valid
    // record per person.
    T1.aggregate(
        // The group-by key.
#error TODO
        // The aggregations.
        {
            // Syntax is {"input column", "output column", aggregationFunction<Type>}
            // Overwriting is supported, but we could also specify a separate aggregate column if we
            // wanted to keep the original data around.
#error TODO
            // We could specify more aggregations here if needed.
        });

    stopwatch::timepoint("PreAgg T1");

    // Next, we run the join.
    // Join T1 (PK) with T2 on 'person' and copy `percentage` per matched person into T2
    // Syntax is LEFT_TABLE.inner_join(RIGHT_TABLE, ...)
    // ORQ's inner join algorithm requires unique keys on the left (but not necessarily a PK-FK
    // relation).
    auto T12 = T1.inner_join(
        T2,
        // The group-by key.
#error TODO
        // The aggregations.
        {
            // copy<> just moves data from the left table to the output. By default, only data from
            // the right table is included, due to some technical quirks of the algorithm.
            // Same syntax as aggregations
#error TODO
        });

    stopwatch::timepoint("T1 >< T2");

    T12.addColumn("insurance_cost");

    // Since insurance_percent is multiplied by 100, we have to divide.
    // We could also defer division for the final step to avoid rounding errors.
    // This uses the secure division algorithm from our prior paper, TVA (USENIX Security 2023)
#error TODO

    T12.project({"[disease]", "insurance_cost"});

    // Sum insurance_cost per disease
    // After the aggregation is applied, T12 contains one valid record per 'disease'
    T12.aggregate(
        // Group by key.
#error TODO
        // Aggregations.
        {
            // Sum all insurance costs.
#error TODO
        });

    stopwatch::timepoint("PreAgg T3");

    // Join T12 (PK) with T3 on 'disease' and copy `insCost` per matched 'disease' into T3
    auto T123 = T12.inner_join(
        // Right-side table
        T3,
        // Group key
#error TODO
        // Aggregations
        {
#error TODO
        });

    stopwatch::timepoint("T2 >< T3");

    // Throw away all other columns
    T123.project({"[class]", "insurance_cost"});

    // Sum 'insurance_cost' per 'class'
    T123.aggregate(
        // Final group by key
#error TODO
        // Final sum aggregation
        {
#error TODO
        });

    // Shuffle the table & mask all invalid rows to prevent leakage.
    T123.finalize();

    stopwatch::timepoint("Final Agg");
    stopwatch::done();

    print_table(T123.open_with_schema(), partyID);

    // Synchronization point.
    MPI_Barrier(MPI_COMM_WORLD);

    // When run under the single-party debug protocol, output the number of operations performed
    runTime->print_statistics();

    // Show the network utilization of this execution
    runTime->print_communicator_statistics();

    return 0;
}