#ifndef SECRECY_NULL_COMMUNICATOR_H
#define SECRECY_NULL_COMMUNICATOR_H

#include "../../debug/debug.h"
#include "communicator.h"
#include "communicator_factory.h"

namespace secrecy {
class NullCommunicator : public Communicator {
    int numParties;

   public:
    /**
     * @brief Null Communicator that does nothing - for the Plaintext 1PC
     * test protocol.
     * @param _currentId
     */
    NullCommunicator() : Communicator(0), numParties(1) {}

    ~NullCommunicator() {}

    template <typename T>
    void checkMapping(const Vector<T> &v) {
        assert(!v.has_mapping());
    }

    void sendShare(int8_t share, PartyID _id) {}
    void sendShare(int16_t share, PartyID _id) {}
    void sendShare(int32_t share, PartyID _id) {}
    void sendShare(int64_t share, PartyID _id) {}

    void sendShares(const Vector<int8_t> &_shares, PartyID _id, size_t _size) {
        checkMapping(_shares);
    }
    void sendShares(const Vector<int16_t> &_shares, PartyID _id, size_t _size) {
        checkMapping(_shares);
    }
    void sendShares(const Vector<int32_t> &_shares, PartyID _id, size_t _size) {
        checkMapping(_shares);
    }
    void sendShares(const Vector<int64_t> &_shares, PartyID _id, size_t _size) {
        checkMapping(_shares);
    }
    void sendShares(const Vector<__int128_t> &_shares, PartyID _id, size_t _size) {
        checkMapping(_shares);
    }

    void receiveShare(int8_t &_share, PartyID _id) {}
    void receiveShare(int16_t &_share, PartyID _id) {}
    void receiveShare(int32_t &_share, PartyID _id) {}
    void receiveShare(int64_t &_share, PartyID _id) {}

    void receiveShares(Vector<int8_t> &_shares, PartyID _id, size_t _size) {
        checkMapping(_shares);
    }
    void receiveShares(Vector<int16_t> &_shares, PartyID _id, size_t _size) {
        checkMapping(_shares);
    }
    void receiveShares(Vector<int32_t> &_shares, PartyID _id, size_t _size) {
        checkMapping(_shares);
    }
    void receiveShares(Vector<int64_t> &_shares, PartyID _id, size_t _size) {
        checkMapping(_shares);
    }
    void receiveShares(Vector<__int128_t> &_shares, PartyID _id, size_t _size) {
        checkMapping(_shares);
    }

    void exchangeShares(Vector<int8_t> sent_shares, Vector<int8_t> &received_shares, PartyID to_id,
                        PartyID from_id, size_t _size) {
        checkMapping(sent_shares);
        checkMapping(received_shares);
        received_shares = sent_shares;
    }

    void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t> &received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) {
        checkMapping(sent_shares);
        checkMapping(received_shares);
        received_shares = sent_shares;
    }

