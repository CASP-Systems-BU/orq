#ifndef SECRECY_COMMUNICATOR_H
#define SECRECY_COMMUNICATOR_H

#include "../../debug/debug.h"
#include "../containers/e_vector.h"

namespace secrecy {
typedef int PartyID;

class Communicator {
   protected:
    PartyID currentId;  // Current Party's absolute index in the parties ring.

    size_t bytes_sent = 0;

    // NOTE: inheriting classes should implement:
    //  - It is advised to wrap communication for each party
    //  in an object inside std::vector.
    //  - Functions to add configuration to new parties.
    //  - Functions to edit current parties configurations.
    //  - Functions that take indices as a relative index
    //  in a ring from the current party.
    //  - Try to use minimum memory while receiving or sending.
    // TODO:
    //  - Create functions to batch data from same round together for threads.
    //      - StartRound    -   AddToRound  -   FinishRound

   public:
    /**
     * Communicator - Initializes the communicator base with the current party index.
     * @param _currentId - The absolute index of the party in the parties ring.
     */
    Communicator(PartyID _currentId) : currentId(_currentId) {}

    virtual ~Communicator() {}

    size_t getBytesSent() const { return bytes_sent; }

    /////////////////////////////////
    /// Peer to Peer Communication //
    /////////////////////////////////

    /**
     * sendShares - Send one data element to a chosen party.
     * @param share - The data element to be sent to the party.
     * @param id - The index of the recipient party relative to the current party.
     */
    virtual void sendShare(int8_t element, PartyID id) = 0;
    virtual void sendShare(int16_t element, PartyID id) = 0;
    virtual void sendShare(int32_t element, PartyID id) = 0;
    virtual void sendShare(int64_t element, PartyID id) = 0;

    /**
     * sendShares - Send many data elements to a chosen party.
     * @param shares - The data elements to be sent to the party.
     * @param id - The index of the recipient party relative to the current party.
     * @param size - Number of data elements to be sent.
     */
    virtual void sendShares(const Vector<int8_t>& shares, PartyID id, size_t size) = 0;
    virtual void sendShares(const Vector<int16_t>& shares, PartyID id, size_t size) = 0;
    virtual void sendShares(const Vector<int32_t>& shares, PartyID id, size_t size) = 0;
    virtual void sendShares(const Vector<int64_t>& shares, PartyID id, size_t size) = 0;
    virtual void sendShares(const Vector<__int128_t>& shares, PartyID id, size_t size) = 0;

    /**
     * receiveShare - Receive data element from some chosen party. blocking call.
     * @param id - The index of the sending party relative to the current party.
     * @return The data element received from the sending party.
     */
    virtual void receiveShare(int8_t& element, PartyID id) = 0;
    virtual void receiveShare(int16_t& element, PartyID id) = 0;
    virtual void receiveShare(int32_t& element, PartyID id) = 0;
    virtual void receiveShare(int64_t& element, PartyID id) = 0;

    /**
     * receiveShares - Receive many data elements from some chosen party. blocking call.
     * @param id - The index of the sending party relative to the current party.
     * @param size - Number of data elements to be received.
     * @return The data elements received from the sending party.
     */
    virtual void receiveShares(Vector<int8_t>& shares, PartyID id, size_t size) = 0;
    virtual void receiveShares(Vector<int16_t>& shares, PartyID id, size_t size) = 0;
    virtual void receiveShares(Vector<int32_t>& shares, PartyID id, size_t size) = 0;
    virtual void receiveShares(Vector<int64_t>& shares, PartyID id, size_t size) = 0;
    virtual void receiveShares(Vector<__int128_t>& shares, PartyID id, size_t size) = 0;

    /**
     * exchangeShares - Sends and Receives many data elements to and from some same party.
     * Calls sendShares and receiveShare in different threads. Function is blocking till
     * both calls return.
     * @param shares - The data elements to be sent to the party.
     * @param id - The index of the other party relative to the current party.
     * @param size - Number of data elements to be sent and received.
     * @return The data elements received from the sending party.
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

    /////////////////////////////////
    ///// Broadcasting functions ////
    /////////////////////////////////
    // TODO: useful signatures.
    // virtual void sendShares(T vector, std::vector<PartyID> ids) = 0;
    // virtual void sendShares(std::vector<T> vector, std::vector<PartyID> ids) = 0;
    // virtual ShareVector receiveMultipleShares(std::vector<PartyID> ids) = 0;
    // virtual ShareVector exchangeMultipleShares(std::vector<T> vector, std::vector<PartyID> ids) =
    // 0;

    /**
     * sendShares - Send multiple Vectors to multiple parties at the same time.
     * @param shares - The std::vector of the vectors to be sent.
     * @param partyID - The std::vector of the parties to send to.
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

    // receive from multiple parties
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
     * exchangeShares - Send and receive multiple Vectors to and from multiple parties.
     *  Note: the to_id and from_id vectors can have different numbers of parties.
     * @param shares - The std::vector of the vectors to be sent.
     * @param received_shares - The std::vector of the vectors to be received.
     * @param to_id - The std::vector of the parties to send `shares` to.
     * @param from_id - The std::vector of the parties to receive `received_shares` from.
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

}  // namespace secrecy
#endif  // SECRECY_COMMUNICATOR_H
