#include "../../include/secrecy.h"

using namespace secrecy::debug;
using namespace secrecy::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();
    int test_size = 128;
    if(argc >= 5){
        test_size = atoi(argv[4]);
    }

    std::vector<std::string> schema = {"[TIMESTAMP]", "TIMESTAMP",
                                       "[MACHINE_TYPE]", "[EVENT_TYPE]", "[JOB_ID]",
                                       "[SEL]", "SEL", "[GAP_WINDOW]", "TOTAL_TASKS_PER_SESSION"};
    std::vector<secrecy::Vector<int>> cloud_data(schema.size(), test_size);
    EncodedTable<int> cloud_table = secret_share(cloud_data, schema);

    // TODO: use operation with public constant
    EncodedTable<int> constant_table = secret_share(std::vector<secrecy::Vector<int>>(1, test_size),
                                               {"[ZERO]"});

    // start timer
    struct timeval begin, end; long seconds, micro; double elapsed;
    gettimeofday(&begin, 0);

    // selection on event type == 0
    // sort by machine type, timestamp
    // keyed_gap window on machine type
    //      -- compute sum of selected per session and machine type
    cloud_table["[SEL]"] = cloud_table["[EVENT_TYPE]"] == constant_table["[ZERO]"];
    cloud_table.convert_b2a_bit("[SEL]", "SEL");
    cloud_table.sort({{"[MACHINE_TYPE]", ASC}, {"[TIMESTAMP]", ASC}}, {"TIMESTAMP", "SEL"});
    cloud_table.gap_session_window({"[MACHINE_TYPE]"},
                                   "TIMESTAMP", "[TIMESTAMP]",
                                   "[GAP_WINDOW]", 10, false);

    using A = ASharedVector<int>;
    cloud_table.aggregate({"[MACHINE_TYPE]", "[GAP_WINDOW]"}, {
        {"SEL", "TOTAL_TASKS_PER_SESSION", secrecy::aggregators::sum<A>}
    });

    // stop timer
    gettimeofday(&end, 0);
    seconds = end.tv_sec - begin.tv_sec; micro = end.tv_usec - begin.tv_usec; elapsed = seconds + micro * 1e-6;
    if(pID == 0) {std::cout << "CLOUD_QUERY:\t\t\t" << test_size << "\t\telapsed\t\t" << elapsed << std::endl;}


#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}