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

    std::vector<std::string> schema = {"[C1]", "[C2]", "[C3]", "[C4]",
                                        "[C5]", "[C6]", "[C7]", "[C8]", "[SEL]"};
    std::vector<secrecy::Vector<int8_t>> cloud_data(schema.size(), test_size);
    EncodedTable<int8_t> data_table = secret_share(cloud_data, schema);

    // TODO: use operation with public constant
    EncodedTable<int8_t> constant_table = secret_share(std::vector<secrecy::Vector<int>>(8, test_size),
                                                {"[c1]", "[c2]", "[c3]", "[c4]",
                                                "[c5]", "[c6]", "[c7]", "[c8]"});

    // start timer
    struct timeval begin, end; long seconds, micro; double elapsed;
    gettimeofday(&begin, 0);


    data_table["[SEL]"] = (data_table["[C1]"] < constant_table["[c1]"])
                        & (data_table["[C2]"] < constant_table["[c2]"])
                        & (data_table["[C3]"] < constant_table["[c3]"])
                        & (data_table["[C4]"] < constant_table["[c4]"])
                        & (data_table["[C5]"] < constant_table["[c5]"])
                        & (data_table["[C6]"] < constant_table["[c6]"])
                        & (data_table["[C7]"] < constant_table["[c7]"])
                        & (data_table["[C8]"] < constant_table["[c8]"]);


    // stop timer
    gettimeofday(&end, 0);
    seconds = end.tv_sec - begin.tv_sec; micro = end.tv_usec - begin.tv_usec; elapsed = seconds + micro * 1e-6;
    if(pID == 0) {std::cout << "WALDO_TABLE_GREATER:\t\t\t" << test_size << "\t\telapsed\t\t" << elapsed << std::endl;}


#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}