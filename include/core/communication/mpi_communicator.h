#ifndef SECRECY_MPI_COMMUNICATOR_H
#define SECRECY_MPI_COMMUNICATOR_H

#include "../../benchmark/stopwatch.h"
#include "../../debug/debug.h"
#include "communicator.h"
#include "communicator_factory.h"

#if defined(MPC_USE_MPI_COMMUNICATOR)
#include "mpi.h"
#endif

#include "../../benchmark/thread_profiling.h"
using namespace secrecy::instrumentation;

namespace secrecy {

#ifdef MPC_USE_MPI_COMMUNICATOR

/**
 * @brief define a correspondence between C++ types and MPI enumerated
 * datatypes.
 *
 * This allows for a single generic implementation of
 * communication primitives, and also provides a workaround for 128-bit
 * types: tell MPI we're actually sending a 64-bit vector of twice the
 * length.
 *
 * @tparam T
 */
template <typename T>
struct MPI_type;

template <>
struct MPI_type<int8_t> {
    inline static const MPI_Datatype v = MPI_INT8_T;
};
template <>
struct MPI_type<int16_t> {
    inline static const MPI_Datatype v = MPI_INT16_T;
};
template <>
struct MPI_type<int32_t> {
    inline static const MPI_Datatype v = MPI_INT32_T;
};
template <>
struct MPI_type<int64_t> {
    inline static const MPI_Datatype v = MPI_INT64_T;
};
template <>
struct MPI_type<__int128_t> {
    // fallback: MPI does not support 128-bit types natively
    // so we send each half as a 64b value.
    inline static const MPI_Datatype v = MPI_INT64_T;
};
#endif

class MPICommunicator : public Communicator {
    int numParties;
    int msg_tag;
    int parallelism_factor;

   public:
    MPICommunicator(const int &_currentId, const int &_numParties, const int &_msg_tag = 7,
                    const int &_parallelism_factor = 1)
        : Communicator(_currentId),
          numParties(_numParties),
          msg_tag(_msg_tag),
          parallelism_factor(_parallelism_factor) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        int __rank, __num_parties;

        MPI_Comm_rank(MPI_COMM_WORLD, &__rank);
        MPI_Comm_size(MPI_COMM_WORLD, &__num_parties);

        // this protocol works with 3 parties only
        if (__rank == 0 && __num_parties != numParties) {
            fprintf(stderr, "ERROR: The number of MPI processes must be %d not %d\n", numParties,
                    __num_parties);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
#endif
    }

    ~MPICommunicator() {}

    template <typename T>
    void sendShare_impl(T share, PartyID _id) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        bytes_sent += sizeof(T);
        thread_stopwatch::InstrumentBlock _ib("comm");
        int ind = (numParties + _id + this->currentId) % numParties;
        MPI_Send(&share, 1, MPI_type<T>::v, ind, msg_tag, MPI_COMM_WORLD);
#endif
    }

    void sendShare(int8_t share, PartyID _id) override { sendShare_impl(share, _id); }

    void sendShare(int16_t share, PartyID _id) override { sendShare_impl(share, _id); }

    void sendShare(int32_t share, PartyID _id) override { sendShare_impl(share, _id); }

    void sendShare(int64_t share, PartyID _id) override { sendShare_impl(share, _id); }

    template <typename T>
    void sendShares_impl(const Vector<T> &_shares, PartyID _id, size_t _size) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        bytes_sent += (sizeof(T) * _size);
        thread_stopwatch::InstrumentBlock _ib("comm");
        int to_ind = (numParties + _id + this->currentId) % numParties;
        std::vector<MPI_Request> requests;

        assert(!_shares.has_mapping());

        if constexpr (std::is_same_v<T, __int128_t>) {
            _size *= 2;
        }

