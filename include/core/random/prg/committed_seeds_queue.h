/**
 * @file committed_seeds_queue.h
 *
 */
#pragma once

#include <math.h>
#include <sodium.h>
#include <stdlib.h>

#include <algorithm>
#include <array>
#include <iomanip>
#include <ostream>
#include <queue>
#include <vector>

#include "backend/common/hash.h"
#include "common_prg.h"
#include "prg_algorithm.h"

namespace orq::random {

int excluded_from_set(std::set<int> s) {
    return (0 + 1 + 2 + 3) - std::accumulate(s.begin(), s.end(), 0);
}

enum CommittedSeedsQueueMode { SECURE, INSECURE_TEST_MODE };

// The seed type
constexpr size_t SEED_BYTES = crypto_aead_aes256gcm_KEYBYTES;
using Seed = std::array<uint8_t, SEED_BYTES>;

// This is the type used for commitments
// It is the result of a hash function
constexpr size_t COM_BYTES = crypto_generichash_BYTES;
using Commitment = std::array<uint8_t, COM_BYTES>;

// Key type for hashing commitments
constexpr size_t KEY_BYTES = crypto_generichash_KEYBYTES;
using Key = std::array<uint8_t, KEY_BYTES>;

// Overload so we can print commitments in hex format
inline std::ostream& operator<<(std::ostream& os, const Commitment& commitment) {
    auto flags = os.flags();
    auto fill = os.fill();

    os << std::hex << std::setfill('0');
    for (auto b : commitment) {
        os << std::setw(2) << static_cast<int>(b & 0xFF);
    }

    os.flags(flags);
    os.fill(fill);
    return os;
}

/**
 * @brief Manages a queue of committed seeds.
 *
 * This class implements a commitment protocol for fresh seed generation, preventing malicious
 * parties from pre-computing random values. Seeds are committed in batches to reduce communication
 * overhead.
 *
 * Commitments are broadcast to all other parties, but openings may be selective (e.g., constructing
 * a shared PRG seed held by only a subset of parties). In this case, opening parties can send
 * special semaphore values to parties who will not receive an opening, so they know the delete the
 * commitment from their queue.
 *
 * By default, runs in SECURE mode. For testing, pass the template argument <INSECURE_TEST_MODE>.
 */
template <CommittedSeedsQueueMode Mode = SECURE>
class CommittedSeedsQueue {
   private:
    // These static asserts guarantee that we can operate on seeds, commitments, and keys directly
    // in-memory. For example, if any of these types were changed to std::vector, or were arrays of
    // non-primitive types, the assertions would fail.
    static_assert(std::is_trivially_copyable_v<Seed>, "Seed type is not trivially copyable");
    static_assert(std::is_trivially_copyable_v<Commitment>,
                  "Commitment type is not trivially copyable");
    static_assert(std::is_trivially_copyable_v<Key>, "Key type is not trivially copyable");

    // In test mode, set instead of calling abort(). Mutable so const methods can set it.
    // But in real executions, this is not used
    mutable bool verificationFailed_ = false;

    // Whether to print the number of commitments remaining on CSQ destruction. Useful for debugging
    // resource usage.
    constexpr static bool PRINT_REMAINING = false;

    // Number of seeds to commit in one batch
    // TODO: figure out best value here. Depends on # parties + # opens
    constexpr static size_t DEFAULT_CSQ_BATCH_SIZE = 1 << 15;

    /**
     * @brief Returns true (and sets `verificationFailed_` in test mode) or calls abort() in secure
     * mode.
     *
     * @param msg
     */
    void reportError(const std::string& msg) const {
        std::cerr << msg << "\n";
        if constexpr (Mode == INSECURE_TEST_MODE) {
            verificationFailed_ = true;
        } else {
            abort();
        }
    }

    // Queue of commitments sent to us from other parties
    std::vector<std::deque<Commitment>> commitments;

    // My committed values
    std::deque<Key> localKeys;
    std::deque<Seed> localSeeds;

    // Communication and randomness infrastructure
    Communicator* communicator;
    std::shared_ptr<random::CommonPRG> localPRG;

    // Protocol parameters
    PartyID me;
    PartyID numParties;

    std::vector<PartyID> exchangeIDs;

    /**
     * @brief Pack a vector of commitments into a Vector of bytes
     *
     * @param in
     * @return Vector<uint8_t>
     */
    static Vector<uint8_t> packCommitments(const std::vector<Commitment>& in) {
        Vector<uint8_t> out(in.size() * COM_BYTES);
        if (!in.empty()) {
            std::memcpy(&out[0], in.data(), in.size() * COM_BYTES);
        }
        return out;
    }

