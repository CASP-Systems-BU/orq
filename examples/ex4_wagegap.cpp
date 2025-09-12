/**
 * @file ex4_wagegap.cpp
 * @brief Example 4: Wagegap Demo
 *
 * Given secret-shared employment data, compute the wagegap for various demographic groups in
 * Academia vs Industry.
 *
 * We generate a random secret-shared table of employment data. The schema is:
 *
 * - Salary: this employee's annual salary in USD
 * - Degree: the highest degree they have earned. (Enum.)
 * - YearsExp: Their years of experience
 * - Region: The region of the world they work in. (Enum.)
 * - Academia: A bit representing whether they work an industry (0) or academia (0)
 *
 * Then, we run a series of aggregations, using each column as part of a compound key, and compute
 * the wage gap between academia and industry.
 *
 * For the _Years of Experience_, we create histogram bins by running comparison circuits and
 * shifting the resulting bits into specific locations to create distinct "labels" for each range.
 *
 * At the end of each subquery, we take the average using private division (since the counts in each
 * group will also be private). Then, we reset the table by calling `configureValid`, which allows
 * us to proceed to the next analysis. (Since oblivious computation cannot delete rows, all input
 * rows remain present in the table and unchanged. We just mark them as `invalid` during query
 * execution.)
 */

#include "orq.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::operators;
using namespace orq::aggregators;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

enum Degree {
    NoCollege,
    Undergraduate,
    Graduate,
    Doctorate,
    DegreeOther,
    _COUNT_DEGREES,
};

enum Region {
    NorthAmerica,
    SouthAmerica,
    Europe,
    Africa,
    Asia,
    MiddleEast,
    Oceania,
    Other,
    _COUNT_REGIONS,
};

// Number of rows in the test data
#define _ROWS (1 << 8)

