#include "orq.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#include <unistd.h>

#include <cmath>

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();

    auto test_size = runTime->getArg<size_t>("test-size", "r", 1 << 20);
    auto num_columns = runTime->getArg<int>("num-columns", "nc", 4);
    auto num_sort_columns = runTime->getArg<int>("num-sort-columns", "ns", 1);

    auto localPRG = runTime->rand0()->localPRG.get();

    // generate a table
    std::vector<orq::Vector<int>> table_data;
    std::vector<std::string> schema;
    for (int i = 0; i < num_columns; i++) {
        table_data.push_back(orq::Vector<int>(test_size));
        schema.push_back("[" + std::to_string(i) + "]");
        localPRG->getNext(table_data[i]);
    }
    EncodedTable<int> table1 = secret_share(table_data, schema);
    EncodedTable<int> table2 = secret_share(table_data, schema);
    EncodedTable<int> table3 = secret_share(table_data, schema);

    std::vector<std::pair<std::string, SortOrder>> spec;
    for (int i = 0; i < num_sort_columns; i++) {
        spec.push_back(std::make_pair("[" + std::to_string(i) + "]", ASC));
    }
    spec.push_back(std::make_pair(ENC_TABLE_VALID, ASC));

    stopwatch::timepoint("Start");
    stopwatch::profile_init();

    table1.sort(spec, orq::SortingProtocol::NETWORK);
    stopwatch::timepoint("Table Sorting Network");

    table2.sort(spec, orq::SortingProtocol::QUICKSORT);
    stopwatch::timepoint("Table Quicksort");

    table3.sort(spec, orq::SortingProtocol::RADIXSORT);
    stopwatch::timepoint("Table Radixsort");

    stopwatch::profile_done();

    return 0;
}