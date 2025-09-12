#pragma once

#include <sys/socket.h>
#include <unistd.h>

#include <unordered_map>

#include "core/communication/communicator.h"
#include "debug/orq_debug.h"
#include "no_copy_ring.h"

/**
 * @brief Storage for a NoCopy send ring and socket file descriptor.
 *
 */
struct PartyInfoBasic {
    int sockfd;

    NoCopyRing sendRing;

    PartyInfoBasic(int sockfd, int ringSize) : sockfd(sockfd), sendRing(ringSize) {}
};

namespace orq {

/**
 * @brief No-copy communicator layer. Builds a ring buffer of pointers to
 * vectors, and sends out each vector sequentially, without introducing extra
 * copies.
 *
 */
class NoCopyCommunicator : public Communicator {
   public:
    /**
     * @brief Construct a new No Copy Communicator
     *
     * @param _currentId party ID of this node
     * @param socket_map map (stored as a vector) of party IDs to socket file
     * descriptors. The current party ID (i.e., `_currentID`) can contain any
     * value in this map and should be ignored.
     * @param _numParties The number of parties in this execution
     */
    NoCopyCommunicator(const int& _currentId, const std::vector<int>& socket_map,
                       const int& _numParties)
        : Communicator(_currentId), numParties(_numParties), _party_map(_numParties) {
#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
        for (int pID = 0; pID < socket_map.size(); pID++) {
            if (pID == _currentId) {
                continue;
            }
            auto sock = socket_map[pID];
            _party_map[pID] = std::make_unique<PartyInfoBasic>(sock, NOCOPY_COMMUNICATOR_RING_SIZE);
        }
#endif
    }

    ~NoCopyCommunicator() {
        for (auto& party : _party_map) {
            if (party) {
                close(party->sockfd);
            }
        }
    }

    //////////////////// Generic Functions Begin ////////////////////

    /**
     * @brief Generic function to print a string along with a type
     *
     * @tparam T the type to output
     * @param name name of the calling function
     * @param meta additional information to output
     */
    template <typename T>
    void printType(const std::string& name, const std::string& meta) {
#if defined(SOCKET_COMMUNICATOR_VERBOSE)
        std::string typeStr = std::is_same<T, int8_t>::value       ? "8"
                              : std::is_same<T, int16_t>::value    ? "16"
                              : std::is_same<T, int32_t>::value    ? "32"
                              : std::is_same<T, int64_t>::value    ? "64"
                              : std::is_same<T, __int128_t>::value ? "128"
                                                                   : "unknown";

        std::cout << currentId << "              " << name << ": " << typeStr << " | " << meta
                  << " |" << std::endl;
#endif
    }

    /**
     * @brief Send a single generic share. Creates a singleton vector and pushes
     * it to the send ring
     *
     * @tparam T type of the share
     * @param share
     * @param _id relative destination party
     */
    template <typename T>
    void sendShareGeneric(T share, PartyID _id) {
#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
        bytes_sent += sizeof(T);
        thread_stopwatch::InstrumentBlock _ib("comm");

        int to_id =
            (numParties + _id + this->currentId) % numParties;  // Convert relative ID to PartyID

        printType<T>("sendShare", "To: " + std::to_string(to_id));

        // TODO: Creating a vector here can be avoided by creating a single element push function
        orq::Vector<T> shareVector = {share};

        int pushIndex = get_party(to_id).sendRing.push(shareVector);

        get_party(to_id).sendRing.wait(pushIndex);
#endif
    }

    /**
     * @brief Send a shared vector. Pushes the vector to the send ring.
     *
     * @tparam T underlying type of the shared vector
     * @param _shares
     * @param _id relative destination party. +1 is the next party, -1 is the
     * previous.
     * @param _size size of the vector (TODO: use size of `_shares`)
     */
    template <typename T>
    void sendSharesGeneric(const Vector<T>& _shares, PartyID _id, size_t _size) {
#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
        bytes_sent += (sizeof(T) * _size);
        thread_stopwatch::InstrumentBlock _ib("comm");

        int to_id =
            (numParties + _id + this->currentId) % numParties;  // Convert relative ID to PartyID

        assert(!_shares.has_mapping());

        printType<T>("sendSharesVector", "To: " + std::to_string(to_id));

        int pushIndex = get_party(to_id).sendRing.push(_shares);

        get_party(to_id).sendRing.wait(pushIndex);
#endif
    }

