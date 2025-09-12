#include "orq.h"

using namespace orq::debug;
using namespace orq::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 128;
    if (argc >= 5) {
        test_size = atoi(argv[4]);
    }

    std::vector<std::string> schema = {"[SEL]", "DATA", "[DATA]", "SUM", "[MAX]", "[MIN]"};
    std::vector<orq::Vector<int>> data(schema.size(), test_size);
    EncodedTable<int> table = secret_share(data, schema);

    // start timer
    struct timeval begin, end;
    long seconds, micro;
    double elapsed;
    gettimeofday(&begin, 0);

    using A = ASharedVector<int>;

    table.aggregate({"[SEL]"}, {{"DATA", "SUM", orq::aggregators::sum<A>}});

    // stop timer
    gettimeofday(&end, 0);
    seconds = end.tv_sec - begin.tv_sec;
    micro = end.tv_usec - begin.tv_usec;
    elapsed = seconds + micro * 1e-6;
    if (pID == 0) {
        std::cout << "SUM_AGGREGATION:\t\t\t" << test_size << "\t\telapsed\t\t" << elapsed
                  << std::endl;
    }

    gettimeofday(&begin, 0);

    using B = BSharedVector<int>;

    table.aggregate({"[SEL]"}, {
                                   {"[DATA]", "[MIN]", orq::aggregators::min<B>},
                               });

    gettimeofday(&end, 0);
    seconds = end.tv_sec - begin.tv_sec;
    micro = end.tv_usec - begin.tv_usec;
    elapsed = seconds + micro * 1e-6;
    if (pID == 0) {
        std::cout << "MIN_AGGREGATION:\t\t\t" << test_size << "\t\telapsed\t\t" << elapsed
                  << std::endl;
    }

    gettimeofday(&begin, 0);

    table.aggregate({"[SEL]"}, {{"[DATA]", "[MAX]", orq::aggregators::max<B>}});

    gettimeofday(&end, 0);
    seconds = end.tv_sec - begin.tv_sec;
    micro = end.tv_usec - begin.tv_usec;
    elapsed = seconds + micro * 1e-6;
    if (pID == 0) {
        std::cout << "MAX_AGGREGATION:\t\t\t" << test_size << "\t\telapsed\t\t" << elapsed
                  << std::endl;
    }

    stopwatch::done();          // print wall clock time
    stopwatch::profile_done();  // print profiling data

    runTime->print_statistics();
    runTime->print_communicator_statistics();

    return 0;
}