        int start = 0;
        size_t __size = _size / parallelism_factor;
        for (int i = 0; i < parallelism_factor; ++i) {
            size_t ___size = (i == parallelism_factor - 1) ? _size - start : __size;

            requests.push_back(MPI_Request());

            MPI_Isend(&_shares[start], ___size, MPI_type<T>::v, to_ind, msg_tag + i, MPI_COMM_WORLD,
                      &requests[i]);
            start += ___size;
        }

        for (size_t i = 0; i < requests.size(); ++i) {
            MPI_Wait(&requests[i], MPI_STATUS_IGNORE);
        }
#endif
    }

    void sendShares(const Vector<int8_t> &shares, PartyID _id, size_t _size) override {
        sendShares_impl(shares, _id, _size);
    }
    void sendShares(const Vector<int16_t> &shares, PartyID _id, size_t _size) override {
        sendShares_impl(shares, _id, _size);
    }
    void sendShares(const Vector<int32_t> &shares, PartyID _id, size_t _size) override {
        sendShares_impl(shares, _id, _size);
    }
    void sendShares(const Vector<int64_t> &shares, PartyID _id, size_t _size) override {
        sendShares_impl(shares, _id, _size);
    }
    void sendShares(const Vector<__int128_t> &shares, PartyID _id, size_t _size) override {
        sendShares_impl(shares, _id, _size);
    }

    template <typename T>
    void receiveShare_impl(T &_share, PartyID _id) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        thread_stopwatch::InstrumentBlock _ib("comm");
        int ind = (numParties + _id + this->currentId) % numParties;
        MPI_Recv(&_share, 1, MPI_type<T>::v, ind, msg_tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#endif
    }

    void receiveShare(int8_t &_share, PartyID _id) override { receiveShare_impl(_share, _id); }
    void receiveShare(int16_t &_share, PartyID _id) override { receiveShare_impl(_share, _id); }
    void receiveShare(int32_t &_share, PartyID _id) override { receiveShare_impl(_share, _id); }
    void receiveShare(int64_t &_share, PartyID _id) override { receiveShare_impl(_share, _id); }

    template <typename T>
    void receiveShares_impl(Vector<T> &_shares, PartyID _id, size_t _size) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        thread_stopwatch::InstrumentBlock _ib("comm");
        std::vector<MPI_Request> requests;

        assert(!_shares.has_mapping());

        // Make sure we have enough room
        assert(_shares.size() >= _size);

        int from_ind = (numParties + _id + this->currentId) % numParties;

        if constexpr (std::is_same_v<T, __int128_t>) {
            _size *= 2;
        }

        size_t start = 0;
        size_t __size = _size / parallelism_factor;
        for (size_t i = 0; i < parallelism_factor; ++i) {
            size_t ___size = (i == parallelism_factor - 1) ? _size - start : __size;
            requests.push_back(MPI_Request());

            MPI_Irecv(&_shares[start], ___size, MPI_type<T>::v, from_ind, msg_tag + i,
                      MPI_COMM_WORLD, &requests[i]);
            start += ___size;
        }

        for (size_t i = 0; i < requests.size(); ++i) {
            MPI_Wait(&requests[i], MPI_STATUS_IGNORE);
        }
