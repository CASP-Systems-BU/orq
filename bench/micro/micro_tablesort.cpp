#include "orq.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#include <unistd.h>

#include <cmath>
// command
// mpirun -np 3 ./micro_radixsort 1 1 8192 $ROWS

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    int num_rows = 1024;
    int num_columns = 4;
    int num_sort_columns = 1;
    if (argc >= 5) {
        num_rows = atoi(argv[4]);
    }
    if (argc >= 6) {
        num_columns = atoi(argv[5]);
    }
    if (argc >= 7) {
        num_sort_columns = atoi(argv[6]);
    }

    auto localPRG = runTime->rand0()->localPRG.get();

    // generate a table
    std::vector<orq::Vector<int>> table_data;
    std::vector<std::string> schema;
    for (int i = 0; i < num_columns; i++) {
        table_data.push_back(orq::Vector<int>(num_rows));
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

    // start timer
    stopwatch::timepoint("Start");

    table1.sort(spec, orq::SortingProtocol::BITONICSORT);

    // stop timer
    stopwatch::timepoint("Table Bitonic Sort");

    table2.sort(spec, orq::SortingProtocol::QUICKSORT);

    // stop timer
    stopwatch::timepoint("Table Quicksort");

    table3.sort(spec, orq::SortingProtocol::RADIXSORT);

    // stop timer
    stopwatch::timepoint("Table Radix Sort");

    return 0;
}