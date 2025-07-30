#include "../../include/secrecy.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace secrecy::operators;
using namespace secrecy::aggregators;

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

// #define CORRECTNESS_CHECK
// #define DEMO_DEBUG_PRINT_OUTPUT

#ifdef CORRECTNESS_CHECK
#include <sqlite3.h>
#include <sstream>

template <typename T>
void correctness_check(
    EncodedTable<T> data,
    EncodedTable<T> res_degree,
    EncodedTable<T> res_year,
    EncodedTable<T> res_region,
    EncodedTable<T> res_field)
{

    single_cout("==== Correctness Check ====");

    single_cout_nonl("  opening secret-shared tables... ");

    // open all of the tables
    // Plaintext input data
    auto pt_data = data.open_with_schema();

    auto data_salary = data.get_column(pt_data, "Salary");
    auto data_years  = data.get_column(pt_data, "YearsExp");
    auto data_field  = data.get_column(pt_data, "[Academia]");
    auto data_degree = data.get_column(pt_data, "[Degree]");
    auto data_region = data.get_column(pt_data, "[Region]");
    
    // Plaintext analysis results
    auto pt_degree = res_degree.open_with_schema();
    auto pt_year   = res_year.open_with_schema();

    auto pt_region = res_region.open_with_schema();
    auto pt_field  = res_field.open_with_schema();

    single_cout(" OK");

    // only one party needs to do correctness check after this point
    if (runTime->getPartyID() != 0) { return; }

    std::cout << "  creating sql database... ";

    // create sqlite database
    sqlite3 * db;
    if (sqlite3_open(NULL, &db)) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }

    const char * create_stmt = R"sql(
        CREATE TABLE WAGEDATA (
            Salary   INTEGER,
            YearsExp INTEGER,
            Academia INTEGER,
            Degree   INTEGER,
            Region   INTEGER
        );
    )sql";
    sqlite3_exec(db, create_stmt, 0, 0, NULL);

    std::cout << "OK\n";

    std::cout << "  loading data... ";

    sqlite3_stmt * stmt = nullptr;
    const char * insert_stmt = R"sql(
        INSERT INTO WAGEDATA (
            Salary, YearsExp, Academia, Degree, Region)
        VALUES (?, ?, ?, ?, ?);
    )sql";
    sqlite3_prepare_v2(db, insert_stmt, -1, &stmt, nullptr);

    for (size_t i = 0; i < _ROWS; i++) {
        sqlite3_bind_int(stmt, 1, data_salary[i]);
        sqlite3_bind_int(stmt, 2, data_years[i]);
        sqlite3_bind_int(stmt, 3, data_field[i]);
        sqlite3_bind_int(stmt, 4, data_degree[i]);
        sqlite3_bind_int(stmt, 5, data_region[i]);

        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }

    std::cout << "OK\n";

    {
        std::cout << "Degree Analysis... ";
        const char * degree_query = R"sql(
            SELECT
                AVG(Salary) as AvgSalary,
                AVG(YearsExp) as AvgYears
            FROM
                WAGEDATA
            GROUP BY
                Degree;
        )sql";

        auto ret = sqlite3_prepare_v2(db, degree_query, -1, &stmt, NULL);
        if (ret) {
            throw std::runtime_error(sqlite3_errmsg(db));
        }

        std::vector<int> true_degree_salary;
        std::vector<int> true_degree_years;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            true_degree_salary.push_back(sqlite3_column_double(stmt, 0));
            true_degree_years.push_back(sqlite3_column_double(stmt, 1));
        }

        Vector<int> degree_salary = res_degree.get_column(pt_degree, "[AvgSalary]");
        Vector<int> degree_years = res_degree.get_column(pt_degree, "[AvgYears]");

        assert(degree_salary.same_as(true_degree_salary));
        assert(degree_years.same_as(true_degree_years));
        std::cout << "OK\n";
    }

    {
        std::cout << "Field Analysis... ";
        const char * field_query = R"sql(
            SELECT
                AVG(Salary) as AvgSalary,
                AVG(YearsExp) as AvgYears
            FROM
                WAGEDATA
            GROUP BY
                Academia;
        )sql";

        auto ret = sqlite3_prepare_v2(db, field_query, -1, &stmt, NULL);
        if (ret) {
            throw std::runtime_error(sqlite3_errmsg(db));
        }

        std::vector<int> true_field_salary;
        std::vector<int> true_field_years;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            true_field_salary.push_back(sqlite3_column_double(stmt, 0));
            true_field_years.push_back(sqlite3_column_double(stmt, 1));
        }

        Vector<int> field_salary = res_field.get_column(pt_field, "[AvgSalary]");
        Vector<int> field_years = res_field.get_column(pt_field, "[AvgYears]");

        assert(field_salary.same_as(true_field_salary));
        assert(field_years.same_as(true_field_years));
        std::cout << "OK\n";
    }

    {
        std::cout << "Region Analysis... ";
        const char * region_query = R"sql(
            SELECT
                AVG(Salary) as AvgSalary,
                AVG(YearsExp) as AvgYears
            FROM
                WAGEDATA
            GROUP BY
                Region;
        )sql";
        
        auto ret = sqlite3_prepare_v2(db, region_query, -1, &stmt, NULL);
        if (ret) {
            throw std::runtime_error(sqlite3_errmsg(db));
        }

        std::vector<int> true_region_salary;
        std::vector<int> true_region_years;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            true_region_salary.push_back(sqlite3_column_double(stmt, 0));
            true_region_years.push_back(sqlite3_column_double(stmt, 1));
        }

        Vector<int> region_salary = res_region.get_column(pt_region, "[AvgSalary]");
        Vector<int> region_years = res_region.get_column(pt_region, "[AvgYears]");

        // Out of order.
        assert(region_salary.same_as(true_region_salary));
        assert(region_years.same_as(true_region_years));
        std::cout << "OK\n";
    }

    {
        std::cout << "Years of Experience Analysis... ";
        const char * year_query = R"sql(
            SELECT
                AVG(Salary) as AvgSalary,
                CASE
                    WHEN YearsExp ==  0                   THEN  1
                    WHEN YearsExp >=  1 AND YearsExp <  3 THEN  2
                    WHEN YearsExp >=  3 AND YearsExp <  6 THEN  6
                    WHEN YearsExp >=  6 AND YearsExp < 11 THEN 14
                    WHEN YearsExp >= 11 AND YearsExp < 21 THEN 30
                    WHEN YearsExp >= 21                   THEN 62
                END as YearGroups
            FROM
                WAGEDATA
            GROUP BY
                YearGroups;
        )sql";
        
        auto ret = sqlite3_prepare_v2(db, year_query, -1, &stmt, NULL);
        if (ret) {
            throw std::runtime_error(sqlite3_errmsg(db));
        }

        std::vector<int> true_year_salary;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            true_year_salary.push_back(sqlite3_column_double(stmt, 0));
        }

        Vector<int> year_salary = res_year.get_column(pt_year, "[AvgSalary]");

        // Out of order.
        assert(year_salary.same_as(true_year_salary));
        std::cout << "OK\n";
    }
}
#endif