    /**
     * @brief Receive a single share. Directly calls the `recv` function.
     *
     * @tparam T
     * @param _share
     * @param _id
     */
    template <typename T>
    void receiveShareGeneric(T& _share, PartyID _id) {
#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
        thread_stopwatch::InstrumentBlock _ib("comm");

        int from_id =
            (numParties + _id + this->currentId) % numParties;  // Convert relative ID to PartyID

        printType<T>("receiveShare", "From: " + std::to_string(from_id));

        int sockfd = get_party(from_id).sockfd;

        T data;
        recv(sockfd, &data, sizeof(data), 0);

        _share = data;
#endif
    }

    /**
     * @brief Receive a shared vector. Directly calls the `recv` function,
     * operating in chunks if the entire vector is not read in a single call.
     *
     * @tparam T underlying type of the shared vector
     * @param _shareVector
     * @param _id relative source party. +1 is the next party, -1 is the
     * previous.
     * @param _size number of elements to receive
     */
    template <typename T>
    void receiveSharesGeneric(Vector<T>& _shareVector, PartyID _id, size_t _size) {
#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
        thread_stopwatch::InstrumentBlock _ib("comm");

        int from_id =
            (numParties + _id + this->currentId) % numParties;  // Convert relative ID to PartyID

        assert(!_shareVector.has_mapping());

        printType<T>("receiveSharesVector", "From: " + std::to_string(from_id));

        int sockfd = get_party(from_id).sockfd;

        size_t totalBytes = _size * sizeof(T);
        size_t bytesProcessed = 0;

        while (bytesProcessed < totalBytes) {
            size_t remainingBytes = totalBytes - bytesProcessed;

            char* dataPtr = reinterpret_cast<char*>(&_shareVector[0]) + bytesProcessed;

            ssize_t receivedBytes = recv(sockfd, dataPtr, remainingBytes, 0);

            if (receivedBytes > 0) {
                bytesProcessed += receivedBytes;
            } else {
                break;
            }
        }
#endif
    }

    /**
     * @brief Concurrently send and receive two vectors. Pushes the send vector
     * to the send ring (which is sent out in the background), and then populate
     * the receive vector by reading from the socket
     *
     * @tparam T
     * @param sent_shares
     * @param received_shares
     * @param _to_id relative destination party. +1 is the next party, -1 is the
     * previous.
     * @param _from_id relative source party
     * @param _size number of elements in each vector (TODO: remove)
     */
    template <typename T>
    void exchangeSharesGeneric(Vector<T> sent_shares, Vector<T>& received_shares, PartyID _to_id,
                               PartyID _from_id, size_t _size) {
#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
        bytes_sent += (sizeof(T) * _size);
        thread_stopwatch::InstrumentBlock _ib("comm");

        int to_id = (numParties + _to_id + this->currentId) % numParties;
        int from_id = (numParties + _from_id + this->currentId) % numParties;

        assert(!sent_shares.has_mapping());
        assert(!received_shares.has_mapping());

        printType<T>("exchangeShares",
                     "To: " + std::to_string(to_id) + " | From: " + std::to_string(from_id));

        int pushIndex = get_party(to_id).sendRing.push(sent_shares);

        size_t totalBytes = _size * sizeof(T);
        size_t bytesProcessed = 0;

        int sockfd = get_party(from_id).sockfd;

        while (bytesProcessed < totalBytes) {
            size_t remainingBytes = totalBytes - bytesProcessed;

            char* dataPtr = reinterpret_cast<char*>(&received_shares[0]) + bytesProcessed;

            ssize_t receivedBytes = recv(sockfd, dataPtr, remainingBytes, 0);

            if (receivedBytes > 0) {
                bytesProcessed += receivedBytes;
            } else {
                break;
            }
        }

        get_party(to_id).sendRing.wait(pushIndex);
#endif
    }