    /**
     * @brief Unpack a Vector of bytes into a vector of Commitments
     *
     * @param in
     * @return std::vector<Commitment>
     */
    static std::vector<Commitment> unpackCommitments(const Vector<uint8_t>& in) {
        if (in.size() % COM_BYTES != 0 || in.size() == 0) {
            throw std::logic_error("invalid commitment byte payload size");
        }

        std::vector<Commitment> out(in.size() / COM_BYTES);
        if (!out.empty()) {
            std::memcpy(out.data(), &in[0], in.size());
        }
        return out;
    }

    /**
     * @brief Hash all values in a deque<Commitment>. This hash can then be exchanged to confirm
     * consistency. We do not use keyed hashing in this case.
     *
     * @param commitments
     * @return Commitment
     */
    static Commitment hashCommitments(const std::deque<Commitment>& commitments) {
        // Hash all commitments together using a generic hash
        std::vector<uint8_t> hash_input;
        for (const auto& com : commitments) {
            if (com.empty()) {
                continue;
            }

            hash_input.insert(hash_input.end(), com.begin(), com.end());
        }

        return Hash::generateHashArray(std::span(hash_input));
    }

    /**
     * @brief Create a hash of each deque of Commitments we have for every other party. Skip ourself
     * (just pack it as all zeros)
     *
     * @return std::vector<Commitment>
     */
    std::vector<Commitment> createOrderedCommitmentHashes() const {
        // Hash commitments in canonical party order
        std::vector<Commitment> ordered_hashes(numParties);

        for (size_t p = 0; p < numParties; p++) {
            if (p == me) {
                // skip my own commitments
                continue;
            }

            if (commitments[p].empty()) {
                reportError("ERROR: P" + std::to_string(me) +
                            " missing commitment queue for party " + std::to_string(p) +
                            " during preprocessing hash check");
                return {};
            }

            ordered_hashes[p] = hashCommitments(commitments[p]);
        }

        return ordered_hashes;
    }

    /**
     * @brief Given my locally computed hashes and a set of received hashes, confirm consistency of
     * the commitment queues.
     *
     * @param local_hashes hashes I computed locally from my own queues
     * @param received_hashes hashes of commitments received from other parties
     */
    void verifyCommitmentHashes(const std::vector<Commitment>& local_hashes,
                                const std::vector<std::vector<Commitment>>& received_hashes) {
        for (int i = 0; i < exchangeIDs.size(); i++) {
            int send_pid = (exchangeIDs[i] + me) % numParties;

            assert(send_pid != me);

            auto r = received_hashes[i];

            if (r.size() != numParties) {
                reportError("ERROR: P" + std::to_string(me) +
                            " received malformed hash payload from party " +
                            std::to_string(send_pid) + ": len " + std::to_string(r.size()));
                return;
            }

            for (size_t j = 0; j < numParties; j++) {
                // I won't check my own hash, nor will I check the hash of the sender
                if (j == me || j == send_pid) {
                    continue;
                }

                // These are Commitments, aka std::array, which provides a != overload.
                if (r[j] != local_hashes[j]) {
                    reportError("ERROR: P" + std::to_string(me) + " commitment hash vector from P" +
                                std::to_string(send_pid) + " mismatch for P" + std::to_string(j));
                }
            }
        }
    }

   public:
    /**
     * @brief Constructor for CommittedSeedsQueue.
     *
     * @param _communicator Reference to the communicator for network operations.
     * @param _localPRG Pointer to local PRG for generating seeds and commitments.
     * @param _partyID The ID of this party.
     * @param _numParties Total number of parties in the protocol.
     */
    CommittedSeedsQueue(Communicator* _communicator, std::shared_ptr<random::CommonPRG> _localPRG,
                        PartyID _partyID, PartyID _numParties)
        : communicator(_communicator), localPRG(_localPRG), me(_partyID), numParties(_numParties) {
        // exchange with all other parties...
        for (PartyID i = 1; i < numParties; i++) {
            exchangeIDs.push_back(i);
        }

        commitments.resize(numParties);
    }

    CommittedSeedsQueue(const CommittedSeedsQueue&) = delete;
    CommittedSeedsQueue& operator=(const CommittedSeedsQueue&) = delete;
    CommittedSeedsQueue(CommittedSeedsQueue&&) = delete;
    CommittedSeedsQueue& operator=(CommittedSeedsQueue&&) = delete;

    ~CommittedSeedsQueue() {
        if (PRINT_REMAINING && me == 0) {
            std::cout << "Note: " << commitmentsAvailable()
                      << " commitments left in PRG seed queue\n ";
        }
    }

