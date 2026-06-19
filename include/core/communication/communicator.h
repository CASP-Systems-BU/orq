#pragma once

#include <NTL/GF2E.h>

#include <span>

#include "backend/common/setting.h"
#include "core/containers/e_vector.h"
#include "core/math/util.h"
#include "debug/orq_debug.h"

namespace orq {
typedef int PartyID;

// GF2E serialization helpers are namespace-level utilities shared across components
// (e.g. Communicator and Hash), not member functions.
inline const Vector<uint8_t> serializeGF2E(const std::span<const NTL::GF2E> s) {
    int nbytes = math::gf2e_num_bytes();

    Vector<uint8_t> out(s.size() * nbytes);

    for (size_t i = 0; i < s.size(); i++) {
        math::serializeGF2E(s[i], &out[i * nbytes]);
    }
    return out;
}

inline const Vector<uint8_t> serializeGF2E(const Vector<NTL::GF2E>& x) {
    return serializeGF2E(x.span());
}

inline const Vector<NTL::GF2E> deserializeGF2E(const uint8_t* buf, size_t elms) {
    int nbytes = math::gf2e_num_bytes();

    Vector<NTL::GF2E> out(elms);
    for (size_t i = 0; i < elms; i++) {
        out[i] = math::deserializeGF2E(&buf[i * nbytes]);
    }
    return out;
}

/**
 * @brief The base communicator class. All communicators must inherit from this
 * class.
 *
 */
class Communicator {
   protected:
    /**
     * @brief pairwise latency of this communicator, milliseconds
     *
     */
    double latency;

    /**
     * @brief pairwise bandwidth of this communicator, Gbps
     *
     */
    double bandwidth;

    /**
     * @brief The current party's absolute index in the party ring.
     *
     */
    PartyID currentId;

    size_t bytes_sent = 0;
    size_t num_rounds;

    orq::service::Setting setting_;

   public:
    /**
     * Initializes the communicator base with the current party index.
     * @param _currentId The absolute index of the party in the parties ring.
     */
    Communicator(PartyID _currentId, std::string host_prefix = "localhost", double latency = 0,
                 double bandwidth = std::numeric_limits<double>::infinity(),
                 orq::service::Setting setting = orq::service::Setting::SAME)
        : currentId(_currentId),
          host_prefix(host_prefix),
          latency(latency),
          bandwidth(bandwidth),
          setting_(setting) {}

    virtual ~Communicator() {}

    size_t getBytesSent() const { return bytes_sent; }

    size_t getRounds() const { return num_rounds; }

    std::string host_prefix;

    /**
     * @brief Get the communicator's latency in milliseconds. This implementation returns a static
     * value, but future implementations may directly measure the value in the caller.
     *
     * @return double
     */
    double getLatency() { return latency; }

    /**
     * @brief Get the communicator's bandwdith in Gbps. This implementation returns a static
     * value, but future implementations may directly measure the value in the caller.
     *
     * @return double
     */
    double getBandwidth() { return bandwidth; }

    /**
     * @brief Get the network setting (same/lan/wan) for this execution.
     *
     * @return orq::service::Setting
     */
    orq::service::Setting getSetting() const { return setting_; }

    /////////////////////////////////
    /// Peer to Peer Communication //
    /////////////////////////////////

    /**
     * Send one data element to a chosen party.
     * @param element The data element to be sent to the party.
     * @param id The index of the recipient party relative to the current party.
     */
    virtual void sendShare(int8_t element, PartyID id) = 0;
    virtual void sendShare(int16_t element, PartyID id) = 0;
    virtual void sendShare(int32_t element, PartyID id) = 0;
    virtual void sendShare(int64_t element, PartyID id) = 0;
    virtual void sendShare(NTL::GF2E& element, PartyID id) {
        Vector<uint8_t> send_data(math::gf2e_num_bytes());
        math::serializeGF2E(element, send_data.data());
        sendShares(send_data, id);
    };

