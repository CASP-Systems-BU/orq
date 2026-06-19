#include "orq.h"

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;
using namespace orq::debug;
using namespace orq::service;

// ../scripts/run_experiment.py -p 3 -r 4 medical

#define GLUCOSE_THRESHOLD 5

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = runTime->getArg<int>("test-size", "r", 8);  // default for startmpc/mpirun

    std::vector<std::string> schema = {"[TIMESTAMP]", "[PATIENT_ID]", "[GLUCOSE]", "INSULIN"};

    std::vector<orq::Vector<int32_t>> medical_data(schema.size(), Vector<int32_t>(test_size));
    EncodedTable<int32_t> medical_table = secret_share<int32_t>(medical_data, schema);

    // start timer
    stopwatch::timepoint("Start");

    medical_table.sort({"[PATIENT_ID]", "[TIMESTAMP]"}, ASC);

    medical_table.addColumns({"[THRESHOLD_WINDOW]", "TOTAL_EVENTS"});

    medical_table.threshold_session_window({"[PATIENT_ID]"}, "[GLUCOSE]", "[TIMESTAMP]",
                                           "[THRESHOLD_WINDOW]", GLUCOSE_THRESHOLD, false);

    /* This call WILL mark final result rows valid. If further computation
     * was occurring, we might want to sort on VALID + trim, but there's no need
     * here. We will simply send the entire shared table to the frontend.
     */
    using A = ASharedVector<int>;
    medical_table.aggregate({"[PATIENT_ID]", "[THRESHOLD_WINDOW]"},
                            {{"INSULIN", "TOTAL_EVENTS", orq::aggregators::sum<A>}});

    // Remove intermediate columns
    medical_table.deleteColumns({"INSULIN", "[GLUCOSE]", "[TIMESTAMP]"});

    // Mask out invalid rows + shuffle for privacy.
    medical_table.finalize();

    stopwatch::done();
    return 0;
}