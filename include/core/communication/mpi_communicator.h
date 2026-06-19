#pragma once

#include "backend/common/setting.h"
#include "communicator.h"
#include "communicator_factory.h"
#include "debug/orq_debug.h"
#include "profiling/stopwatch.h"

#if defined(MPC_USE_MPI_COMMUNICATOR)
#include "mpi.h"
#endif

#include "profiling/thread_profiling.h"
using namespace orq::instrumentation;

namespace orq {

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

// \cond DOXYGEN_IGNORE
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
// \endcond DOXYGEN_IGNORE
#endif

class MPICommunicator : public Communicator {
    int numParties;
    int msg_tag;
    const int parallelism_factor = 1;

   public:
    MPICommunicator(int _currentId, int _msg_tag = 7, const std::string host_prefix = "localhost",
                    double latency = 0, double bandwidth = std::numeric_limits<double>::infinity(),
                    orq::service::Setting setting = orq::service::Setting::SAME)
        : Communicator(_currentId, host_prefix, latency, bandwidth, setting), msg_tag(_msg_tag) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        MPI_Comm_size(MPI_COMM_WORLD, &numParties);
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
    void sendShares_impl(const Vector<T> &_shares, PartyID _id) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        num_rounds += 1;
        auto _size = _shares.size();
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

    void sendShares(const Vector<int8_t> &shares, PartyID _id) override {
        sendShares_impl(shares, _id);
    }
    void sendShares(const Vector<int16_t> &shares, PartyID _id) override {
        sendShares_impl(shares, _id);
    }
    void sendShares(const Vector<int32_t> &shares, PartyID _id) override {
        sendShares_impl(shares, _id);
    }
    void sendShares(const Vector<int64_t> &shares, PartyID _id) override {
        sendShares_impl(shares, _id);
    }
    void sendShares(const Vector<__int128_t> &shares, PartyID _id) override {
        sendShares_impl(shares, _id);
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
    void receiveShares_impl(Vector<T> &_shares, PartyID _id) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        thread_stopwatch::InstrumentBlock _ib("comm");
        std::vector<MPI_Request> requests;

        auto _size = _shares.size();

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

    void receiveShares(Vector<int8_t> &_shareVector, PartyID _id) override {
        receiveShares_impl(_shareVector, _id);
    }

    void receiveShares(Vector<int16_t> &_shareVector, PartyID _id) override {
        receiveShares_impl(_shareVector, _id);
    }

    void receiveShares(Vector<int32_t> &_shareVector, PartyID _id) override {
        receiveShares_impl(_shareVector, _id);
    }

    void receiveShares(Vector<int64_t> &_shareVector, PartyID _id) override {
        receiveShares_impl(_shareVector, _id);
    }

    void receiveShares(Vector<__int128_t> &_shareVector, PartyID _id) override {
        receiveShares_impl(_shareVector, _id);
    }

    /**
     * @brief Single party version of exchange shares
     *
     * @tparam T
     * @param sent_shares
     * @param received_shares
     * @param _id
     * @param _size
     */
    template <typename T>
    void exchangeShares_impl(Vector<T> sent_shares, Vector<T> &received_shares, PartyID _id) {
        exchangeShares_impl(sent_shares, received_shares, _id, _id);
    }

    void exchangeShares(Vector<int8_t> sent_shares, Vector<int8_t> &received_shares,
                        PartyID _id) override {
        exchangeShares_impl(sent_shares, received_shares, _id);
    }

    void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t> &received_shares,
                        PartyID _id) override {
        exchangeShares_impl(sent_shares, received_shares, _id);
    }

    void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t> &received_shares,
                        PartyID _id) override {
        exchangeShares_impl(sent_shares, received_shares, _id);
    }