    /**
     * @brief Send shares to multiple parties. `shares` and `partyID` must be
     * vectors of the same length.
     *
     * @tparam T
     * @param shares vector of Vectors; one for each party.
     * @param partyID vector of relative party IDs to send to.
     */
    template <typename T>
    void sendSharesGeneric(const std::vector<Vector<T>>& shares, std::vector<PartyID> partyID) {
#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
        thread_stopwatch::InstrumentBlock _ib("comm");

        assert(shares.size() == partyID.size());

        std::vector<int> pushIndices(shares.size());
        for (int party_idx = 0; party_idx < partyID.size(); ++party_idx) {
            printType<T>("SendingMultipleVectors", "To: " + std::to_string(partyID[party_idx]));
            assert(!shares[party_idx].has_mapping());

            auto size = shares[party_idx].size();
            bytes_sent += (sizeof(T) * size);

            // Convert relative ID to PartyID
            int to_id = (numParties + partyID[party_idx] + this->currentId) % numParties;

            // Push the shares to the send ring
            pushIndices[party_idx] = get_party(to_id).sendRing.push(shares[party_idx]);
        }

        for (int party_idx = 0; party_idx < partyID.size(); ++party_idx) {
            // Convert relative ID to PartyID
            int to_id = (numParties + partyID[party_idx] + this->currentId) % numParties;

            // Wait for the push to complete
            get_party(to_id).sendRing.wait(pushIndices[party_idx]);
        }
#endif
    }

    /**
     * @brief Receive shares from multiple parties. `shares` and `partyID` must
     * be vectors of the same length.
     *
     * @tparam T
     * @param shares vector of Vectors to receive into
     * @param partyID relative source party IDs
     */
    template <typename T>
    void receiveBroadcastGeneric(std::vector<Vector<T>>& shares, std::vector<PartyID> partyID) {
#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
        thread_stopwatch::InstrumentBlock _ib("comm");

        assert(shares.size() == partyID.size());

        printType<T>("receiveBroadcast", "");

        for (int party_idx = 0; party_idx < partyID.size(); ++party_idx) {
            // Convert relative ID to PartyID
            int from_id = (numParties + partyID[party_idx] + this->currentId) % numParties;

            int sockfd = get_party(from_id).sockfd;

            auto size = shares[party_idx].size();

            size_t totalBytes = size * sizeof(T);
            size_t bytesProcessed = 0;

            assert(!shares[party_idx].has_mapping());

            std::vector<T> temp_recv_vector(size);

            while (bytesProcessed < totalBytes) {
                size_t remainingBytes = totalBytes - bytesProcessed;
                size_t bytesToCopy = std::min(remainingBytes, SOCKET_COMMUNICATOR_BUFFER_SIZE);

                char* dataPtr = reinterpret_cast<char*>(temp_recv_vector.data()) + bytesProcessed;

                ssize_t receivedBytes = recv(sockfd, dataPtr, bytesToCopy, 0);

                if (receivedBytes > 0) {
                    bytesProcessed += receivedBytes;
                } else {
                    break;
                }
            }

            for (size_t i = 0; i < shares[party_idx].size(); i++) {
                shares[party_idx][i] = temp_recv_vector[i];
            }
        }
#endif
    }

    /**
     * @brief Exchange shares to and from multiple parties. `shares` and `to_id`
     * must have the same length, as should `received_shares` and `from_id`.
     *
     * @tparam T
     * @param shares vector of Vectors to send
     * @param received_shares vector of Vectors to receive into
     * @param to_id relative destination IDs
     * @param from_id relative source IDs
     */
    template <typename T>
    void exchangeSharesGeneric(const std::vector<Vector<T>>& shares,
                               std::vector<Vector<T>>& received_shares, std::vector<PartyID> to_id,
                               std::vector<PartyID> from_id) {
#if defined(MPC_USE_NO_COPY_COMMUNICATOR)
        thread_stopwatch::InstrumentBlock _ib("comm");

        assert(shares.size() == to_id.size());

        // First, let's start sending the shares
        std::vector<int> pushIndices(shares.size());
        for (int party_idx = 0; party_idx < to_id.size(); ++party_idx) {
            printType<T>("SendingMultipleVectors", "To: " + std::to_string(to_id[party_idx]));
            assert(!shares[party_idx].has_mapping());

            auto size = shares[party_idx].size();
            bytes_sent += (sizeof(T) * size);

            // Convert relative ID to PartyID
            int ind = (numParties + to_id[party_idx] + this->currentId) % numParties;

            // Push the shares to the send ring
            pushIndices[party_idx] = get_party(ind).sendRing.push(shares[party_idx]);
        }

        // Now, let's receive the shares
        receiveBroadcastGeneric(received_shares, from_id);

        // Make sure that all vectors are sent.
        for (int party_idx = 0; party_idx < to_id.size(); ++party_idx) {
            // Convert relative ID to PartyID
            int ind = (numParties + to_id[party_idx] + this->currentId) % numParties;

            // Wait for the push to complete
            get_party(ind).sendRing.wait(pushIndices[party_idx]);
        }
#endif
    }

