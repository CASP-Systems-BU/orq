#include "orq.h"

using namespace orq::debug;
using namespace orq::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    auto test_size = runTime->getArg<size_t>("test-size", "r", 1 << 20);

    std::vector<std::string> schema = {"TIMESTAMP", "TUMBLING_WINDOW_PER_HOUR"};
    std::vector<orq::Vector<int64_t>> data(schema.size(), orq::Vector<int>(test_size));
    EncodedTable<int64_t> table = secret_share(data, schema);

    // start timer
    struct timeval begin, end;
    long seconds, micro;
    double elapsed;
    gettimeofday(&begin, 0);

    table.tumbling_window("TIMESTAMP", 3600, "TUMBLING_WINDOW_PER_HOUR");

    // stop timer
    gettimeofday(&end, 0);
    seconds = end.tv_sec - begin.tv_sec;
    micro = end.tv_usec - begin.tv_usec;
    elapsed = seconds + micro * 1e-6;
    if (pID == 0) {
        std::cout << "TUMBLING_QUERY:\t\t\t" << test_size << "\t\telapsed\t\t" << elapsed
                  << std::endl;
    }

    return 0;
}