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

    std::vector<std::string> schema = {"[TIMESTAMP]", "TIMESTAMP", "[ID]", "[GAP_WINDOW]"};
    std::vector<secrecy::Vector<int>> data(schema.size(), test_size);
    EncodedTable<int> table = secret_share(data, schema);


    // start timer
    struct timeval begin, end; long seconds, micro; double elapsed;
    gettimeofday(&begin, 0);

    table.gap_session_window({"[ID]"},
                                   "TIMESTAMP", "[TIMESTAMP]",
                                   "[GAP_WINDOW]", 10, false);
    // stop timer
    gettimeofday(&end, 0);
    seconds = end.tv_sec - begin.tv_sec; micro = end.tv_usec - begin.tv_usec; elapsed = seconds + micro * 1e-6;
    if(pID == 0) {std::cout << "MICRO_SESSION_GAP:\t\t\t" << test_size << "\t\telapsed\t\t" << elapsed << std::endl;}

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}