    ///////////////////// Generic Functions End /////////////////////

    void sendShare(int8_t share, PartyID _id) { sendShareGeneric(share, _id); }

    void sendShare(int16_t share, PartyID _id) { sendShareGeneric(share, _id); }

    void sendShare(int32_t share, PartyID _id) { sendShareGeneric(share, _id); }

    void sendShare(int64_t share, PartyID _id) { sendShareGeneric(share, _id); }

    void sendShares(const Vector<int8_t>& _shares, PartyID _id, size_t _size) {
        sendSharesGeneric(_shares, _id, _size);
    }

    void sendShares(const Vector<int16_t>& _shares, PartyID _id, size_t _size) {
        sendSharesGeneric(_shares, _id, _size);
    }

    void sendShares(const Vector<int32_t>& _shares, PartyID _id, size_t _size) {
        sendSharesGeneric(_shares, _id, _size);
    }

    void sendShares(const Vector<int64_t>& _shares, PartyID _id, size_t _size) {
        sendSharesGeneric(_shares, _id, _size);
    }

    void sendShares(const Vector<__int128_t>& _shares, PartyID _id, size_t _size) {
        sendSharesGeneric(_shares, _id, _size);
    }

    void receiveShare(int8_t& _share, PartyID _id) { receiveShareGeneric(_share, _id); }

    void receiveShare(int16_t& _share, PartyID _id) { receiveShareGeneric(_share, _id); }

    void receiveShare(int32_t& _share, PartyID _id) { receiveShareGeneric(_share, _id); }

    void receiveShare(int64_t& _share, PartyID _id) { receiveShareGeneric(_share, _id); }

    void receiveShares(Vector<int8_t>& _shareVector, PartyID _id, size_t _size) {
        receiveSharesGeneric(_shareVector, _id, _size);
    }

    void receiveShares(Vector<int16_t>& _shareVector, PartyID _id, size_t _size) {
        receiveSharesGeneric(_shareVector, _id, _size);
    }

    void receiveShares(Vector<int32_t>& _shareVector, PartyID _id, size_t _size) {
        receiveSharesGeneric(_shareVector, _id, _size);
    }

    void receiveShares(Vector<int64_t>& _shareVector, PartyID _id, size_t _size) {
        receiveSharesGeneric(_shareVector, _id, _size);
    }

    void receiveShares(Vector<__int128_t>& _shareVector, PartyID _id, size_t _size) {
        receiveSharesGeneric(_shareVector, _id, _size);
    }

    void exchangeShares(Vector<int8_t> sent_shares, Vector<int8_t>& received_shares, PartyID _id,
                        size_t _size) {
        exchangeShares(sent_shares, received_shares, _id, _id, _size);
    }

    void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t>& received_shares, PartyID _id,
                        size_t _size) {
        exchangeShares(sent_shares, received_shares, _id, _id, _size);
    }

