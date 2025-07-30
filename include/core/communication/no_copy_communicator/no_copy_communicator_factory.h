#pragma once

#include "../../../service/socket/startmpc/startmpc.h"
#include "../communicator.h"
#include "../communicator_factory.h"
#include "no_copy_communicator.h"

namespace secrecy {

namespace service {
    bool RunTimeRunning();
}

namespace {

// TODO: refactor this so that the communicator does not know
//  about the runtime
#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
    void send_communication_thread(int rank, std::vector<int> communicator_index_list,
                                   int host_count, std::shared_ptr<std::barrier<>> b) {
        // Wait for all threads to start before continuing
        b->arrive_and_wait();

        // Fetch pointers to the communicators
        std::vector<secrecy::NoCopyCommunicator*> communicator_list;
        for (int i = 0; i < communicator_index_list.size(); i++) {
            while (service::runTime->workers.size() <= communicator_index_list[i]) {
                std::this_thread::sleep_for(std::chrono::milliseconds(SOCKET_COMMUNICATOR_WAIT_MS));
            }
            communicator_list.push_back(static_cast<secrecy::NoCopyCommunicator*>(
                service::runTime->workers[communicator_index_list[i]].getCommunicator()));
        }

        // Terminate when service::runTime exits
        while (service::RunTimeRunning()) {
            for (auto* communicator : communicator_list) {
                for (int j = 0; j < host_count; j++) {
                    if (j == rank) continue;

                    auto& partyInfo = communicator->get_party(j);
                    if (!partyInfo.sendRing.isRingEmpty()) {
                        NoCopyRingEntry* entry = partyInfo.sendRing.currentEntry();
                        size_t r =
                            send_wrapper(partyInfo.sockfd, entry->buffer, entry->buffer_size);
                        assert(r == entry->buffer_size);
                        partyInfo.sendRing.pop(entry);
                    }

                    if (!service::RunTimeRunning()) return;
                }
                if (!service::RunTimeRunning()) return;
            }
        }
    }

    void setup_send_threads(int rank, int threads_num, int host_count) {
        int num_communication_threads;
        if (NOCOPY_COMMUNICATOR_THREADS <= 0) {
            num_communication_threads = threads_num;
        } else {
            num_communication_threads = std::min(threads_num, NOCOPY_COMMUNICATOR_THREADS);
        }

        // Barrier for all threads (worker + main) to synchronize just after
        // thread creation. This may help prevent race conditions with threads
        // trying to access their PartyInfo maps _before_ (or while) they are
        // being instantiated.
        //
        // This was introduced while debugging the socket map race condition,
        // described below, but is unrelated. However, it's probably best to keep
        // this in so that all threads are synchronized.
        auto b = std::make_shared<std::barrier<>>(num_communication_threads + 1);

        if (rank == 0)
            std::cout << "NoCopyComm | Communication Threads: " << num_communication_threads
                      << std::endl;

        std::vector<std::vector<int>> communicator_index_list(num_communication_threads);

        // Round robin assignment
        for (int i = 0; i < threads_num; i++) {
            communicator_index_list[i % num_communication_threads].push_back(i);
        }

        // Send thread for each Secrecy thread
        for (int i = 0; i < num_communication_threads; i++) {
            service::runTime->emplace_socket_thread(send_communication_thread, rank,
                                                    communicator_index_list[i], host_count, b);
        }
        // Main thread waits for all send threads to start
        b->arrive_and_wait();
    }

    // TODO: Move this to the runtime, along with thread destruction
    void setup_communication_threads(int rank, int threads_num, int host_count) {
        setup_send_threads(rank, threads_num, host_count);
    }

#endif
}  // namespace

class NoCopyCommunicatorFactory : public CommunicatorFactory<NoCopyCommunicatorFactory> {
   public:
    NoCopyCommunicatorFactory(int argc, char** argv, int numParties, int threadsNum)
        : threadsNum_(threadsNum),
          numParties_(numParties) {
        socketMaps_.resize(threadsNum_);

        for (auto& m : socketMaps_) {
            m.resize(numParties);
        }

#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
        startmpc_init(&partyId_, numParties_, threadsNum_, socketMaps_);
#endif
    }

    std::unique_ptr<Communicator> create() {
        static int instanceCount = 0;

        // Create a new communicator instance
        auto communicator =
            std::make_unique<NoCopyCommunicator>(partyId_, socketMaps_[instanceCount], numParties_);

        // Increment the instance count for the next communicator
        instanceCount++;

        return communicator;
    }

    void start() {
#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
        setup_communication_threads(partyId_, threadsNum_, numParties_);
#endif
    }

    int getPartyId() const { return partyId_; }

    int getNumParties() const { return numParties_; }

    void blockingReady() {
        // No additional setup needed for NoCopyCommunicator
    }

   private:
    int partyId_;

    const int threadsNum_;
    const int numParties_;

    /**
     * @brief Vector of socket maps for each Secrecy thread. Each element of the
     * outer vector is assigned to a thread. The inner vector maps from party ID
     * to socket file descriptor:
     *
     * socket_maps[Thread #][Party ID] -> Socket file descriptor
     *
     * NOTE: this used to be a vector<map<int, int>>, but this led to a
     * concurrent write and thus an infrequent race condition.
     */
    std::vector<std::vector<int>> socketMaps_;
};

}  // namespace secrecy