#endif
    }

    void receiveShares(Vector<int8_t> &_shareVector, PartyID _id, size_t _size) override {
        receiveShares_impl(_shareVector, _id, _size);
    }

    void receiveShares(Vector<int16_t> &_shareVector, PartyID _id, size_t _size) override {
        receiveShares_impl(_shareVector, _id, _size);
    }

    void receiveShares(Vector<int32_t> &_shareVector, PartyID _id, size_t _size) override {
        receiveShares_impl(_shareVector, _id, _size);
    }

    void receiveShares(Vector<int64_t> &_shareVector, PartyID _id, size_t _size) override {
        receiveShares_impl(_shareVector, _id, _size);
    }

    void receiveShares(Vector<__int128_t> &_shareVector, PartyID _id, size_t _size) override {
        receiveShares_impl(_shareVector, _id, _size);
    }

    /**
     * @brief Exchange shares - single party specifier version
     *
     * @tparam T
     * @param sent_shares
     * @param received_shares
     * @param _id
     * @param _size
     */
    template <typename T>
    void exchangeShares_impl(Vector<T> sent_shares, Vector<T> &received_shares, PartyID _id,
                             size_t _size) {
        exchangeShares_impl(sent_shares, received_shares, _id, _id, _size);
    }

    void exchangeShares(Vector<int8_t> sent_shares, Vector<int8_t> &received_shares, PartyID _id,
                        size_t _size) override {
        exchangeShares_impl(sent_shares, received_shares, _id, _size);
    }

    void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t> &received_shares, PartyID _id,
                        size_t _size) override {
        exchangeShares_impl(sent_shares, received_shares, _id, _size);
    }

    void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t> &received_shares, PartyID _id,
                        size_t _size) override {
        exchangeShares_impl(sent_shares, received_shares, _id, _size);
    }

    void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t> &received_shares, PartyID _id,
                        size_t _size) override {
        exchangeShares_impl(sent_shares, received_shares, _id, _size);
    }

    void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t> &received_shares,
                        PartyID _id, size_t _size) override {
        exchangeShares_impl(sent_shares, received_shares, _id, _size);
    }

    template <typename T>
    void exchangeShares_impl(Vector<T> _shares, Vector<T> &received_shares, PartyID to_id,
                             PartyID from_id, size_t _size) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        bytes_sent += (sizeof(T) * _size);
        thread_stopwatch::InstrumentBlock _ib("comm");
        int to_ind = (numParties + to_id + this->currentId) % numParties;
        int from_ind = (numParties + from_id + this->currentId) % numParties;

        std::vector<MPI_Request> requests;

        assert(!_shares.has_mapping());
        assert(!received_shares.has_mapping());
        assert(received_shares.size() >= _size);

        if constexpr (std::is_same_v<T, __int128_t>) {
            _size *= 2;
        }

        size_t start = 0;
        size_t __size = _size / parallelism_factor;
        for (size_t i = 0; i < parallelism_factor; ++i) {
            size_t ___size = (i == parallelism_factor - 1) ? _size - start : __size;

            requests.push_back(MPI_Request());
            requests.push_back(MPI_Request());

            MPI_Irecv(&received_shares[start], ___size, MPI_type<T>::v, from_ind, msg_tag + i,
                      MPI_COMM_WORLD, &requests[2 * i]);
            MPI_Isend(&_shares[start], ___size, MPI_type<T>::v, to_ind, msg_tag + i, MPI_COMM_WORLD,
                      &requests[2 * i + 1]);
            start += ___size;
        }

        for (size_t i = 0; i < requests.size(); ++i) {
            MPI_Wait(&requests[i], MPI_STATUS_IGNORE);
        }
#endif
    }

    void exchangeShares(Vector<int8_t> sent_shares, Vector<int8_t> &received_shares, PartyID to_id,
                        PartyID from_id, size_t _size) override {
        exchangeShares_impl(sent_shares, received_shares, to_id, from_id, _size);
    }

    void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t> &received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) override {
        exchangeShares_impl(sent_shares, received_shares, to_id, from_id, _size);
    }

    void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t> &received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) override {
        exchangeShares_impl(sent_shares, received_shares, to_id, from_id, _size);
    }

    void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t> &received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) override {
        exchangeShares_impl(sent_shares, received_shares, to_id, from_id, _size);
    }

    void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t> &received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) override {
        exchangeShares_impl(sent_shares, received_shares, to_id, from_id, _size);
    }

    template <typename T>
    void sendShares_impl(const std::vector<Vector<T>> &shares, std::vector<PartyID> partyID) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        thread_stopwatch::InstrumentBlock _ib("comm");
        std::vector<MPI_Request> requests;

        assert(shares.size() == partyID.size());

        size_t size_mul = 1;
        if constexpr (std::is_same_v<T, __int128_t>) {
            size_mul = 2;
        }

        for (size_t i = 0; i < partyID.size(); ++i) {
            int to_ind = (numParties + partyID[i] + this->currentId) % numParties;
            requests.push_back(MPI_Request());
            assert(!shares[i].has_mapping());
            MPI_Isend(&shares[i][0], shares[i].size() * size_mul, MPI_type<T>::v, to_ind, msg_tag,
                      MPI_COMM_WORLD, &requests[i]);
        }

        for (size_t i = 0; i < requests.size(); ++i) {
            MPI_Wait(&requests[i], MPI_STATUS_IGNORE);
        }