int main(int argc, char **argv)
{
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();

    using D = int32_t;
    using uD = std::make_unsigned_t<D>;

    std::vector<std::string> columns = {
        "Salary",           // in USD
        "[Salary]",             // --- not used
        "Degree",               // --- not used
        "[Degree]",         // enum, see above
        "YearsExp",             // --- not used
        "[YearsExp]",
        "Region",               // --- not used
        "[Region]",         // enum, see above
        "Academia",             // --- not used
        "[Academia]",       // bit, 0 = industry, 1 = academia
    };

    // FAKE DATA //
    Vector<uD> u_salary(_ROWS);
    runTime->populateLocalRandom(u_salary);
    Vector<D> salary(_ROWS);
    salary = u_salary % 500000;

    Vector<uD> u_degree(_ROWS);
    runTime->populateLocalRandom(u_degree);
    Vector<D> degree(_ROWS);
    degree = u_degree % (_COUNT_DEGREES - 1) + 1;

    // NOTE: proper handling of people in school here? Filter out zeros?
    Vector<uD> u_years(_ROWS);
    runTime->populateLocalRandom(u_years);
    Vector<D> years(_ROWS);
    years = u_years % 40;

    Vector<uD> u_region(_ROWS);
    runTime->populateLocalRandom(u_region);
    Vector <D> region(_ROWS);
    region = u_region % (int) _COUNT_REGIONS;

    Vector<D> academia(_ROWS);
    runTime->populateLocalRandom(academia);
    academia.mask(1);

    std::vector<secrecy::Vector<D>> data = {
        // ashared // bshared
        salary,    salary,
        degree,    degree,
        years,     years,
        region,    region,
        academia,  academia
    };
    
#ifdef DEMO_DEBUG_PRINT_OUTPUT
    EncodedTable<D> T = secret_share(data, columns);
#else
    EncodedTable<D> T("WageData", columns, _ROWS);
    T.inputCSVTableSecretShares("../examples/wagegap/ex-wagegap-share-" + std::to_string(pID) + ".csv");
#endif

    // Types we don't need.
    T.deleteColumns({
        "[Salary]", "Degree", "Region", "YearsExp", "Academia",
    });

    T.addColumns({
        "GroupSumSalary", "GroupCount", "[YearGroups]", "[OutTable]"
    }, T.size());

    // TODO: figure out how to subsume this into library
    using A = ASharedVector<D>;
    using B = BSharedVector<D>;

    /*** Compute wage gap by degree ***/
    T.aggregate({"[Academia]", "[Degree]"}, {
        {"Salary",   "GroupSumSalary", sum<A>},
        {"Salary",   "GroupCount",     count<A>},
    });

    T.sort({ENC_TABLE_VALID}, DESC);

    T["[OutTable]"] = T["[OutTable]"] ^ 1;
    auto out_AD = T.deepcopy();
    out_AD.head(2 * _COUNT_DEGREES);

    out_AD.deleteColumns({"Salary", "YearsExp", "[Region]", "[YearGroups]",
                          "[YearsExp]"});
    out_AD.finalize();

    // At this point, we would normally open `out_AD`. But we don"t actually want to:
    // instead, output shares to a file using Muhammad's code (TBD) and get them
    // back to bucket.

#ifdef DEMO_DEBUG_PRINT_OUTPUT
    // TESTING ONLY:
    single_cout("Analysis by DEGREE");
    print_table(out_AD.open_with_schema(), pID);
#endif

    T.configureValid(); // revalidate all rows

    /*** Compute wage gap by years of experience ***
     * create bucket-bitmaps.
     * 
     *  0 (school)         00001
     *  1 - 4              00011
     *  5 - 10             00111
     * 11 - 20             01111
     * 21 +                11111
     * 
     * Shifts and XORs are local.
     */
    T["[YearGroups]"] = (
        ( T["[YearsExp]"] >=  0)       ^ // zero / school       00001
        ((T["[YearsExp]"] >=  1) << 1) ^ //  1 - 4 years        00011
        ((T["[YearsExp]"] >=  5) << 2) ^ //  5 - 10             00111
        ((T["[YearsExp]"] >= 11) << 3) ^ // 11 - 20             01111
        ((T["[YearsExp]"] >= 21) << 4)); // 21                  11111

    T.aggregate({"[Academia]", "[YearGroups]"}, {
        {"Salary", "GroupSumSalary", sum<A>},
        {"Salary", "GroupCount", count<A>},
    });

    T.sort({ENC_TABLE_VALID}, DESC);
    T["[OutTable]"] = T["[OutTable]"] << 1;

    const int NUM_YEAR_GROUPS = 5;

    // pull out the academia-years result (5 bins)
    auto out_AY = T.deepcopy();
    out_AY.head(2 * NUM_YEAR_GROUPS);

    out_AY.deleteColumns({"Salary", "YearsExp", "[Region]", "[YearsExp]",
                          "[Degree]"});
    out_AY.finalize();

#ifdef DEMO_DEBUG_PRINT_OUTPUT
    single_cout("Analysis by YEARS EXPERIENCE");
    print_table(out_AY.open_with_schema(), pID);
#endif

    T.configureValid(); // revalidate all rows

    /*** Compute wage gap by region ***/
    T.aggregate({"[Academia]", "[Region]"}, {
        {"Salary",   "GroupSumSalary", sum<A>},
        {"Salary",   "GroupCount",     count<A>},
    });

    T.sort({ENC_TABLE_VALID}, DESC);
    T["[OutTable]"] = T["[OutTable]"] << 1;

    auto out_AR = T.deepcopy();
    out_AR.head(2 * _COUNT_REGIONS);

    out_AR.deleteColumns({"Salary", "YearsExp", "[Degree]", "[YearsExp]",
                          "[YearGroups]"});

    out_AR.finalize();

#ifdef DEMO_DEBUG_PRINT_OUTPUT
    single_cout("Analysis by REGION")
    print_table(out_AR.open_with_schema(), pID);
#endif

    T.configureValid();

    /*** Compute global wage gap by field ***/
    T.aggregate({"[Academia]"}, {
        {"Salary",   "GroupSumSalary", sum<A>},
        {"Salary",   "GroupCount",     count<A>},
    });

    T.sort({ENC_TABLE_VALID}, DESC);
    T["[OutTable]"] = T["[OutTable]"] << 1;

    // binary choice. pull top two
    auto out_global = T.deepcopy();
    out_global.head(2);

    out_global.deleteColumns({"Salary", "YearsExp", "[Degree]", "[YearsExp]",
                          "[YearGroups]", "[Region]"});

#ifdef DEMO_DEBUG_PRINT_OUTPUT
    single_cout("Global analysis by FIELD")
    print_table(out_global.open_with_schema(), pID);
#endif

// #ifdef CORRECTNESS_CHECK
//     T.configureValid();
//     correctness_check(
//         T,  // input table
//         SD, // results by degree
//         SY, // results by years experience
//         SR, // results by region
//         SA  // results by field
//     );
// #endif

    auto comb = out_AD.concatenate(out_AY).concatenate(out_AR).concatenate(out_global, true);
    comb.deleteColumns({ENC_TABLE_JOIN_ID});

    // Compute averages
    comb.addColumn({"[AvgSalary]"}, comb.size());
    comb["[AvgSalary]"] = comb["GroupSumSalary"] / comb["GroupCount"];
    // TODO: fix
    comb["[AvgSalary]"].encoding = secrecy::Encoding::BShared;

    comb.deleteColumns({"GroupSumSalary", "GroupCount"});

    const int n_rows = 1 + _COUNT_DEGREES + _COUNT_REGIONS + _COUNT_DEGREES;

    // Add back in any rows that we may have missed above.
    std::vector<std::string> output_columns = {
        "[OutTable]", "[Academia]", "[AvgSalary]", "[Degree]",
        "[YearGroups]", "[Region]",
    };

    std::vector<Vector<D>> zero_data = {
        // 5 degrees     5 year groups   8 regions                global
        {1, 1, 1, 1, 1,  2, 2, 2, 2, 2,  4, 4, 4, 4, 4, 4, 4, 4,  8},
        Vector<D>(n_rows), // Academia
        Vector<D>(n_rows, INT_MAX), // AvgSalary
        // Degree
        {0, 1, 2, 3, 4,  0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,  0},
        // Year Groups
        {0, 0, 0, 0, 0,  1, 3, 7, 15, 31, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        // Region
        {0, 0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 1, 2, 3, 4, 5, 6, 7,  0}
    };

    EncodedTable<D> zero_industry = secret_share(zero_data, output_columns);

    // change from industry to academia
    zero_data[1] = zero_data[1] + 1;
    EncodedTable<D> zero_academia = secret_share(zero_data, output_columns);

    auto zero_table = zero_industry.concatenate(zero_academia);
    zero_table.deleteColumns({ENC_TABLE_JOIN_ID});

    // Append the empty table
    auto full_out = comb.concatenate(zero_table, true);
    full_out.deleteColumns({ENC_TABLE_JOIN_ID});

    full_out.sort({
        ENC_TABLE_VALID, "[Academia]", "[OutTable]", "[Degree]", "[Region]", "[YearGroups]",
    }, ASC);

    full_out.aggregate(
        {ENC_TABLE_VALID, "[Academia]", "[OutTable]", "[Degree]", "[Region]", "[YearGroups]"}, {
            {"[AvgSalary]", "[AvgSalary]", min<B>}
        },
        {.do_sort = false}
    );
    
    // We need to get the invalid rows back up top. This could be done non-
    // obliviously (we know which rows we added) but there's no easy way to do
    // this 
    full_out.sort({
        ENC_TABLE_VALID, "[Academia]", "[OutTable]", "[Degree]", "[Region]", "[YearGroups]",
    }, ASC);

    // Invalid (padding) rows are at top.
    full_out.tail(2 * n_rows);
    auto industry_avg = full_out.deepcopy();
    industry_avg.head(n_rows);

    auto academia_avg = full_out;
    academia_avg.tail(n_rows);

    auto wage_gap = industry_avg;
    wage_gap["[AvgSalary]"] = industry_avg["[AvgSalary]"] - academia_avg["[AvgSalary]"];

    wage_gap.deleteColumns({"[Academia]"});

#ifdef DEMO_DEBUG_PRINT_OUTPUT
    single_cout("Wage Gap Analyses")
    print_table(wage_gap.open_with_schema(), pID);
#else
    // TODO: create dir if doesn"t exist
    wage_gap.outputCSVTableSecretShares("../results/wagegap/ex-wagegap-out-share-" + std::to_string(pID) + ".csv");
#endif

    runTime->print_statistics();


#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}