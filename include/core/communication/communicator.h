#pragma once

#include "core/containers/e_vector.h"
#include "debug/orq_debug.h"

namespace orq {
typedef int PartyID;

/**
 * @brief The base communicator class. All communicators must inherit from this
 * class.
 *
 */
class Communicator {
   protected:
    /**
     * @brief The current party's absolute index in the party ring.
     *
     */
    PartyID currentId;

    size_t bytes_sent = 0;

   public:
    /**
     * Initializes the communicator base with the current party index.
     * @param _currentId The absolute index of the party in the parties ring.
     */
    Communicator(PartyID _currentId) : currentId(_currentId) {}

    virtual ~Communicator() {}

    size_t getBytesSent() const { return bytes_sent; }

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

    /**
     * Send a vector to a chosen party.
     * @param shares The data elements to be sent to the party.
     * @param id The index of the recipient party relative to the current party.
     * @param size Number of data elements to be sent.
     */
    virtual void sendShares(const Vector<int8_t>& shares, PartyID id, size_t size) = 0;
    virtual void sendShares(const Vector<int16_t>& shares, PartyID id, size_t size) = 0;
    virtual void sendShares(const Vector<int32_t>& shares, PartyID id, size_t size) = 0;
    virtual void sendShares(const Vector<int64_t>& shares, PartyID id, size_t size) = 0;
    virtual void sendShares(const Vector<__int128_t>& shares, PartyID id, size_t size) = 0;

    /**
     * Receive an element from the chosen party. blocking call.
     * @param element reference to variable to receive into
     * @param id The index of the sending party relative to the current party.
     */
    virtual void receiveShare(int8_t& element, PartyID id) = 0;
    virtual void receiveShare(int16_t& element, PartyID id) = 0;
    virtual void receiveShare(int32_t& element, PartyID id) = 0;
    virtual void receiveShare(int64_t& element, PartyID id) = 0;

    /**
     * Receive a vector from some chosen party. blocking call.
     * @param shares reference to vector to receive into
     * @param id The index of the sending party relative to the current party.
     * @param size Number of data elements to be received.
     */
    virtual void receiveShares(Vector<int8_t>& shares, PartyID id, size_t size) = 0;
    virtual void receiveShares(Vector<int16_t>& shares, PartyID id, size_t size) = 0;
    virtual void receiveShares(Vector<int32_t>& shares, PartyID id, size_t size) = 0;
    virtual void receiveShares(Vector<int64_t>& shares, PartyID id, size_t size) = 0;
    virtual void receiveShares(Vector<__int128_t>& shares, PartyID id, size_t size) = 0;

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
                                PartyID id, size_t size) = 0;
    virtual void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t>& received_shares,
                                PartyID id, size_t size) = 0;
    virtual void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t>& received_shares,
                                PartyID id, size_t size) = 0;
    virtual void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t>& received_shares,
                                PartyID id, size_t size) = 0;
    virtual void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t>& received_shares,
                                PartyID id, size_t size) = 0;

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
                                PartyID to_id, PartyID from_id, size_t size) = 0;
    virtual void exchangeShares(Vector<int16_t> sent_shares, Vector<int16_t>& received_shares,
                                PartyID to_id, PartyID from_id, size_t size) = 0;
    virtual void exchangeShares(Vector<int32_t> sent_shares, Vector<int32_t>& received_shares,
                                PartyID to_id, PartyID from_id, size_t size) = 0;
    virtual void exchangeShares(Vector<int64_t> sent_shares, Vector<int64_t>& received_shares,
                                PartyID to_id, PartyID from_id, size_t size) = 0;
    virtual void exchangeShares(Vector<__int128_t> sent_shares, Vector<__int128_t>& received_shares,
                                PartyID to_id, PartyID from_id, size_t size) = 0;

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
};

}  // namespace orq
