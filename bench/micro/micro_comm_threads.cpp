#include <algorithm>
#include <cmath>
#include <iostream>

#include "orq.h"

using namespace orq::debug;
using namespace orq::service;
using namespace std::chrono;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// #define CORRECTNESS_CHECK

#if DEFAULT_BITWIDTH == 64
using T = int64_t;
#else
using T = int32_t;
#endif

const int SEND_PARTY_ID = 0;
const int RECV_PARTY_ID = 1;
const int SAMPLE_COUNT = 1000;

int64_t get_current_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void threadTask(int pID, int communicator_index, int test_size) {
    int party_offset = (pID == SEND_PARTY_ID) ? +1 : -1;

    std::array<int64_t, SAMPLE_COUNT> e2e_latency;

    // Add some data to the send batch
    orq::Vector<T> send_batch(test_size), recv_batch(test_size);
    for (int i = 0; i < test_size; i++) {
        send_batch[i] = i;
    }

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        int64_t currentTime = get_current_time();

        runTime->workers[communicator_index].getCommunicator()->exchangeShares(
            send_batch, recv_batch, party_offset, send_batch.size());

        e2e_latency[i] = get_current_time() - currentTime;

#ifdef CORRECTNESS_CHECK
        assert(send_batch.same_as(recv_batch));
#endif
    }

    if (pID == SEND_PARTY_ID && communicator_index == 0) {
        for (int i = 0; i < SAMPLE_COUNT - 1; i++) {
            std::cout << e2e_latency[i] << ",";
        }
        std::cout << e2e_latency[SAMPLE_COUNT - 1] << std::endl;
    }
}

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();

    int thread_num = runTime->get_num_threads();
    int test_size = 1 << 16;
    if (argc >= 5) {
        test_size = atoi(argv[4]);
    }

    std::string comm_suffix;
    if (COMMUNICATOR_NUM == MPI_COMMUNICATOR)
        comm_suffix = "MPI";
    else if (COMMUNICATOR_NUM == NOCOPY_COMMUNICATOR)
        comm_suffix = "NoCopyComm (" + std::to_string(NOCOPY_COMMUNICATOR_THREADS) + ")";

    single_cout(
        "Vector: " << test_size << " x " << std::numeric_limits<std::make_unsigned_t<T>>::digits
                   << "b | Sample count: " << SAMPLE_COUNT << " | Communicator: " << comm_suffix);

#ifdef CORRECTNESS_CHECK
    single_cout("Correctness check enabled");
#endif

    if (pID == SEND_PARTY_ID || pID == RECV_PARTY_ID) {
        std::vector<std::thread> threads;
        for (int i = 0; i < thread_num; ++i) {
            threads.emplace_back(threadTask, pID, i, test_size);
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }
    return 0;
}
