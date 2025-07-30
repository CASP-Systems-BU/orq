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

    // TODO: do aggregation by device_id
    std::vector<std::string> schema = {"[TIMESTAMP]", "TIMESTAMP", "[DEVICE_ID]",
                                       "TUMBLING_WINDOW_PER_HOUR", "[TUMBLING_WINDOW_PER_HOUR]",
                                       "ENERGY_CONSUMPTION", "TOTAL_CONSUMPTION"};
    std::vector<secrecy::Vector<int32_t>> energy_data(schema.size(), test_size);

    EncodedTable<int32_t> energy_table = secret_share(energy_data, schema);
    ASharedVector<int64_t> timestamp_a(test_size);


    // start timer
    struct timeval begin, end; long seconds, micro; double elapsed;
    gettimeofday(&begin, 0);

    // energy_table.tumbling_window("TIMESTAMP", 3600, "TUMBLING_WINDOW_PER_HOUR");
    ASharedVector<int64_t> window_id = timestamp_a / 3600;
    (*energy_table["TUMBLING_WINDOW_PER_HOUR"].contents.get()) = window_id;
    energy_table.convert_a2b("TUMBLING_WINDOW_PER_HOUR", "[TUMBLING_WINDOW_PER_HOUR]");
    energy_table.sort({{"[TUMBLING_WINDOW_PER_HOUR]", ASC}}, {"ENERGY_CONSUMPTION"});
    
    using A = ASharedVector<int>;
    energy_table.aggregate({"[TUMBLING_WINDOW_PER_HOUR]"}, {
        {"ENERGY_CONSUMPTION", "TOTAL_CONSUMPTION", secrecy::aggregators::sum<A>}
    });

    // stop timer
    gettimeofday(&end, 0);
    seconds = end.tv_sec - begin.tv_sec; micro = end.tv_usec - begin.tv_usec; elapsed = seconds + micro * 1e-6;
    if(pID == 0) {std::cout << "ENERGY_QUERY:\t\t\t" << test_size << "\t\telapsed\t\t" << elapsed << std::endl;}
    

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}