#endif
    }

    void sendShares(const std::vector<Vector<int8_t>> &shares,
                    std::vector<PartyID> partyID) override {
        sendShares_impl(shares, partyID);
    }

    void sendShares(const std::vector<Vector<int16_t>> &shares,
                    std::vector<PartyID> partyID) override {
        sendShares_impl(shares, partyID);
    }

    void sendShares(const std::vector<Vector<int32_t>> &shares,
                    std::vector<PartyID> partyID) override {
        sendShares_impl(shares, partyID);
    }

    void sendShares(const std::vector<Vector<int64_t>> &shares,
                    std::vector<PartyID> partyID) override {
        sendShares_impl(shares, partyID);
    }

    void sendShares(const std::vector<Vector<__int128_t>> &shares,
                    std::vector<PartyID> partyID) override {
        sendShares_impl(shares, partyID);
    }

    template <typename T>
    void receiveBroadcast_impl(std::vector<Vector<T>> &shares, std::vector<PartyID> partyID) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        thread_stopwatch::InstrumentBlock _ib("comm");
        std::vector<MPI_Request> requests;

        assert(shares.size() == partyID.size());

        size_t size_mul = 1;
        if constexpr (std::is_same_v<T, __int128_t>) {
            size_mul = 2;
        }

        for (size_t i = 0; i < partyID.size(); ++i) {
            int to_ind = (numParties + partyID[i] + this->currentId) % numParties;
            requests.push_back(MPI_Request());
            assert(!shares[i].has_mapping());
            MPI_Irecv(&shares[i][0], shares[i].size() * size_mul, MPI_type<T>::v, to_ind, msg_tag,
                      MPI_COMM_WORLD, &requests[i]);
        }

        for (size_t i = 0; i < requests.size(); ++i) {
            MPI_Wait(&requests[i], MPI_STATUS_IGNORE);
        }
#endif
    }

    void receiveBroadcast(std::vector<Vector<int8_t>> &shares,
                          std::vector<PartyID> partyID) override {
        receiveBroadcast_impl(shares, partyID);
    }

    void receiveBroadcast(std::vector<Vector<int16_t>> &shares,
                          std::vector<PartyID> partyID) override {
        receiveBroadcast_impl(shares, partyID);
    }

    void receiveBroadcast(std::vector<Vector<int32_t>> &shares,
                          std::vector<PartyID> partyID) override {
        receiveBroadcast_impl(shares, partyID);
    }

    void receiveBroadcast(std::vector<Vector<int64_t>> &shares,
                          std::vector<PartyID> partyID) override {
        receiveBroadcast_impl(shares, partyID);
    }

    void receiveBroadcast(std::vector<Vector<__int128_t>> &shares,
                          std::vector<PartyID> partyID) override {
        receiveBroadcast_impl(shares, partyID);
    }

    template <typename T>
    void exchangeShares_impl(const std::vector<Vector<T>> &shares,
                             std::vector<Vector<T>> &received_shares, std::vector<PartyID> to_id,
                             std::vector<PartyID> from_id) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        thread_stopwatch::InstrumentBlock _ib("comm");
        std::vector<MPI_Request> requests;

        assert(shares.size() == to_id.size());
        assert(shares.size() == from_id.size());

        size_t size_mul = 1;
        if constexpr (std::is_same_v<T, __int128_t>) {
            size_mul = 2;
        }

        for (size_t i = 0; i < from_id.size(); ++i) {
            int ind = (numParties + from_id[i] + this->currentId) % numParties;
            requests.push_back(MPI_Request());
            assert(!shares[i].has_mapping());
            MPI_Isend(&shares[i][0], shares[i].size() * size_mul, MPI_type<T>::v, ind, msg_tag,
                      MPI_COMM_WORLD, &requests.back());
        }

        for (size_t i = 0; i < to_id.size(); ++i) {
            int ind = (numParties + to_id[i] + this->currentId) % numParties;
            requests.push_back(MPI_Request());
            assert(!received_shares[i].has_mapping());
            MPI_Irecv(&received_shares[i][0], received_shares[i].size() * size_mul, MPI_type<T>::v,
                      ind, msg_tag, MPI_COMM_WORLD, &requests.back());
        }

        for (size_t i = 0; i < requests.size(); ++i) {
            MPI_Wait(&requests[i], MPI_STATUS_IGNORE);
        }