    void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t> &received_shares,
                        PartyID _id) override {
        exchangeShares_impl(sent_shares, received_shares, _id);
    }

    void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t> &received_shares,
                        PartyID _id) override {
        exchangeShares_impl(sent_shares, received_shares, _id);
    }

    template <typename T>
    void exchangeShares_impl(Vector<T> _shares, Vector<T> &received_shares, PartyID to_id,
                             PartyID from_id) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        num_rounds += 1;
        auto _size = _shares.size();
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
                        PartyID from_id) override {
        exchangeShares_impl(sent_shares, received_shares, to_id, from_id);
    }

    void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t> &received_shares,
                        PartyID to_id, PartyID from_id) override {
        exchangeShares_impl(sent_shares, received_shares, to_id, from_id);
    }

    void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t> &received_shares,
                        PartyID to_id, PartyID from_id) override {
        exchangeShares_impl(sent_shares, received_shares, to_id, from_id);
    }

    void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t> &received_shares,
                        PartyID to_id, PartyID from_id) override {
        exchangeShares_impl(sent_shares, received_shares, to_id, from_id);
    }

    void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t> &received_shares,
                        PartyID to_id, PartyID from_id) override {
        exchangeShares_impl(sent_shares, received_shares, to_id, from_id);
    }

    template <typename T>
    void sendShares_impl(const std::vector<Vector<T>> &shares, std::vector<PartyID> partyID) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        num_rounds += 1;
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
        num_rounds += 1;
        thread_stopwatch::InstrumentBlock _ib("comm");
        std::vector<MPI_Request> send_requests;
        std::vector<MPI_Request> recv_requests;
        std::vector<int> expected_recv_sources;
        std::vector<int> expected_recv_counts;

        assert(shares.size() == to_id.size());
        assert(received_shares.size() == from_id.size());

        size_t size_mul = 1;
        if constexpr (std::is_same_v<T, __int128_t>) {
            size_mul = 2;
        }

        for (size_t i = 0; i < to_id.size(); ++i) {
            int ind = (numParties + to_id[i] + this->currentId) % numParties;
            send_requests.push_back(MPI_Request());
            assert(!shares[i].has_mapping());

            assert(shares[i].size() == received_shares[i].size());

            MPI_Isend(&shares[i][0], shares[i].size() * size_mul, MPI_type<T>::v, ind, msg_tag,
                      MPI_COMM_WORLD, &send_requests.back());
        }

        for (size_t i = 0; i < from_id.size(); ++i) {
            int ind = (numParties + from_id[i] + this->currentId) % numParties;
            recv_requests.push_back(MPI_Request());
            expected_recv_sources.push_back(ind);
            assert(!received_shares[i].has_mapping());

            expected_recv_counts.push_back(static_cast<int>(received_shares[i].size() * size_mul));

            MPI_Irecv(&received_shares[i][0], received_shares[i].size() * size_mul, MPI_type<T>::v,
                      ind, msg_tag, MPI_COMM_WORLD, &recv_requests.back());
        }

        for (size_t i = 0; i < recv_requests.size(); ++i) {
            MPI_Status status;
            MPI_Wait(&recv_requests[i], &status);

            int recv_count = -1;
            MPI_Get_count(&status, MPI_type<T>::v, &recv_count);

            assert(status.MPI_SOURCE == expected_recv_sources[i]);
            assert(status.MPI_TAG == msg_tag);
            assert(recv_count == expected_recv_counts[i]);
        }

        for (size_t i = 0; i < send_requests.size(); ++i) {
            MPI_Wait(&send_requests[i], MPI_STATUS_IGNORE);
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
    MPICommunicatorFactory(CommFactoryArgs args)
        : msgTag_(args.msgTag), host_prefix(args.host_prefix), setting_(args.setting) {
#if defined(MPC_USE_MPI_COMMUNICATOR)
        int provided;

        // MPI_Init(&argc, &argv);
        MPI_Init_thread(&args.argc, &args.argv, MPI_THREAD_MULTIPLE, &provided);
        if (provided != MPI_THREAD_MULTIPLE) {
            printf("Sorry, this MPI implementation does not support multiple threads\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        MPI_Comm_rank(MPI_COMM_WORLD, &partyId_);
        MPI_Comm_size(MPI_COMM_WORLD, &numParties);
#endif

        this->latency = args.latency;
        this->bandwidth = args.bandwidth;

        orq::benchmarking::stopwatch::partyID = partyId_;
    }

    std::unique_ptr<Communicator> create() {
        static int instanceCount = 0;

        // Create a new communicator instance
        auto communicator =
            std::make_unique<MPICommunicator>(partyId_, msgTag_ + instanceCount * 10, host_prefix,
                                              this->latency, this->bandwidth, setting_);

        // Increment the instance count for the next communicator
        instanceCount++;

        return communicator;
    }

    void start() {}

    int getPartyId() const { return partyId_; }

    int getNumParties() const { return numParties; }

    orq::service::Setting getSetting() const { return setting_; }

    void blockingReady() {
        // No implementation needed for MPI
    }

   private:
    int partyId_;
    const int msgTag_;
    int numParties;
    const std::string host_prefix;
    const orq::service::Setting setting_;
};

}  // namespace orq
