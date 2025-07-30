#ifndef GAVEL_SHARED_MEMORY_COMMUNICATOR_H
#define GAVEL_SHARED_MEMORY_COMMUNICATOR_H

#include <map>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

#include "communicator.h"

namespace secrecy {

    template<typename Share, typename ShareVector>
    class SharedMemoryCommunicator : public Communicator {

        typedef std::map<std::pair<int, int>, std::atomic<int> > CommunicatorSize;
        typedef std::map<std::pair<int, int>, std::queue<ShareVector> > CommunicatorQueue;
        typedef std::map<std::pair<int, int>, std::mutex> CommunicatorGuard;
        typedef std::shared_ptr<CommunicatorQueue> CommunicatorQueuePtr;
        typedef std::shared_ptr<CommunicatorGuard> CommunicatorGuardPtr;
        typedef std::shared_ptr<CommunicatorSize> CommunicatorSizePtr;

        CommunicatorQueuePtr shares;
        CommunicatorGuardPtr guards;
        CommunicatorSizePtr sizes;
        int numParties;

    public:

        SharedMemoryCommunicator(int _currentId, CommunicatorQueuePtr _shares, CommunicatorGuardPtr _guards,
                                 CommunicatorSizePtr _sizes, const int &_numParties)
                : Communicator (_currentId), shares(_shares), guards(_guards),
                  sizes(_sizes), numParties(_numParties) {}

        ~SharedMemoryCommunicator() {}

        void sendShares(Share share, PartyID id) {}

        void sendShares(ShareVector _shares, PartyID _id, int _size) {
            int i = (_id + this->currentId) % numParties;
            int j = this->currentId;

            guards.get()[0][{i, j}].lock();

            shares.get()[0][{i, j}].push(_shares);

#ifdef MPC_COMMUNICATOR_PRINT_DATA
            printf("%d:\t(%d, %d) is being filled\n", partyNumber, i, j);
    //        if (partyNumber == 1)
    //            std::cout << vector.get()[0][{i, j}].front() << std::endl;
#endif
            sizes.get()[0][{i, j}]++;
            guards.get()[0][{i, j}].unlock();
#ifdef MPC_COMMUNICATOR_PRINT_DATA
            printf("%d:\t(%d, %d) has been filled\n", partyNumber, i, j);
#endif
        }

        Share receiveShare(PartyID id) {
            return 0;
        }

        ShareVector receiveShares(PartyID _id, int _size) {
            int i = this->currentId;
            int j = (_id + this->currentId) % numParties;

            while (sizes.get()[0][{i, j}] < 1) {
#ifdef MPC_COMMUNICATOR_PRINT_DATA
                //            printf("%d:\t(%d, %d) trying to emptying\n", partyNumber, i, j);
#endif
            }

            guards.get()[0][{i, j}].lock();
            sizes.get()[0][{i, j}]--;
            auto _shares = shares.get()[0][{i, j}].front();
            shares.get()[0][{i, j}].pop();
#ifdef MPC_COMMUNICATOR_PRINT_DATA
            printf("%d:\t(%d, %d) is being emptied\n", partyNumber, i, j);
    //        if (partyNumber == 1)
    //            std::cout << _shares << std::endl;
#endif

            guards.get()[0][{i, j}].unlock();
#ifdef MPC_COMMUNICATOR_PRINT_DATA
            printf("%d:\t(%d, %d) has been emptied\n", partyNumber, i, j);
#endif
            return _shares;
        }

        ShareVector exchangeShares(ShareVector _shares, PartyID _id, int _size) {
            sendShares(_shares, _id, _size);
            return receiveShares(_id, _size);
        }

        ShareVector exchangeShares(ShareVector _shares, PartyID to_id, PartyID from_id, int _size){
            sendShares(_shares, to_id, _size);
            return receiveShares(from_id, _size);
        }

        static std::vector<SharedMemoryCommunicator> createCommunicators(int num_parties) {
            CommunicatorQueuePtr shares(new CommunicatorQueue());
            CommunicatorGuardPtr guards(new CommunicatorGuard());
            CommunicatorSizePtr sizes(new CommunicatorSize());

            // initialize elements in the map
            for (int i = 0; i < num_parties; ++i) {
                for (int j = 0; j < num_parties; ++j) {
                    shares.get()[0][{i, j}];
                    guards.get()[0][{i, j}];
                    sizes.get()[0][{i, j}] = 0;
                }
            }

            std::vector<SharedMemoryCommunicator> communicators;
            for (int i = 0; i < num_parties; ++i) {
                communicators.push_back(SharedMemoryCommunicator(i, shares, guards, sizes, num_parties));
            }

            return communicators;
        }
    };
}
#endif //GAVEL_SHARED_MEMORY_COMMUNICATOR_H