#endif
    }

    void exchangeShares(const std::vector<Vector<int8_t>> &shares,
                        std::vector<Vector<int8_t>> &received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) override {
        exchangeShares_impl(shares, received_shares, to_id, from_id);
    }

    void exchangeShares(const std::vector<Vector<int16_t>> &shares,
                        std::vector<Vector<int16_t>> &received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) override {
        exchangeShares_impl(shares, received_shares, to_id, from_id);
    }

    void exchangeShares(const std::vector<Vector<int32_t>> &shares,
                        std::vector<Vector<int32_t>> &received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) override {
        exchangeShares_impl(shares, received_shares, to_id, from_id);
    }

    void exchangeShares(const std::vector<Vector<int64_t>> &shares,
                        std::vector<Vector<int64_t>> &received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) override {
        exchangeShares_impl(shares, received_shares, to_id, from_id);
    }

    void exchangeShares(const std::vector<Vector<__int128_t>> &shares,
                        std::vector<Vector<__int128_t>> &received_shares,
                        std::vector<PartyID> to_id, std::vector<PartyID> from_id) override {
        exchangeShares_impl(shares, received_shares, to_id, from_id);
    }
};

class MPICommunicatorFactory : public CommunicatorFactory<MPICommunicatorFactory> {
   public:
    MPICommunicatorFactory(int argc, char **argv, int numParties, int threadsNum, int msgTag = 1000,
                           int parallelismFactor = 1)
        : numParties_(numParties), msgTag_(msgTag), parallelismFactor_(parallelismFactor) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        int provided;

        // MPI_Init(&argc, &argv);
        MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
        if (provided != MPI_THREAD_MULTIPLE) {
            printf("Sorry, this MPI implementation does not support multiple threads\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        MPI_Comm_rank(MPI_COMM_WORLD, &partyId_);
#endif

        secrecy::benchmarking::stopwatch::partyID = partyId_;
    }

    ~MPICommunicatorFactory() {
        // #if defined(MPC_USE_MPI_COMMUNICATOR)
        //             MPI_Finalize();
        // #endif
    }

    std::unique_ptr<Communicator> create() {
        static int instanceCount = 0;

        // Create a new communicator instance
        auto communicator = std::make_unique<MPICommunicator>(
            partyId_, numParties_, msgTag_ + instanceCount * 10, parallelismFactor_);

        // Increment the instance count for the next communicator
        instanceCount++;

        return communicator;
    }

    void start() {}

    int getPartyId() const { return partyId_; }

    int getNumParties() const { return numParties_; }

    void blockingReady() {
        // No implementation needed for MPI
    }

   private:
    int partyId_;
    const int numParties_;
    const int msgTag_;
    const int parallelismFactor_;
};

}  // namespace secrecy

#endif  // SECRECY_MPI_COMMUNICATOR_H