int main(int argc, char **argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();

    using D = int32_t;
    using uD = std::make_unsigned_t<D>;

    std::vector<std::string> schema = {
        "Salary",    // in USD
        "[Degree]",  // enum, see above
        "[YearsExp]",
        "[Region]",    // enum, see above
        "[Academia]",  // bit, 0 = industry, 1 = academia
    };

    // FAKE DATA //
    Vector<uD> u_salary(_ROWS);
    runTime->populateLocalRandom(u_salary);
    Vector<D> salary(_ROWS);
    // Random salary: uniform in 0 - 500k
    salary = u_salary % 500000;

    Vector<uD> u_degree(_ROWS);
    runTime->populateLocalRandom(u_degree);
    Vector<D> degree(_ROWS);
    // Random degree; implicit enum (see above)
    degree = u_degree % (_COUNT_DEGREES - 1) + 1;

    // NOTE: proper handling of people in school here? Filter out zeros?
    Vector<uD> u_years(_ROWS);
    runTime->populateLocalRandom(u_years);
    Vector<D> years(_ROWS);
    // Random years of experience: uniform in 0 - 40
    years = u_years % 40;

    Vector<uD> u_region(_ROWS);
    runTime->populateLocalRandom(u_region);
    Vector<D> region(_ROWS);
    // Random region: implicit enum (see above)
    region = u_region % (int)_COUNT_REGIONS;

    Vector<D> academia(_ROWS);
    runTime->populateLocalRandom(academia);
    // Single bit, 0 = industry, 1 = academia
    academia.mask(1);

    std::vector<orq::Vector<D>> data = {salary, degree, years, region, academia};

    // Secret share the table
    EncodedTable<D> T = secret_share(data, schema);

    // and add columns for intermediates
    T.addColumns({"GroupSumSalary", "GroupCount", "[YearGroups]", "[AverageSalary]", "[OutTable]"});

    using A = ASharedVector<D>;
    using B = BSharedVector<D>;

    /*** Compute wage gap by degree ***/
    // Aggregate according to keys {academia, degree}, summing salary + counting people per group
    // Note that first arg of count<A> is ignored; we tend to just use the output column
    T.aggregate({"[Academia]", "[Degree]"}, {
                                                {"Salary", "GroupSumSalary", sum<A>},
                                                {"GroupCount", "GroupCount", count<A>},
                                            });

    // Sort according to valid: puts the valid rows up top
    T.sort({ENC_TABLE_VALID}, DESC);

    T["[OutTable]"] = T["[OutTable]"] ^ 1;
    auto out_AD = T.deepcopy();

    // Head takes the first n rows; since we sorted by valid, this will include all valid rows
    out_AD.head(2 * _COUNT_DEGREES);

    // Compute the average. Note RHS has AShared columns while the LHS has BShared ([] around column
    // names). operator/ operates over BShares but autoconverts if AShares are passed in.
    out_AD["[AverageSalary]"] = out_AD["GroupSumSalary"] / out_AD["GroupCount"];

    out_AD.deleteColumns(
        {"GroupSumSalary", "GroupCount", "Salary", "[Region]", "[YearGroups]", "[YearsExp]"});
    // finalize() zeros out the invalid rows and shuffles the table. This prevents ordering leakage
    // and makes sure invalid rows are actually "deleted".
    out_AD.finalize();

    single_cout("Analysis by DEGREE");
    print_table(out_AD.open_with_schema(), pID);

    T.configureValid();  // revalidate all rows (start a new analysis)

    /*** Compute wage gap by years of experience ***
     * Create bucket-bitmaps: effectively a simple histogram
     *
     *  0 (school)         00001
     *  1 - 4              00011
     *  5 - 10             00111
     * 11 - 20             01111
     * 21 +                11111
     *
     * Shifts and XORs are local.
     */
    T["[YearGroups]"] = ((T["[YearsExp]"] >= 0) ^          // zero / school       00001
                         ((T["[YearsExp]"] >= 1) << 1) ^   //  1 - 4 years        00011
                         ((T["[YearsExp]"] >= 5) << 2) ^   //  5 - 10             00111
                         ((T["[YearsExp]"] >= 11) << 3) ^  // 11 - 20             01111
                         ((T["[YearsExp]"] >= 21) << 4));  // 21                  11111

    // Compute a new aggregation: compound key is academia and year group
    T.aggregate({"[Academia]", "[YearGroups]"}, {
                                                    {"Salary", "GroupSumSalary", sum<A>},
                                                    {"GroupCount", "GroupCount", count<A>},
                                                });

    T.sort({ENC_TABLE_VALID}, DESC);
    T["[OutTable]"] = T["[OutTable]"] << 1;

    const int NUM_YEAR_GROUPS = 5;

    // pull out the academia-years result (5 bins)
    auto out_AY = T.deepcopy();
    out_AY.head(2 * NUM_YEAR_GROUPS);

    out_AY["[AverageSalary]"] = out_AY["GroupSumSalary"] / out_AY["GroupCount"];

    out_AY.deleteColumns(
        {"GroupSumSalary", "GroupCount", "Salary", "[Region]", "[YearsExp]", "[Degree]"});
    out_AY.finalize();

    single_cout("Analysis by YEARS EXPERIENCE");
    print_table(out_AY.open_with_schema(), pID);

    T.configureValid();  // revalidate all rows

    /*** Compute wage gap by region ***/
    T.aggregate({"[Academia]", "[Region]"}, {
                                                {"Salary", "GroupSumSalary", sum<A>},
                                                {"GroupCount", "GroupCount", count<A>},
                                            });

    T.sort({ENC_TABLE_VALID}, DESC);
    T["[OutTable]"] = T["[OutTable]"] << 1;

    auto out_AR = T.deepcopy();
    out_AR.head(2 * _COUNT_REGIONS);

    out_AR["[AverageSalary]"] = out_AR["GroupSumSalary"] / out_AR["GroupCount"];

    out_AR.deleteColumns(
        {"GroupSumSalary", "GroupCount", "Salary", "[Degree]", "[YearsExp]", "[YearGroups]"});

    out_AR.finalize();

    single_cout("Analysis by REGION") print_table(out_AR.open_with_schema(), pID);

    T.configureValid();

    /*** Compute global wage gap by field ***/
    T.aggregate({"[Academia]"}, {
                                    {"Salary", "GroupSumSalary", sum<A>},
                                    {"GroupCount", "GroupCount", count<A>},
                                });

    T.sort({ENC_TABLE_VALID}, DESC);
    T["[OutTable]"] = T["[OutTable]"] << 1;

    // binary choice. pull top two
    auto out_global = T.deepcopy();
    out_global.head(2);

    out_global["[AverageSalary]"] = out_global["GroupSumSalary"] / out_global["GroupCount"];

    out_global.deleteColumns({"GroupSumSalary", "GroupCount", "Salary", "[Degree]", "[YearsExp]",
                              "[YearGroups]", "[Region]"});

    single_cout("Global analysis by FIELD");
    print_table(out_global.open_with_schema(), pID);
}