    void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t>& received_shares, PartyID _id,
                        size_t _size) {
        exchangeShares(sent_shares, received_shares, _id, _id, _size);
    }

    void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t>& received_shares, PartyID _id,
                        size_t _size) {
        exchangeShares(sent_shares, received_shares, _id, _id, _size);
    }

    void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t>& received_shares,
                        PartyID _id, size_t _size) {
        exchangeShares(sent_shares, received_shares, _id, _id, _size);
    }

    void exchangeShares(Vector<int8_t> sent_shares, Vector<int8_t>& received_shares, PartyID to_id,
                        PartyID from_id, size_t _size) {
        exchangeSharesGeneric(sent_shares, received_shares, to_id, from_id, _size);
    }

    void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t>& received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) {
        exchangeSharesGeneric(sent_shares, received_shares, to_id, from_id, _size);
    }

    void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t>& received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) {
        exchangeSharesGeneric(sent_shares, received_shares, to_id, from_id, _size);
    }

    void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t>& received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) {
        exchangeSharesGeneric(sent_shares, received_shares, to_id, from_id, _size);
    }

    void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t>& received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) {
        exchangeSharesGeneric(sent_shares, received_shares, to_id, from_id, _size);
    }

    void sendShares(const std::vector<Vector<int8_t>>& shares, std::vector<PartyID> partyID) {
        sendSharesGeneric(shares, partyID);
    }

    void sendShares(const std::vector<Vector<int16_t>>& shares, std::vector<PartyID> partyID) {
        sendSharesGeneric(shares, partyID);
    }

    void sendShares(const std::vector<Vector<int32_t>>& shares, std::vector<PartyID> partyID) {
        sendSharesGeneric(shares, partyID);
    }

    void sendShares(const std::vector<Vector<int64_t>>& shares, std::vector<PartyID> partyID) {
        sendSharesGeneric(shares, partyID);
    }

    void sendShares(const std::vector<Vector<__int128_t>>& shares, std::vector<PartyID> partyID) {
        sendSharesGeneric(shares, partyID);
    }

    void receiveBroadcast(std::vector<Vector<int8_t>>& shares, std::vector<PartyID> partyID) {
        receiveBroadcastGeneric(shares, partyID);
    }

    void receiveBroadcast(std::vector<Vector<int16_t>>& shares, std::vector<PartyID> partyID) {
        receiveBroadcastGeneric(shares, partyID);
    }

    void receiveBroadcast(std::vector<Vector<int32_t>>& shares, std::vector<PartyID> partyID) {
        receiveBroadcastGeneric(shares, partyID);
    }

    void receiveBroadcast(std::vector<Vector<int64_t>>& shares, std::vector<PartyID> partyID) {
        receiveBroadcastGeneric(shares, partyID);
    }

    void receiveBroadcast(std::vector<Vector<__int128_t>>& shares, std::vector<PartyID> partyID) {
        receiveBroadcastGeneric(shares, partyID);
    }

    void exchangeShares(const std::vector<Vector<int8_t>>& shares,
                        std::vector<Vector<int8_t>>& received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        exchangeSharesGeneric(shares, received_shares, to_id, from_id);
    }

    void exchangeShares(const std::vector<Vector<int16_t>>& shares,
                        std::vector<Vector<int16_t>>& received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        exchangeSharesGeneric(shares, received_shares, to_id, from_id);
    }

    void exchangeShares(const std::vector<Vector<int32_t>>& shares,
                        std::vector<Vector<int32_t>>& received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        exchangeSharesGeneric(shares, received_shares, to_id, from_id);
    }

    void exchangeShares(const std::vector<Vector<int64_t>>& shares,
                        std::vector<Vector<int64_t>>& received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        exchangeSharesGeneric(shares, received_shares, to_id, from_id);
    }

    void exchangeShares(const std::vector<Vector<__int128_t>>& shares,
                        std::vector<Vector<__int128_t>>& received_shares,
                        std::vector<PartyID> to_id, std::vector<PartyID> from_id) {
        exchangeSharesGeneric(shares, received_shares, to_id, from_id);
    }

    /**
     * @brief Get the party info object for the given (absolute) party. Performs
     * a bounds check on the ID and asserts that the struct pointer is not null.
     *
     * @param id
     * @return PartyInfoBasic&
     */
    PartyInfoBasic& get_party(int id) {
        assert(id >= 0 && id < numParties);
        assert(_party_map[id] != nullptr);

        return *_party_map[id];
    }

   private:
    int numParties;
    std::vector<std::unique_ptr<PartyInfoBasic>> _party_map;
};
}  // namespace orq
