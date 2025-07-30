#include "../../include/secrecy.h"
#include "../../include/benchmark/stopwatch.h"

using namespace secrecy::debug;
using namespace secrecy::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#define GLUCOSE_THRESHOLD 5

#define LOAD_DATA
// #define CORRECTNESS_CHECK

#ifdef CORRECTNESS_CHECK
// Since this a time-series query, SQL isn't helpful. Implement in pure C++
void correctness_check(EncodedTable<int> &in, EncodedTable<int> &out) {

    single_cout("==== Correctness Check ====");
    single_cout_nonl("opening secret-shared tables... ");

    auto plain_data = in.open_with_schema();

    auto insulin = in.get_column(plain_data, "INSULIN");
    auto glucose = in.get_column(plain_data, "[GLUCOSE]");
    auto patient = in.get_column(plain_data, "[PATIENT_ID]");
    auto timestamp = in.get_column(plain_data, "[TIMESTAMP]");

    auto plain_result = out.open_with_schema();

    auto total_events = out.get_column(plain_result, "TOTAL_EVENTS");
    auto patient_result = out.get_column(plain_result, "[PATIENT_ID]");
    auto window_ids = out.get_column(plain_result, "[THRESHOLD_WINDOW]");

    single_cout("OK");

    if (runTime->getPartyID() != 0) { return; }

    std::cout << "Computing threshold window... ";

    std::vector<int> _windows;
    std::vector<int> _events;

    bool inside_window = false;
    int current_window = -1;
    int current_events = 0;
    int last_patient = patient[0];
    for (int row = 0; row < patient.size(); row++) {
        if (patient[row] == last_patient && glucose[row] > GLUCOSE_THRESHOLD) {
            if (! inside_window) {
                inside_window = true;
                current_window = timestamp[row];
                _windows.push_back(current_window);
            }
        } else {
            if (inside_window) {
                // not any more
                _events.push_back(current_events);
                current_events = 0;
            }

            last_patient = patient[row];
            inside_window = false;
        }

        if (inside_window) {
            current_events += insulin[row];
        }
    }

    // ended inside a window. update
    if (inside_window) {
        _events.push_back(current_events);
    }

    std::cout << "OK\n";

    std::cout << "Comparing results... ";

    assert(window_ids.same_as(_windows));
    assert(total_events.same_as(_events));

    std::cout << "OK\n";

}
#endif

int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();

    // test file is ~4300 rows but we need power of two
    int test_size = 8192;
    if(argc >= 5){
        test_size = atoi(argv[4]);
    }

    std::vector<std::string> schema = {"[TIMESTAMP]", "[PATIENT_ID]",
                                        "[GLUCOSE]", "INSULIN"};

#ifdef LOAD_DATA
    EncodedTable<int> medical_table("Glucose/Insulin", schema, test_size);

    // Load plaintext data...
    medical_table.inputCSVTableData("../examples/medical/ex-glucose-data-noisy.csv", 0);

    // Or load secret shares
    // medical_table.inputCSVTableSecretShares("../examples/medical/ex-glucose-share-" + std::to_string(pID) + ".csv");
#else
    // Dummy (empty) data. TODO: load data from one or more CSV
    std::vector<secrecy::Vector<int>> medical_data (schema.size(), test_size);
    EncodedTable<int> medical_table = secret_share(medical_data, schema);
#endif

#ifdef CORRECTNESS_CHECK
    EncodedTable<int> orig = medical_table.deepcopy();
#endif

    stopwatch::timepoint("start");

    medical_table.sort({"[PATIENT_ID]", "[TIMESTAMP]"}, ASC);

    stopwatch::timepoint("sort");

    medical_table.addColumns({"[THRESHOLD_WINDOW]", "TOTAL_EVENTS"}, test_size);

    medical_table.threshold_session_window({"[PATIENT_ID]"},
                                           "[GLUCOSE]", "[TIMESTAMP]",
                                           "[THRESHOLD_WINDOW]", GLUCOSE_THRESHOLD, false);

    stopwatch::timepoint("session window");

    /* This call WILL mark final result rows valid. If further computation
     * was occurring, we might want to sort on VALID + trim, but there's no need
     * here. We will simply send the entire shared table to the frontend.
     */
    using A = ASharedVector<int>;
    medical_table.aggregate({"[PATIENT_ID]", "[THRESHOLD_WINDOW]"},
    {
        {"INSULIN", "TOTAL_EVENTS", secrecy::aggregators::sum<A>}
    });

    // Remove intermediate columns
    medical_table.deleteColumns({"INSULIN", "[GLUCOSE]", "[TIMESTAMP]"});

    // Mask out invalid rows + shuffle for privacy.
    medical_table.finalize();

    // TODO: shuffle the table before output to prevent inference attacks on
    // intermediate counts
    // medical_table.shuffle();

    stopwatch::timepoint("aggregate");
    stopwatch::done();

    single_cout("medical query: size " << test_size);

#ifdef LOAD_DATA
    // TODO: create dir if doesn't exist
    medical_table.outputCSVTableSecretShares("../results/medical/ex-glucose-out-share-" + std::to_string(pID) + ".csv");
#endif

#ifdef CORRECTNESS_CHECK
    // make sure in the same order
    orig.sort({"[PATIENT_ID]", "[TIMESTAMP]"}, ASC);
    correctness_check(orig, medical_table);
#endif

    MPI_Finalize();
}