    /**
     * @brief Repopulate the queue with new committed seeds.
     *
     * Generates a batch of fresh seeds, commits to them, and exchanges
     * commitments with all other parties in a single communication round.
     *
     * @param batch_size how many committed seeds to generate
     */
    void repopulateQueue(size_t batch_size = DEFAULT_CSQ_BATCH_SIZE) {
        std::vector<Key> key_v(batch_size);
        std::vector<Seed> seed_v(batch_size);
        std::vector<Commitment> com_v(batch_size);

        //////////////////////////////////////////////////
        // Step 1: Generate local seeds and commitments
        //////////////////////////////////////////////////

        // keys can just be random bytes
        localPRG->getNext(key_v);

        for (int i = 0; i < batch_size; i++) {
            // Seeds will be used as AES inputs
            // This ends up calling down to randombytes, but explicitly calling keygen is better for
            // the case of a general PRF.
            orq::random::AESPRGAlgorithm::aesKeyGen(seed_v[i]);

            // Create commitment using hash. com = H_key(seed)
            auto h = Hash(key_v[i]);
            h.update(std::span(seed_v[i]));
            com_v[i] = h.finalizeArray();
        }

        //////////////////////////////////////////////////
        // Step 2: Exchange commitments with all parties
        //////////////////////////////////////////////////
        // We can only exchange orq::Vector. Pack my commitments:
        auto sendPayload = packCommitments(com_v);

        // Allocate space to receive
        std::vector<Vector<uint8_t>> recvPayload;
        for (auto _ : exchangeIDs) {
            recvPayload.push_back(Vector<uint8_t>(sendPayload.size()));
        }

        // TODO: nocopy getting stuck here
        // Just send multiple identical copies of sendPayload (references, no extra alloc)
        communicator->exchangeShares(
            std::vector<decltype(sendPayload)>(exchangeIDs.size(), sendPayload), recvPayload,
            exchangeIDs, exchangeIDs);

        //////////////////////////////////////////////////
        // Step 3: Unpack received commitments from other parties & store
        //////////////////////////////////////////////////
        for (int i = 0; i < exchangeIDs.size(); i++) {
            PartyID p = (me + exchangeIDs[i]) % numParties;
            auto remote = unpackCommitments(recvPayload[i]);

            // Get this party's commitment queue
            // reference (auto&) is important: otherwise we insert into a copy of the deque.
            auto& comQ = commitments[p];

            comQ.insert(comQ.end(), remote.begin(), remote.end());
        }

        // Add these keys and seeds to the queues
        localKeys.insert(localKeys.end(), key_v.begin(), key_v.end());
        localSeeds.insert(localSeeds.end(), seed_v.begin(), seed_v.end());

        ////////////////////////////////////////////////////////
        // Step 4: Verify commitment consistency via hashes
        ////////////////////////////////////////////////////////
        // Compute hashes of all commitments we received in canonical party order.
        auto local_hashes = createOrderedCommitmentHashes();

        // Pack our hashes for transmission
        auto packed_hashes = packCommitments(local_hashes);

        // Exchange hashes with all parties
        std::vector<Vector<uint8_t>> received_packed_hashes;
        for (auto _ : exchangeIDs) {
            received_packed_hashes.push_back(Vector<uint8_t>(packed_hashes.size()));
        }

        communicator->exchangeShares(
            std::vector<decltype(packed_hashes)>(exchangeIDs.size(), packed_hashes),
            received_packed_hashes, exchangeIDs, exchangeIDs);

        // Unpack and verify received hashes.
        std::vector<std::vector<Commitment>> received_hashes;
        for (const auto& packed : received_packed_hashes) {
            received_hashes.push_back(unpackCommitments(packed));
        }

        verifyCommitmentHashes(local_hashes, received_hashes);

        // Sanity check
        assert(commitmentsAvailable() > 0);
    }

    /**
     * @brief Get a verified common PRG for everyone
     *
     * @return std::shared_ptr<CommonPRG>
     */
    std::shared_ptr<CommonPRG> nextPRG() {
        std::set<PartyID> everyone;
        for (int i = 0; i < numParties; i++) {
            everyone.insert(i);
        }

        return nextPRGforGroup(everyone);
    }