    void sendShare(uint8_t element, PartyID id) {
        sendShare(*reinterpret_cast<int8_t*>(&element), id);
    }
    void sendShare(uint16_t element, PartyID id) {
        sendShare(*reinterpret_cast<int16_t*>(&element), id);
    }
    void sendShare(uint32_t element, PartyID id) {
        sendShare(*reinterpret_cast<int32_t*>(&element), id);
    }
    void sendShare(uint64_t element, PartyID id) {
        sendShare(*reinterpret_cast<int64_t*>(&element), id);
    }

    /**
     * Send a vector to a chosen party.
     * @param shares The data elements to be sent to the party.
     * @param id The index of the recipient party relative to the current party.
     * @param size Number of data elements to be sent.
     */
    virtual void sendShares(const Vector<int8_t>& shares, PartyID id) = 0;
    virtual void sendShares(const Vector<int16_t>& shares, PartyID id) = 0;
    virtual void sendShares(const Vector<int32_t>& shares, PartyID id) = 0;
    virtual void sendShares(const Vector<int64_t>& shares, PartyID id) = 0;
    virtual void sendShares(const Vector<__int128_t>& shares, PartyID id) = 0;

    void sendShares(const Vector<uint8_t>& shares, PartyID id) {
        sendShares(*reinterpret_cast<const Vector<int8_t>*>(&shares), id);
    }
    void sendShares(const Vector<uint16_t>& shares, PartyID id) {
        sendShares(*reinterpret_cast<const Vector<int16_t>*>(&shares), id);
    }
    void sendShares(const Vector<uint32_t>& shares, PartyID id) {
        sendShares(*reinterpret_cast<const Vector<int32_t>*>(&shares), id);
    }
    void sendShares(const Vector<uint64_t>& shares, PartyID id) {
        sendShares(*reinterpret_cast<const Vector<int64_t>*>(&shares), id);
    }
    void sendShares(const Vector<__uint128_t>& shares, PartyID id) {
        sendShares(*reinterpret_cast<const Vector<__int128_t>*>(&shares), id);
    }

    void sendShares(const Vector<NTL::GF2E>& shares, PartyID id) {
        sendShares(serializeGF2E(shares), id);
    }

    /**
     * Receive an element from the chosen party. blocking call.
     * @param element reference to variable to receive into
     * @param id The index of the sending party relative to the current party.
     */
    virtual void receiveShare(int8_t& element, PartyID id) = 0;
    virtual void receiveShare(int16_t& element, PartyID id) = 0;
    virtual void receiveShare(int32_t& element, PartyID id) = 0;
    virtual void receiveShare(int64_t& element, PartyID id) = 0;
    virtual void receiveShare(NTL::GF2E& element, PartyID id) {
        Vector<uint8_t> recv_data(math::gf2e_num_bytes());
        receiveShares(recv_data, id);
        element = math::deserializeGF2E(recv_data.data());
    };

    void receiveShare(uint8_t& element, PartyID id) {
        receiveShare(*reinterpret_cast<int8_t*>(&element), id);
    }
    void receiveShare(uint16_t& element, PartyID id) {
        receiveShare(*reinterpret_cast<int16_t*>(&element), id);
    }
    void receiveShare(uint32_t& element, PartyID id) {
        receiveShare(*reinterpret_cast<int32_t*>(&element), id);
    }
    void receiveShare(uint64_t& element, PartyID id) {
        receiveShare(*reinterpret_cast<int64_t*>(&element), id);
    }

    /**
     * Receive a vector from some chosen party. blocking call.
     * @param shares reference to vector to receive into
     * @param id The index of the sending party relative to the current party.
     * @param size Number of data elements to be received.
     */
    virtual void receiveShares(Vector<int8_t>& shares, PartyID id) = 0;
    virtual void receiveShares(Vector<int16_t>& shares, PartyID id) = 0;
    virtual void receiveShares(Vector<int32_t>& shares, PartyID id) = 0;
    virtual void receiveShares(Vector<int64_t>& shares, PartyID id) = 0;
    virtual void receiveShares(Vector<__int128_t>& shares, PartyID id) = 0;