    void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t> &received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) {
        checkMapping(sent_shares);
        checkMapping(received_shares);
        received_shares = sent_shares;
    }

    void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t> &received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) {
        checkMapping(sent_shares);
        checkMapping(received_shares);
        received_shares = sent_shares;
    }

    void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t> &received_shares,
                        PartyID to_id, PartyID from_id, size_t _size) {
        checkMapping(sent_shares);
        checkMapping(received_shares);
        received_shares = sent_shares;
    }

    void exchangeShares(Vector<int8_t> sent_shares, Vector<int8_t> &received_shares, PartyID id,
                        size_t _size) {
        checkMapping(sent_shares);
        checkMapping(received_shares);
        received_shares = sent_shares;
    }

    void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t> &received_shares, PartyID id,
                        size_t _size) {
        checkMapping(sent_shares);
        checkMapping(received_shares);
        received_shares = sent_shares;
    }

    void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t> &received_shares, PartyID id,
                        size_t _size) {
        checkMapping(sent_shares);
        checkMapping(received_shares);
        received_shares = sent_shares;
    }

    void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t> &received_shares, PartyID id,
                        size_t _size) {
        checkMapping(sent_shares);
        checkMapping(received_shares);
        received_shares = sent_shares;
    }

    void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t> &received_shares,
                        PartyID id, size_t _size) {
        checkMapping(sent_shares);
        checkMapping(received_shares);
        received_shares = sent_shares;
    }

    void sendShares(const std::vector<Vector<int8_t>> &shares, std::vector<PartyID> partyID) {
        for (int i = 0; i < shares.size(); i++) {
            checkMapping(shares[i]);
        }
    }

    void sendShares(const std::vector<Vector<int16_t>> &shares, std::vector<PartyID> partyID) {
        for (int i = 0; i < shares.size(); i++) {
            checkMapping(shares[i]);
        }
    }

    void sendShares(const std::vector<Vector<int32_t>> &shares, std::vector<PartyID> partyID) {
        for (int i = 0; i < shares.size(); i++) {
            checkMapping(shares[i]);
        }
    }

    void sendShares(const std::vector<Vector<int64_t>> &shares, std::vector<PartyID> partyID) {
        for (int i = 0; i < shares.size(); i++) {
            checkMapping(shares[i]);
        }
    }

    void sendShares(const std::vector<Vector<__int128_t>> &shares, std::vector<PartyID> partyID) {
        for (int i = 0; i < shares.size(); i++) {
            checkMapping(shares[i]);
        }
    }

    void receiveBroadcast(std::vector<Vector<int8_t>> &shares, std::vector<PartyID> partyID) {}
    void receiveBroadcast(std::vector<Vector<int16_t>> &shares, std::vector<PartyID> partyID) {}
    void receiveBroadcast(std::vector<Vector<int32_t>> &shares, std::vector<PartyID> partyID) {}
    void receiveBroadcast(std::vector<Vector<int64_t>> &shares, std::vector<PartyID> partyID) {}
    void receiveBroadcast(std::vector<Vector<__int128_t>> &shares, std::vector<PartyID> partyID) {}

    void exchangeShares(const std::vector<Vector<int8_t>> &shares,
                        std::vector<Vector<int8_t>> &received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        for (int i = 0; i < shares.size(); i++) {
            checkMapping(shares[i]);
            checkMapping(received_shares[i]);
            received_shares[i] = shares[i];
        }
    }

    void exchangeShares(const std::vector<Vector<int16_t>> &shares,
                        std::vector<Vector<int16_t>> &received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        for (int i = 0; i < shares.size(); i++) {
            checkMapping(shares[i]);
            checkMapping(received_shares[i]);
            received_shares[i] = shares[i];
        }
    }

    void exchangeShares(const std::vector<Vector<int32_t>> &shares,
                        std::vector<Vector<int32_t>> &received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        for (int i = 0; i < shares.size(); i++) {
            checkMapping(shares[i]);
            checkMapping(received_shares[i]);
            received_shares[i] = shares[i];
        }
    }

    void exchangeShares(const std::vector<Vector<int64_t>> &shares,
                        std::vector<Vector<int64_t>> &received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        for (int i = 0; i < shares.size(); i++) {
            checkMapping(shares[i]);
            checkMapping(received_shares[i]);
            received_shares[i] = shares[i];
        }
    }

    void exchangeShares(const std::vector<Vector<__int128_t>> &shares,
                        std::vector<Vector<__int128_t>> &received_shares,
                        std::vector<PartyID> to_id, std::vector<PartyID> from_id) {
        for (int i = 0; i < shares.size(); i++) {
            checkMapping(shares[i]);
            checkMapping(received_shares[i]);
            received_shares[i] = shares[i];
        }
    }
};

class NullCommunicatorFactory : public CommunicatorFactory<NullCommunicatorFactory> {
   public:
    NullCommunicatorFactory(int argc, char **argv, int numParties, int threadsNum)
        : CommunicatorFactory<NullCommunicatorFactory>() {}

    std::unique_ptr<Communicator> create() { return std::make_unique<NullCommunicator>(); }

    int getPartyId() const { return 0; }

    int getNumParties() const { return 1; }

    void blockingReady() {}
};

}  // namespace secrecy

#endif  // SECRECY_NULL_COMMUNICATOR_H