    /**
     * @brief Get a verified common PRG for the group: open the next committed seed, check for
     * correctness, and generate a joint PRG seed. Any party not in the group will get a null
     * pointer.
     *
     * @param group
     * @return std::shared_ptr<CommonPRG>
     */
    std::shared_ptr<CommonPRG> nextPRGforGroup(std::set<PartyID> group) {
        if (!group.contains(me)) {
            // No PRG for me. Throw away my copy of everyone else's commitment
            for (auto p : group) {
                auto& q = commitments[p];
                if (q.empty()) {
                    std::cout << "WARNING: non-participating P" << me
                              << "'s CommittedSeedsQueue for party " << p << " is empty!\n";
                } else {
                    q.pop_front();
                }
            }

            return nullptr;
        }

        // Everyone else still running is in the group. Exchange keys and seeds, and open
        // commitments.

        ////////////////////////////////////////////////////////
        // Step 1: Compute exchange parties and their relative indices
        ////////////////////////////////////////////////////////
        std::vector<PartyID> groupExchange;
        for (auto g : group) {
            if (g != me) {
                groupExchange.push_back(g - me);
            }
        }

        ////////////////////////////////////////////////////////
        // Step 2: Exchange seeds and keys with all parties in group
        ////////////////////////////////////////////////////////

        if (localSeeds.empty() || localKeys.empty()) {
            reportError("ERROR: P" + std::to_string(me) + " local queue is empty!");
            return nullptr;
        }

        // Pack seeds and keys together.
        auto mySeed = localSeeds.front();
        localSeeds.pop_front();

        auto myKey = localKeys.front();
        localKeys.pop_front();

        Vector<uint8_t> openingPayload(SEED_BYTES + KEY_BYTES);
        std::memcpy(openingPayload.data(), mySeed.data(), SEED_BYTES);
        std::memcpy(openingPayload.data() + SEED_BYTES, myKey.data(), KEY_BYTES);

        std::vector<Vector<uint8_t>> openingsToSend(groupExchange.size(), openingPayload);
        std::vector<Vector<uint8_t>> receivedOpenings;
        for (auto _ : groupExchange) {
            receivedOpenings.push_back(Vector<uint8_t>(SEED_BYTES + KEY_BYTES));
        }

        communicator->exchangeShares(openingsToSend, receivedOpenings, groupExchange,
                                     groupExchange);

        ////////////////////////////////////////////////////////
        // Step 3: Verify commitments for all received seeds
        ////////////////////////////////////////////////////////
        for (int i = 0; i < groupExchange.size(); ++i) {
            PartyID p = (groupExchange[i] + me) % numParties;

            // Extract seed and key from received payload
            Seed rseed;
            Key rkey;
            std::memcpy(rseed.data(), receivedOpenings[i].data(), SEED_BYTES);
            std::memcpy(rkey.data(), receivedOpenings[i].data() + SEED_BYTES, KEY_BYTES);

            // Compute expected commitment
            auto h = Hash(rkey);
            h.update(std::span(rseed));
            Commitment expectedCom = h.finalizeArray();

            // Check against the first commitment in the queue for this party
            if (commitments[p].empty()) {
                reportError("ERROR: P" + std::to_string(me) + "'s CommittedSeedsQueue for party " +
                            std::to_string(p) + " is empty!");
                return nullptr;
            }

            if (expectedCom != commitments[p].front()) {
                reportError("ERROR: P" + std::to_string(me) +
                            " commitment verification failed for seed from party " +
                            std::to_string(p) + "! Group: " + debug::container2str(group));
                return nullptr;
            }

            // Remove the verified commitment from the queue
            commitments[p].pop_front();

            // As we verify committed party seeds, construct the joint seed
            for (auto i = 0; i < SEED_BYTES; i++) {
                mySeed[i] ^= rseed[i];
            }
        }

        // Return a new CommonPRG initialized with the seed
        // Everyone in the group agrees on this seed, because we already verified everyone agreed on
        // the commitments.
        std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm =
            std::make_unique<AESPRGAlgorithm>(mySeed);
        return std::make_shared<CommonPRG>(std::move(prg_algorithm), me);
    }

    bool verificationHasFailed() const { return verificationFailed_; }

    /**
     * @brief Flip bits in the front commitment stored for party p.
     *
     * Only available in test mode — will not compile if called on CommittedSeedsQueue<SECURE>.
     *
     */
    void corruptCommitmentFrom(PartyID p)
        requires(Mode == INSECURE_TEST_MODE)
    {
        if (!commitments[p].empty()) {
            commitments[p].front()[0] ^= 0xFF;
        }
    }

    /**
     * @brief Return the number of commitments left in the queue. This is the minimum over all
     * parties (i.e., represents an upper bound on the number of seeds we could still open for
     * arbitrary randomness groups)
     *
     * NOTE: depending on group asymmetries, parties may see different return values from this
     * method. Do NOT use this method as a heuristic to decide when to repopulate: parties might
     * decide differently and deadlock. Instead, repopulate at fixed points, or use communication
     * synchronization to agree on repopulation.
     *
     * @return size_t
     */
    size_t commitmentsAvailable() {
        std::optional<size_t> min_el;
        for (PartyID p = 0; p < numParties; p++) {
            if (p == me) {
                continue;
            }

            auto s = commitments[p].size();
            min_el = std::min(s, min_el.value_or(s));
        }

        return *min_el;
    }
};

}  // namespace orq::random