    void receiveShares(Vector<uint8_t>& shares, PartyID id) {
        receiveShares(*reinterpret_cast<Vector<int8_t>*>(&shares), id);
    }
    void receiveShares(Vector<uint16_t>& shares, PartyID id) {
        receiveShares(*reinterpret_cast<Vector<int16_t>*>(&shares), id);
    }
    void receiveShares(Vector<uint32_t>& shares, PartyID id) {
        receiveShares(*reinterpret_cast<Vector<int32_t>*>(&shares), id);
    }
    void receiveShares(Vector<uint64_t>& shares, PartyID id) {
        receiveShares(*reinterpret_cast<Vector<int64_t>*>(&shares), id);
    }
    void receiveShares(Vector<__uint128_t>& shares, PartyID id) {
        receiveShares(*reinterpret_cast<Vector<__int128_t>*>(&shares), id);
    }

    void receiveShares(Vector<NTL::GF2E>& shares, PartyID id) {
        size_t elms = shares.size();
        size_t buf_size = elms * math::gf2e_num_bytes();
        Vector<uint8_t> buffer(buf_size);
        receiveShares(buffer, id);
        shares = deserializeGF2E(&buffer[0], elms);
    }

    /**
     * Sends and Receives vectors to and from some same party. Subclasses may
     * implement in an arbitrary order, but neither send nor receive should
     * block each other. exchangeShares should block until both calls are
     * completed. Implementations may choose to run send and receive
     * concurrently, or queue the send while waiting on a blocking receive.
     *
     * @param sent_shares vector to send
     * @param received_shares vector to receive into
     * @param id The index of the other party relative to the current party.
     * @param size Number of data elements to be sent and received.
     */
    virtual void exchangeShares(Vector<int8_t> sent_shares, Vector<int8_t>& received_shares,
                                PartyID id) = 0;
    virtual void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t>& received_shares,
                                PartyID id) = 0;
    virtual void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t>& received_shares,
                                PartyID id) = 0;
    virtual void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t>& received_shares,
                                PartyID id) = 0;
    virtual void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t>& received_shares,
                                PartyID id) = 0;

    void exchangeShares(Vector<uint8_t> sent_shares, Vector<uint8_t>& received_shares, PartyID id) {
        exchangeShares(*reinterpret_cast<Vector<int8_t>*>(&sent_shares),
                       *reinterpret_cast<Vector<int8_t>*>(&received_shares), id);
    }
    void exchangeShares(Vector<uint16_t> sent_shares, Vector<uint16_t>& received_shares,
                        PartyID id) {
        exchangeShares(*reinterpret_cast<Vector<int16_t>*>(&sent_shares),
                       *reinterpret_cast<Vector<int16_t>*>(&received_shares), id);
    }
    void exchangeShares(Vector<uint32_t> sent_shares, Vector<uint32_t>& received_shares,
                        PartyID id) {
        exchangeShares(*reinterpret_cast<Vector<int32_t>*>(&sent_shares),
                       *reinterpret_cast<Vector<int32_t>*>(&received_shares), id);
    }
    void exchangeShares(Vector<uint64_t> sent_shares, Vector<uint64_t>& received_shares,
                        PartyID id) {
        exchangeShares(*reinterpret_cast<Vector<int64_t>*>(&sent_shares),
                       *reinterpret_cast<Vector<int64_t>*>(&received_shares), id);
    }
    void exchangeShares(Vector<__uint128_t> sent_shares, Vector<__uint128_t>& received_shares,
                        PartyID id) {
        exchangeShares(*reinterpret_cast<Vector<__int128_t>*>(&sent_shares),
                       *reinterpret_cast<Vector<__int128_t>*>(&received_shares), id);
    }

    void exchangeShares(Vector<NTL::GF2E>& send_shares, Vector<NTL::GF2E>& recv_shares,
                        PartyID id) {
        size_t elms = recv_shares.size();
        size_t buf_size = elms * math::gf2e_num_bytes();
        Vector<uint8_t> recv_buffer(buf_size);
        exchangeShares(serializeGF2E(send_shares), recv_buffer, id);
        recv_shares = deserializeGF2E(&recv_buffer[0], elms);
    }

    /**
     * @brief Exchange shares with two different parties.
     *
     * @param sent_shares
     * @param received_shares
     * @param to_id relative ID of destination party
     * @param from_id relative ID of source party
     * @param size
     */
    virtual void exchangeShares(Vector<int8_t> sent_shares, Vector<int8_t>& received_shares,
                                PartyID to_id, PartyID from_id) = 0;
    virtual void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t>& received_shares,
                                PartyID to_id, PartyID from_id) = 0;
    virtual void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t>& received_shares,
                                PartyID to_id, PartyID from_id) = 0;
    virtual void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t>& received_shares,
                                PartyID to_id, PartyID from_id) = 0;
    virtual void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t>& received_shares,
                                PartyID to_id, PartyID from_id) = 0;

    void exchangeShares(Vector<uint8_t> sent_shares, Vector<uint8_t>& received_shares,
                        PartyID to_id, PartyID from_id) {
        exchangeShares(*reinterpret_cast<Vector<int8_t>*>(&sent_shares),
                       *reinterpret_cast<Vector<int8_t>*>(&received_shares), to_id, from_id);
    }
    void exchangeShares(Vector<uint16_t> sent_shares, Vector<uint16_t>& received_shares,
                        PartyID to_id, PartyID from_id) {
        exchangeShares(*reinterpret_cast<Vector<int16_t>*>(&sent_shares),
                       *reinterpret_cast<Vector<int16_t>*>(&received_shares), to_id, from_id);
    }
    void exchangeShares(Vector<uint32_t> sent_shares, Vector<uint32_t>& received_shares,
                        PartyID to_id, PartyID from_id) {
        exchangeShares(*reinterpret_cast<Vector<int32_t>*>(&sent_shares),
                       *reinterpret_cast<Vector<int32_t>*>(&received_shares), to_id, from_id);
    }
    void exchangeShares(Vector<uint64_t> sent_shares, Vector<uint64_t>& received_shares,
                        PartyID to_id, PartyID from_id) {
        exchangeShares(*reinterpret_cast<Vector<int64_t>*>(&sent_shares),
                       *reinterpret_cast<Vector<int64_t>*>(&received_shares), to_id, from_id);
    }
    void exchangeShares(Vector<__uint128_t> sent_shares, Vector<__uint128_t>& received_shares,
                        PartyID to_id, PartyID from_id) {
        exchangeShares(*reinterpret_cast<Vector<__int128_t>*>(&sent_shares),
                       *reinterpret_cast<Vector<__int128_t>*>(&received_shares), to_id, from_id);
    }

    void exchangeShares(const Vector<NTL::GF2E>& send_shares, Vector<NTL::GF2E>& recv_shares,
                        PartyID to_id, PartyID from_id) {
        size_t elms = recv_shares.size();
        size_t buf_size = elms * math::gf2e_num_bytes();
        Vector<uint8_t> recv_buffer(buf_size);
        exchangeShares(serializeGF2E(send_shares), recv_buffer, to_id, from_id);
        recv_shares = deserializeGF2E(&recv_buffer[0], elms);
    }

    /**
     * Send multiple Vectors to multiple parties at the same time. Vector sizes
     * must match.
     * @param shares The std::vector of the vectors to be sent.
     * @param partyID The std::vector of the parties to send to.
     */
    virtual void sendShares(const std::vector<Vector<int8_t>>& shares,
                            std::vector<PartyID> partyID) = 0;
    virtual void sendShares(const std::vector<Vector<int16_t>>& shares,
                            std::vector<PartyID> partyID) = 0;
    virtual void sendShares(const std::vector<Vector<int32_t>>& shares,
                            std::vector<PartyID> partyID) = 0;
    virtual void sendShares(const std::vector<Vector<int64_t>>& shares,
                            std::vector<PartyID> partyID) = 0;
    virtual void sendShares(const std::vector<Vector<__int128_t>>& shares,
                            std::vector<PartyID> partyID) = 0;

    void sendShares(const std::vector<Vector<uint8_t>>& shares, std::vector<PartyID> partyID) {
        sendShares(*reinterpret_cast<const std::vector<Vector<int8_t>>*>(&shares), partyID);
    }
    void sendShares(const std::vector<Vector<uint16_t>>& shares, std::vector<PartyID> partyID) {
        sendShares(*reinterpret_cast<const std::vector<Vector<int16_t>>*>(&shares), partyID);
    }
    void sendShares(const std::vector<Vector<uint32_t>>& shares, std::vector<PartyID> partyID) {
        sendShares(*reinterpret_cast<const std::vector<Vector<int32_t>>*>(&shares), partyID);
    }
    void sendShares(const std::vector<Vector<uint64_t>>& shares, std::vector<PartyID> partyID) {
        sendShares(*reinterpret_cast<const std::vector<Vector<int64_t>>*>(&shares), partyID);
    }
    void sendShares(const std::vector<Vector<__uint128_t>>& shares, std::vector<PartyID> partyID) {
        sendShares(*reinterpret_cast<const std::vector<Vector<__int128_t>>*>(&shares), partyID);
    }

    /**
     * @brief Receive from multiple parties. Vector sizes must match.
     *
     * @param shares vector of Vectors to be sent
     * @param partyID
     */
    virtual void receiveBroadcast(std::vector<Vector<int8_t>>& shares,
                                  std::vector<PartyID> partyID) = 0;
    virtual void receiveBroadcast(std::vector<Vector<int16_t>>& shares,
                                  std::vector<PartyID> partyID) = 0;
    virtual void receiveBroadcast(std::vector<Vector<int32_t>>& shares,
                                  std::vector<PartyID> partyID) = 0;
    virtual void receiveBroadcast(std::vector<Vector<int64_t>>& shares,
                                  std::vector<PartyID> partyID) = 0;
    virtual void receiveBroadcast(std::vector<Vector<__int128_t>>& shares,
                                  std::vector<PartyID> partyID) = 0;

    void receiveBroadcast(std::vector<Vector<uint8_t>>& shares, std::vector<PartyID> partyID) {
        receiveBroadcast(*reinterpret_cast<std::vector<Vector<int8_t>>*>(&shares), partyID);
    }
    void receiveBroadcast(std::vector<Vector<uint16_t>>& shares, std::vector<PartyID> partyID) {
        receiveBroadcast(*reinterpret_cast<std::vector<Vector<int16_t>>*>(&shares), partyID);
    }
    void receiveBroadcast(std::vector<Vector<uint32_t>>& shares, std::vector<PartyID> partyID) {
        receiveBroadcast(*reinterpret_cast<std::vector<Vector<int32_t>>*>(&shares), partyID);
    }
    void receiveBroadcast(std::vector<Vector<uint64_t>>& shares, std::vector<PartyID> partyID) {
        receiveBroadcast(*reinterpret_cast<std::vector<Vector<int64_t>>*>(&shares), partyID);
    }
    void receiveBroadcast(std::vector<Vector<__uint128_t>>& shares, std::vector<PartyID> partyID) {
        receiveBroadcast(*reinterpret_cast<std::vector<Vector<__int128_t>>*>(&shares), partyID);
    }

    void receiveBroadcast(std::vector<Vector<NTL::GF2E>>& shares, std::vector<PartyID> id) {
        int nbytes = math::gf2e_num_bytes();
        std::vector<Vector<uint8_t>> buffers;
        for (size_t i = 0; i < shares.size(); i++) {
            buffers.push_back(Vector<uint8_t>(shares[i].size() * nbytes));
        }
        receiveBroadcast(buffers, id);
        for (size_t i = 0; i < shares.size(); i++) {
            shares[i] = deserializeGF2E(&buffers[i][0], shares[i].size());
        }
    }

    /**
     * Send and receive multiple Vectors to and from multiple parties.

     * @param shares The std::vector of the vectors to be sent.
     * @param received_shares The std::vector of the vectors to be received.
     * @param to_id The std::vector of the parties to send `shares` to.
     * @param from_id The std::vector of the parties to receive `received_shares` from.
     */
    virtual void exchangeShares(const std::vector<Vector<int8_t>>& shares,
                                std::vector<Vector<int8_t>>& received_shares,
                                std::vector<PartyID> to_id, std::vector<PartyID> from_id) = 0;
    virtual void exchangeShares(const std::vector<Vector<int16_t>>& shares,
                                std::vector<Vector<int16_t>>& received_shares,
                                std::vector<PartyID> to_id, std::vector<PartyID> from_id) = 0;
    virtual void exchangeShares(const std::vector<Vector<int32_t>>& shares,
                                std::vector<Vector<int32_t>>& received_shares,
                                std::vector<PartyID> to_id, std::vector<PartyID> from_id) = 0;
    virtual void exchangeShares(const std::vector<Vector<int64_t>>& shares,
                                std::vector<Vector<int64_t>>& received_shares,
                                std::vector<PartyID> to_id, std::vector<PartyID> from_id) = 0;
    virtual void exchangeShares(const std::vector<Vector<__int128_t>>& shares,
                                std::vector<Vector<__int128_t>>& received_shares,
                                std::vector<PartyID> to_id, std::vector<PartyID> from_id) = 0;

    void exchangeShares(const std::vector<Vector<uint8_t>>& shares,
                        std::vector<Vector<uint8_t>>& received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        exchangeShares(*reinterpret_cast<const std::vector<Vector<int8_t>>*>(&shares),
                       *reinterpret_cast<std::vector<Vector<int8_t>>*>(&received_shares), to_id,
                       from_id);
    }
    void exchangeShares(const std::vector<Vector<uint16_t>>& shares,
                        std::vector<Vector<uint16_t>>& received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        exchangeShares(*reinterpret_cast<const std::vector<Vector<int16_t>>*>(&shares),
                       *reinterpret_cast<std::vector<Vector<int16_t>>*>(&received_shares), to_id,
                       from_id);
    }
    void exchangeShares(const std::vector<Vector<uint32_t>>& shares,
                        std::vector<Vector<uint32_t>>& received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        exchangeShares(*reinterpret_cast<const std::vector<Vector<int32_t>>*>(&shares),
                       *reinterpret_cast<std::vector<Vector<int32_t>>*>(&received_shares), to_id,
                       from_id);
    }
    void exchangeShares(const std::vector<Vector<uint64_t>>& shares,
                        std::vector<Vector<uint64_t>>& received_shares, std::vector<PartyID> to_id,
                        std::vector<PartyID> from_id) {
        exchangeShares(*reinterpret_cast<const std::vector<Vector<int64_t>>*>(&shares),
                       *reinterpret_cast<std::vector<Vector<int64_t>>*>(&received_shares), to_id,
                       from_id);
    }
    void exchangeShares(const std::vector<Vector<__uint128_t>>& shares,
                        std::vector<Vector<__uint128_t>>& received_shares,
                        std::vector<PartyID> to_id, std::vector<PartyID> from_id) {
        exchangeShares(*reinterpret_cast<const std::vector<Vector<__int128_t>>*>(&shares),
                       *reinterpret_cast<std::vector<Vector<__int128_t>>*>(&received_shares), to_id,
                       from_id);
    }
};

}  // namespace orq