#include "../../include/secrecy.h"

using namespace secrecy::service::mpi_service::replicated_3pc;

int main(int argc, char **argv){
    secrecy_init(argc, argv);
    auto pID = secrecy::service::runTime->getPartyID();

    ////////////////////////////////////////////
    //          WORKING WITH TBALES           //
    ////////////////////////////////////////////
    // Creating table .. always put "SEL" in the schema
    const std::string tableName = "Employee_Profile";
    std::vector<std::string> schema = {"[EMPLOYEE_ID]", "[REGION]", "[AGE]"};
    const int employeesSize = 32;
    EncodedTable<int32_t> employees(tableName, schema, employeesSize);

    // Reading input from file with party 0 using plain text.
    // employees.inputCSVTableData("../examples/employees.csv", 0);

    // Reading input from file using secret shares.
    employees.inputCSVTableSecretShares("../examples/employees_secrets_3pc_" + std::to_string(runTime->getPartyID()) + ".csv");

    // Writing current table secret shares to a file.
    // employees.outputCSVTableSecretShares("../examples/employees_secrets_3pc_" + std::to_string(runTime->getPartyID()) + ".csv");

    // Writing column from current table to a file.
    employees.outputSecretShares("[AGE]", "../examples/employees_age_secrets_3pc_" + std::to_string(runTime->getPartyID()) + ".csv");

    // sorting table
    employees.sort({std::make_pair(ENC_TABLE_VALID, SortOrder::DESC), std::make_pair("[AGE]", SortOrder::ASC)});

    // output table
    auto opened = employees.open_with_schema();

    // printing table
    secrecy::debug::print_table(opened, pID);
    if (pID ==0) std::cout << std::endl;

    
    ////////////////////////////////////////////
    //          WORKING WITH VECTORS          //
    ////////////////////////////////////////////

    // Creating a BSharedVector using secret shares from a file
    BSharedVector<int32_t> ages(32, "../examples/employees_age_secrets_3pc_" + std::to_string(runTime->getPartyID()) + ".csv");

    // Sorting vector
    secrecy::operators::bitonic_sort(ages, SortOrder::ASC);

    // Writing current vector secret shares to a file.
    ages.outputSecretShares("../examples/employees_sorted_age_secrets_3pc_" + std::to_string(runTime->getPartyID()) + ".csv");

    // Opening vector
    auto opened_ages = ages.open();

    // Printing vector
    secrecy::debug::print(opened_ages, pID);
    if (pID ==0) std::cout << std::endl;    

    return 0;
}