#include "orq.h"

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#include <filesystem>

int main(int argc, char **argv) {
    orq_init(argc, argv);
    auto pID = orq::service::runTime->getPartyID();

    // For now, this test only works for 3PC.
#ifndef MPC_PROTOCOL_REPLICATED_THREE
    return 0;
#endif

    std::filesystem::create_directories("../results/employees/");
    single_cout("Created output directories");

    ////////////////////////////////////////////
    //          WORKING WITH TABLES           //
    ////////////////////////////////////////////
    // Table data
    Vector<int32_t> employeeAge_25 = {25, 30, 35,  40, 45, 50, 55, 60, 65, 70, 75, 80, 85,
                                      90, 95, 100, 25, 30, 35, 40, 45, 50, 55, 60, 25};
    Vector<int32_t> employeeID = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
                                  14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};
    Vector<int32_t> employeeRegion = {45, 70, 45, 70, 45, 70, 45, 70, 45, 70, 45, 70, 45,
                                      70, 45, 70, 45, 70, 45, 70, 45, 70, 45, 70, 45};

    // Creating table .. always put "SEL" in the schema
    const std::string tableName = "Employee_Profile";
    std::vector<std::string> schema = {"[EMPLOYEE_ID]", "[REGION]", "[AGE]"};
    const int employeesSize = 32;
    EncodedTable<int32_t> employees_1(tableName, schema, employeesSize);
    EncodedTable<int32_t> employees_2(tableName, schema, employeesSize);
    EncodedTable<int32_t> employees_3(tableName, schema, employeesSize);
    EncodedTable<int32_t> employees_4(tableName, schema, employeesSize);

    {
        // Reading input from file with party 0 using plain text.
        employees_1.inputCSVTableData("../tests/data/employees.csv", 0);
        auto opened_1 = employees_1.open_with_schema();
        assert(employeeAge_25.same_as(opened_1.first[0]));
        assert(employeeID.same_as(opened_1.first[1]));
        assert(employeeRegion.same_as(opened_1.first[2]));
        single_cout("Table: reading data ...OK");
    }

    {
        // Reading input from file using secret shares.
        employees_2.inputCSVTableSecretShares("../tests/data/employees_secrets_3pc_" +
                                              std::to_string(runTime->getPartyID()) + ".csv");
        auto opened_2 = employees_2.open_with_schema();
        assert(employeeAge_25.same_as(opened_2.first[0]));
        assert(employeeID.same_as(opened_2.first[1]));
        assert(employeeRegion.same_as(opened_2.first[2]));
        single_cout("Table: reading secret shares ...OK");
    }

    {
        // Writing current table secret shares to a file.
        employees_1.outputCSVTableSecretShares("../results/employees/employees_secrets_3pc_" +
                                               std::to_string(runTime->getPartyID()) + ".csv");
        employees_3.inputCSVTableSecretShares("../tests/data/employees_secrets_3pc_" +
                                              std::to_string(runTime->getPartyID()) + ".csv");
        auto opened_3 = employees_3.open_with_schema();
        assert(employeeAge_25.same_as(opened_3.first[0]));
        assert(employeeID.same_as(opened_3.first[1]));
        assert(employeeRegion.same_as(opened_3.first[2]));
        single_cout("Table: writing secret shares ...OK");
    }

    Vector<int32_t> employeeAge_32 = {25, 30, 35, 40, 45,  50, 55, 60, 65, 70, 75,
                                      80, 85, 90, 95, 100, 25, 30, 35, 40, 45, 50,
                                      55, 60, 25, 0,  0,   0,  0,  0,  0,  0};

    {
        // Writing column from current table to a file.
        employees_1.outputSecretShares("[AGE]", "../results/employees/employees_age_secrets_3pc_" +
                                                    std::to_string(runTime->getPartyID()) + ".csv");
        employees_4.inputSecretShares("[AGE]", "../tests/data/employees_age_secrets_3pc_" +
                                                   std::to_string(runTime->getPartyID()) + ".csv");
        auto opened_4 = employees_4.open_with_schema();
        assert(employeeAge_32.same_as(opened_4.first[0]));
        single_cout("Table: writing column secret shares ...OK");
        single_cout("Table: reading column secret shares ...OK");
    }

    ////////////////////////////////////////////
    //          WORKING WITH VECTORS          //
    ////////////////////////////////////////////

    {
        // Creating a BSharedVector using secret shares from a file
        BSharedVector<int32_t> ages(32, "../tests/data/employees_age_secrets_3pc_" +
                                            std::to_string(runTime->getPartyID()) + ".csv");
        auto ages_opened = ages.open();
        assert(employeeAge_32.same_as(ages_opened));
        single_cout("Vector: reading secret shares ...OK");
    }

    {
        BSharedVector<int32_t> ages(32, "../tests/data/employees_age_secrets_3pc_" +
                                            std::to_string(runTime->getPartyID()) + ".csv");
        std::sort(employeeAge_32.begin(), employeeAge_32.end());
        orq::operators::bitonic_sort(ages, SortOrder::ASC);
        ages.outputSecretShares("../results/employees/employees_sorted_age_secrets_3pc_" +
                                std::to_string(runTime->getPartyID()) + ".csv");
        BSharedVector<int32_t> ages_sorted(32, "../tests/data/employees_sorted_age_secrets_3pc_" +
                                                   std::to_string(runTime->getPartyID()) + ".csv");
        auto ages_sorted_opened = ages_sorted.open();
        assert(employeeAge_32.same_as(ages_sorted_opened));
        single_cout("Vector: writing secret shares ...OK");
    }
}