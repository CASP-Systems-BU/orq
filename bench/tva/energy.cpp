#include "orq.h"

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;
using namespace orq::debug;
using namespace orq::service;

// ../scripts/run_experiment.py -p 3 -r 4 energy

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = runTime->getArg<int>("test-size", "r", 8);  // default for startmpc/mpirun

    std::vector<std::string> schema = {"[TIMESTAMP]",
                                       "TIMESTAMP",
                                       "[DEVICE_ID]",
                                       "TUMBLING_WINDOW_PER_HOUR",
                                       "[TUMBLING_WINDOW_PER_HOUR]",
                                       "ENERGY_CONSUMPTION",
                                       "TOTAL_CONSUMPTION"};

    std::vector<orq::Vector<int32_t>> energy_data(schema.size(), Vector<int32_t>(test_size));

    EncodedTable<int32_t> energy_table = secret_share<int32_t>(energy_data, schema);
    ASharedVector<int64_t> timestamp_a(test_size);

    // start timer
    stopwatch::timepoint("Start");

    // energy_table.tumbling_window("TIMESTAMP", 3600, "TUMBLING_WINDOW_PER_HOUR");
    ASharedVector<int64_t> window_id = timestamp_a / 3600;
    (*energy_table["TUMBLING_WINDOW_PER_HOUR"].contents.get()) = window_id;
    energy_table.convert_a2b("TUMBLING_WINDOW_PER_HOUR", "[TUMBLING_WINDOW_PER_HOUR]");
    energy_table.sort({{"[TUMBLING_WINDOW_PER_HOUR]", ASC}}, {"ENERGY_CONSUMPTION"});

    using A = ASharedVector<int>;
    energy_table.aggregate({"[TUMBLING_WINDOW_PER_HOUR]"},
                           {{"ENERGY_CONSUMPTION", "TOTAL_CONSUMPTION", orq::aggregators::sum<A>}});

    stopwatch::done();
    return 0;
}