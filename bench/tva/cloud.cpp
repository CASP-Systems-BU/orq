#include "orq.h"

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;
using namespace orq::debug;
using namespace orq::service;

// ../scripts/run_experiment.py -p 3 -r 4 cloud

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = runTime->getArg<int>("test-size", "r", 8);  // default for startmpc/mpirun

    std::vector<std::string> schema = {"[TIMESTAMP]",  "TIMESTAMP",    "[MACHINE_TYPE]",
                                       "[EVENT_TYPE]", "[JOB_ID]",     "[SEL]",
                                       "SEL",          "[GAP_WINDOW]", "TOTAL_TASKS_PER_SESSION"};

    std::vector<orq::Vector<int32_t>> cloud_data(schema.size(), Vector<int32_t>(test_size));
    EncodedTable<int32_t> cloud_table = secret_share<int32_t>(cloud_data, schema);

    // start timer
    stopwatch::timepoint("Start");

    // selection on event type == 0
    // sort by machine type, timestamp
    // keyed_gap window on machine type
    //      -- compute sum of selected per session and machine type
    cloud_table["[SEL]"] = cloud_table["[EVENT_TYPE]"] == 0;
    cloud_table.convert_b2a_bit("[SEL]", "SEL");
    cloud_table.sort({{"[MACHINE_TYPE]", ASC}, {"[TIMESTAMP]", ASC}}, {"TIMESTAMP", "SEL"});
    cloud_table.gap_session_window({"[MACHINE_TYPE]"}, "TIMESTAMP", "[TIMESTAMP]", "[GAP_WINDOW]",
                                   10, false);

    using A = ASharedVector<int>;
    cloud_table.aggregate({"[MACHINE_TYPE]", "[GAP_WINDOW]"},
                          {{"SEL", "TOTAL_TASKS_PER_SESSION", orq::aggregators::sum<A>}});

    stopwatch::done();
    return 0;
}