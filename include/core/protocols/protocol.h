#pragma once

#include <numeric>
#include <set>
#include <vector>

#include "core/communication/communicator.h"
#include "core/random/manager.h"
#include "debug/orq_debug.h"
#include "profiling/stopwatch.h"

// Temporary workaround until we make protocol classes templated.
#define TYPE_BITS_int8_t 8
#define TYPE_BITS_int16_t 16
#define TYPE_BITS_int32_t 32
#define TYPE_BITS_int64_t 64
#define TYPE_BITS___int128_t 128

#define TYPE_BITS(T) TYPE_BITS_##T
#define GLUE_(a, b) a##b
#define GLUE(a, b) GLUE_(a, b)

// map PROTO_OBJ_NAME(int32_t) => proto_32 for Worker member lookup
// Eventually, this should be something like worker.proto<int32_t>
#define PROTO_OBJ_NAME(T) GLUE(proto_, TYPE_BITS(T))

using namespace orq::benchmarking;

namespace orq {

namespace service {
    class Worker;
}

/**
 * @brief Actions for resharing operations.
 */
enum class ReshareAction { Send, Receive };

/**
 * @brief Assignment for resharing operations.
 *
 * Defines whether a party sends or receives shares during resharing,
 * along with the target parties and share indices.
 */
struct ReshareAssignment {
    // whether this party sends (in group) or receives (not in group)
    ReshareAction action;

    // Who to send to, or receive from
    std::vector<int> ranks;

    // if sending, which shares to send
    std::vector<int> shareIdx;
};

#ifdef MAL_TEST_MODE
#warning Running in malicious test mode - will not provide malicious security!
#endif

/**
 * @brief Base class for all secure multi-party computation protocols.
 */
// The protocol base
class ProtocolBase {
    // party id -> shares held by that party
    std::vector<std::vector<int>> partyShareMap = {};
    // share id -> parties holding that share
    std::vector<std::vector<int>> sharePartyMap = {};

    /**
     * @brief Defines the mapping from rank to the secret shares held by that rank.
     *
     * Index i in the vector is the set of shares held by rank i.
     * For a replication k, party i holds shares [i, i+1, ..., i+k-1].
     * This function should be called during construction.
     */
    void generatePartyShareMappings() {
        partyShareMap.clear();
        for (int i = 0; i < numParties; i++) {
            std::vector<int> party_shares;
            for (int j = 0; j < replicationNumber; j++) {
                party_shares.push_back((i + j) % numParties);
            }
            partyShareMap.push_back(party_shares);
        }
    }

    /**
     * @brief Generate a mapping from shares to parties holding that share.
     *
     * Called during construction to populate the mapping.
     */
    void generateSharePartyMappings() {
        sharePartyMap.clear();
        sharePartyMap.resize(numParties);
        auto psm = getPartyShareMappings();

        for (int p = 0; p < numParties; p++) {
            for (auto s : psm[p]) {
                sharePartyMap[s].push_back(p);
            }
        }

        for (auto v : sharePartyMap) {
            std::sort(v.begin(), v.end());
        }
    }

    /**
     * @brief Generate send assignment for resharing shares to other parties.
     *
     * @param p Party identifier.
     * @param group Set of parties in the resharing group.
     * @return ReshareAssignment for sending shares.
     */
    auto generate_send_assignment(int p, std::set<int> group) {
        ReshareAssignment ra;
        ra.action = ReshareAction::Send;

        auto s2p_map = getSharePartyMappings();
        auto p2s_map = getPartyShareMappings();

        auto my_shares = p2s_map[p];
        std::vector<bool> is_canonical;

        std::vector<int> ranks, share_idx;

        for (int si = 0; si < replicationNumber; si++) {
            auto s = my_shares[si];
            auto s_parties = s2p_map[s];

            bool canonical = true;

            // Check other parties with this share (sorted list)
            for (auto q : s_parties) {
                // Are they in the group?
                if (!group.contains(q)) {
                    continue;
                }

                // Someone else in group has lower rank.
                if (q != p) {
                    canonical = false;
                }

                break;
            }

            is_canonical.push_back(canonical);
        }

        // Check all parties not in the group and see what shares they want
        for (int q = 0; q < numParties; q++) {
            if (group.contains(q)) {
                continue;
            }

            auto q_shares = p2s_map[q];
            for (auto qs : q_shares) {
                bool should_send = false;
                int si = 0;

                // Do I have this share & am I the canonical holder?
                for (; si < replicationNumber; si++) {
                    if (my_shares[si] == qs && is_canonical[si]) {
                        should_send = true;
                        break;
                    }
                }

                if (should_send) {
                    ranks.push_back(q - p);
                    share_idx.push_back(si);
                }
            }
        }

        ra.ranks = ranks;
        ra.shareIdx = share_idx;

        return ra;
    }

    /**
     * @brief Generate receive assignment for resharing shares from other parties.
     *
     * @param p Party identifier.
     * @param group Set of parties in the resharing group.
     * @return ReshareAssignment for receiving shares.
     */
    auto generate_recv_assignment(int p, std::set<int> group) {
        ReshareAssignment ra;
        ra.action = ReshareAction::Receive;

        auto s2p_map = getSharePartyMappings();
        auto p2s_map = getPartyShareMappings();

        auto my_shares = p2s_map[p];

        // *relative* ranks
        std::vector<int> send_parties;

        for (auto s : my_shares) {
            // Receive from the lowest party in the group
            auto s_parties = s2p_map[s];

            for (auto q : s_parties) {
                if (group.contains(q)) {
                    send_parties.push_back(q - p);
                    break;
                }
            }
        }

        ra.ranks = send_parties;
        return ra;
    }

    /**
     * @brief Internal code to check for malicious behavior. Semihonest protocols should not
     * implement. Malicious protocols should implement their checks here.
     *
     * @return true no malicious behavior occurred
     * @return false malicious behavior occurred
     */
    virtual bool start_malicious_check_internal() { return true; }

    virtual bool finalize_malicious_check_internal() { return true; }

    /**
     * @brief Reset the internal malicious-detection state
     *
     */
    virtual void reset_malicious_state() {}

    // This allows us to call reset_malicious_state, which is a private method
    friend class service::Worker;

   protected:
    // The unique id of the party that created the Protocol instance
    PartyID partyID;
    // The total number of computing parties participating in the protocol execution
    const int numParties;
    // The replication factor
    const int replicationNumber;
    // The number of allowed corruptions
    // const int corruptionsNumber;
    // The number of parties required to reconstruct the shares.
    // const int reconstructNumber;

    /**
     * @brief Map from party-groups to resharing assignment.
     *
     * `reshare` will retrieve a list of assignments from this map given
     * the current group, and then each party will execute its respective
     * instructions.
     *
     */
    std::map<std::set<int>, ReshareAssignment> reshareMap = {};

   public:
    size_t send_calls = 0;
    size_t recv_calls = 0;

    /**
     * @brief Constructor for ProtocolBase.
     *
     * @param pID Party identifier for this protocol instance.
     * @param partiesNum Total number of parties in the protocol.
     * @param replicationNum Replication factor for the protocol.
     */
    ProtocolBase(PartyID pID, int partiesNum, int replicationNum)
        : partyID(pID), numParties(partiesNum), replicationNumber(replicationNum) {
        assert(pID < partiesNum);

        generatePartyShareMappings();
        generateSharePartyMappings();

        // Iterate through all groups and generate resharing assignments.
        for (auto g : getGroups()) {
            if (g.contains(partyID)) {
                reshareMap[g] = generate_send_assignment(partyID, g);
            } else {
                reshareMap[g] = generate_recv_assignment(partyID, g);
            }
        }
    };

    /**
     * Generates all combinations of a given size of an input size
     * (recursively).
     * @param set the set of ints to find combinations of.
     * @param partial_set the partially complete combination at any given
     * point in the recursion.
     * @param size the size of the combinations to search for, decremented
     * with each recursive call.
     * @param combinations a vector of combinations to be filled throughout
     * the algorithm.
     */
    static void generateAllCombinations(std::set<int> set, std::set<int> partial_set, int size,
                                        std::vector<std::set<int>>& combinations) {
        // base case
        if (size == 0) {
            combinations.push_back(partial_set);
            return;
        }
        // recursive case
        for (int element : set) {
            std::set<int> new_set(set);
            // remove all elements in new_set that are less than the current
            // element
            for (int other_element : set) {
                if (other_element < element) {
                    new_set.erase(other_element);
                }
            }
            new_set.erase(element);
            // add current element to the partial set and recurse
            std::set<int> new_partial_set(partial_set);
            new_partial_set.insert(element);
            generateAllCombinations(new_set, new_partial_set, size - 1, combinations);
        }
    }

    /**
     * @brief Defines the groups for the protocol.
     *
     * This is a STATIC function to be callable during setup before protocol objects exist.
     *
     * @param num_parties The number of parties in the protocol.
     * @param parties_to_reconstruct Number of parties needed to reconstruct.
     * @param num_adversaries Number of adversarial parties.
     * @return The vector of groups.
     */
    static std::vector<std::set<int>> generateGroups(int num_parties, int parties_to_reconstruct,
                                                     int num_adversaries) {
        if (num_parties == 1) {
            // special handling.
            return {{0}};
        }

        int group_size = num_parties - num_adversaries;

        // Don't return singleton groups.
        if (group_size == 1) {
            return {};
        }

        std::set<int> parties;
        for (int i = 0; i < num_parties; i++) {
            parties.insert(i);
        }
        std::set<int> partial_combination;
        std::vector<std::set<int>> groups;
        generateAllCombinations(parties, partial_combination, group_size, groups);

        return groups;
    }

    /**
     * @brief Get the groups for shuffling operations.
     *
     * @return Vector of party groups for shuffling.
     */
    virtual std::vector<std::set<int>> getGroups() const {
        return generateGroups(numParties, 2, 1);
    }

    /**
     * @brief Generate randomness groups (includes the group of everyone).
     *
     * @param n Number of parties.
     * @param n_reconstruct Number of parties needed for reconstruction.
     * @param n_adversary Number of adversarial parties.
     * @return Vector of randomness groups.
     */
    static std::vector<std::set<int>> generateRandomnessGroups(int n, int n_reconstruct,
                                                               int n_adversary) {
        // Get the regular groups
        auto g = generateGroups(n, n_reconstruct, n_adversary);

        // Add the set of everyone
        std::vector<int> _everyone(n);
        std::iota(_everyone.begin(), _everyone.end(), 0);

        g.push_back(std::set<int>(_everyone.begin(), _everyone.end()));

        return g;
    }

    /**
     * @brief Get a mapping from parties to share numbers
     *
     * @return std::vector<std::vector<int>>
     */
    std::vector<std::vector<int>> getPartyShareMappings() const {
        assert(partyShareMap.size() > 0);
        return partyShareMap;
    }

    /**
     * @brief Get a mapping from shares to parties holding that share.
     *
     * @return Vector mapping share IDs to party lists.
     */
    std::vector<std::vector<int>> getSharePartyMappings() const {
        assert(sharePartyMap.size() > 0);
        return sharePartyMap;
    }

    /**
     * @brief Get the party ID for this protocol instance.
     *
     * @return Party identifier.
     */
    int getPartyID() const { return partyID; }

    /**
     * @brief Get the replication number for this protocol.
     *
     * @return Replication factor.
     */
    const int getRepNumber() const { return replicationNumber; }

    /**
     * @brief Get the total number of parties in this protocol.
     *
     * @return Number of parties.
     */
    const int getNumParties() const { return numParties; }

    /**
     * @brief Print statistics for this protocol. The default is to do
     * nothing, but protocols may choose to override this function to
     * print operator counts, network statistics, or other information.
     *
     */
    virtual void print_statistics() {}

    /**
     * @brief Mark statistics. This can be used to take relative
     * measurements of a section of code.
     *
     */
    virtual void mark_statistics() {}

    /**
     * @brief Clear all accumulated statistics (counters, marks, etc.).
     *
     */
    virtual void clear_statistics() {}

    /**
     * @brief Start a check for malicious behavior. Aborts if malicious behavior occurred.
     *
     * In MAL_TEST_MODE, does not abort, and resets internal state. MAL_TEST_MODE should
     * only be used for explicitly testing malicious behavior, and never on benchmarks or real
     * queries.
     */
    virtual bool start_malicious_check() {
        auto ok = start_malicious_check_internal();

#ifndef MAL_TEST_MODE
        // Normal programs will abort on a failed test.
        if (!ok) {
            abort();
        }
#endif

        return ok;
    }

    /**
     * @brief Complete a check for malicious behavior. By contract, may assume that all threads have
     * run the corresponding start_*() method before any finalize_*() method runs.
     *
     * In MAL_TEST_MODE, does not abort, and resets the internal state.
     *
     * @return true
     * @return false
     */
    virtual bool finalize_malicious_check() {
        auto ok = finalize_malicious_check_internal();

#ifndef MAL_TEST_MODE
        // Normal programs will abort on a failed test.
        if (!ok) {
            abort();
        }
#endif

        return ok;
    }
};

/**
 * @brief Abstract class defining primitive methods for secure protocols.
 *
 * This is the abstract class that defines the primitive methods each secure
 * protocol must implement.
 *
 * @tparam Data Plaintext data type.
 * @tparam Share Share type (e.g., a 32-bit integer, a pair of 64-bit integers, etc.).
 * @tparam Vector Data container type.
 * @tparam EVector Share container type.
 *
 * Primitive operations are grouped as follows:
 *  1. Arithmetic operations on arithmetic shares.
 *  2. Boolean operations on boolean shares.
 *  3. Primitives for sending and receiving shares.
 *  4. Primitives for constructing and opening shares to learners.
 */
template <typename Data, typename Share, typename Vector, typename EVector>
class Protocol : public ProtocolBase {
   public:
    // The communicator
    Communicator* communicator;
    // The randomness manager, in place of the old random generator
    random::RandomnessManager* randomnessManager;

    /**
     * @brief Protocol constructor.
     *
     * @param _communicator A pointer to the communicator.
     * @param _randomnessManager A pointer to the randomness manager.
     * @param _partyID The (globally) unique identifier of the party that calls this constructor.
     * @param _numParties The total number of computing parties participating in the protocol.
     * @param _replicationNumber The protocol's replication factor.
     */
    Protocol(Communicator* _communicator, random::RandomnessManager* _randomnessManager,
             PartyID _partyID, int _numParties, int _replicationNumber)
        : ProtocolBase(_partyID, _numParties, _replicationNumber) {
        this->communicator = _communicator;
        this->randomnessManager = _randomnessManager;
    }
    /// Destructor
    virtual ~Protocol() {}

    /**
     * @brief Reshare shares with other parties.
     *
     * The group rerandomizes the vector v and sends shares to all parties that are not in the
     * group.
     *
     * Operates in place.
     *
     * TODO: fix runtime to support this better.
     *
     * @param v The EVector representing each party's view of the vector to be
     * @param group The group of parties that perform the resharing.
     * @param binary whether this is a binary (true) or arithmetic (false) resharing
     * reshared.
     */
    virtual void reshare(EVector& v, const std::set<int> group, bool binary) {
        auto ra = reshareMap[group];

        if (ra.action == ReshareAction::Send) {
            assert(ra.ranks.size() == ra.shareIdx.size());

            {
                // Scope rand for garbage collection
                std::vector<Vector> rand;
                for (int i = 0; i < numParties; i++) {
                    rand.push_back(Vector(v.size()));
                }

                // Randomize
                if (binary) {
                    this->randomnessManager->zeroSharingGenerator->groupGetNextBinary(rand, group);
                } else {
                    this->randomnessManager->zeroSharingGenerator->groupGetNextArithmetic(rand,
                                                                                          group);
                }

                auto my_shares = getPartyShareMappings()[partyID];
                // Generating too many random values here: each party only needs
                // RepNum random vectors, but is generating PartyNum
                for (int i = 0; i < replicationNumber; i++) {
                    if (binary) {
                        v(i) ^= rand[my_shares[i]];
                    } else {
                        v(i) += rand[my_shares[i]];
                    }
                }
            }

            for (int i = 0; i < ra.ranks.size(); i++) {
                this->communicator->sendShares(v(ra.shareIdx[i]), ra.ranks[i]);
            }
        } else if (ra.action == ReshareAction::Receive) {
            this->communicator->receiveBroadcast(v.contents, ra.ranks);
        } else {
            throw new std::runtime_error("Invalid reshare action.");
        }
    }

    virtual void handle_precision(const EVector& x, const EVector& y, EVector& z) {
        if (x.getPrecision() != y.getPrecision()) {
            throw std::runtime_error("Precision mismatch between multiplication inputs");
        }
        z.matchPrecision(x);
    }

    // **************************************** //
    //          Arithmetic operations           //
    // **************************************** //

    /**
     * @brief Defines vectorized arithmetic addition.
     *
     * @param x The first shared vector of size S.
     * @param y The second shared vector of size S.
     * @param z The output shared vector of size S.
     */
    virtual void add_a(const EVector& x, const EVector& y, EVector& z) { z = x + y; }

    /**
     * @brief Defines vectorized arithmetic subtraction.
     *
     * @param x The first shared vector of size S.
     * @param y The second shared vector of size S.
     * @param z The output shared vector of size S.
     */
    virtual void sub_a(const EVector& x, const EVector& y, EVector& z) { z = x - y; }

    /**
     * @brief Defines vectorized arithmetic multiplication.
     *
     * @param first The first shared vector of size S.
     * @param second The second shared vector of size S.
     * @param result The output shared vector of size S.
     */
    virtual void multiply_a(const EVector& first, const EVector& second, EVector& result) = 0;

    /**
     * @brief Defines vectorized arithmetic negation.
     *
     * @param in The input shared vector of size S.
     * @param out The output shared vector of size S.
     */
    virtual void neg_a(const EVector& in, EVector& out) { out = -in; }

    /**
     * @brief Defines vectorized arithmetic division by constant.
     *
     * @param input The input shared vector.
     * @param c The constant divisor.
     * @return Pair of shared vectors representing the division result.
     */
    virtual std::pair<EVector, EVector> div_const_a(const EVector& input, const Data c) = 0;

    /**
     * @brief Defines the vectorized dot product operation for consecutive elements.
     *
     * @param x The first shared vector of size S.
     * @param y The second shared vector of size S.
     * @param z The output shared vector.
     * @param aggSize The number of consecutive pairs of elements to aggregate. `0` means the full
     * vector.
     */
    virtual void dot_product_a(const EVector& x, const EVector& y, EVector& z,
                               const size_t aggSize = 0) {
        EVector res(x.size());
        multiply_a(x, y, res);
        z = res.chunkedSum(aggSize);
    }

    /**
     * Defines vectorized arithmetic truncation.
     * This method must take one input vector with arithmetic shares and return
     * a new vector that contains arithmetic shares of the truncated input.
     * The default implementation invokes the protocol's public division protocol.
     * @param x - The shared vector to truncate.
     */
    virtual void truncate(EVector& x) {
        if constexpr (!std::integral<Data>) {
            // Only integral types support fixed point (no NTL types)
            return;
        } else {
            int precision = x.getPrecision();
            if (precision == 0) {
                return;
            }

            // compute the public divisor
            Data divisor = 1 << precision;

            // run public division and discard the error
            // by discarding the error, we avoid incurring the a2b cost
            //   at the expense of one bit of error
            // ignoring error correction is thus a good default for truncation
            std::pair<EVector, EVector> ret = this->div_const_a(x, divisor);
            x = ret.first;

            // preserve precision
            x.setPrecision(precision);
        }
    }

    // **************************************** //
    //            Boolean operations            //
    // **************************************** //

    /**
     * @brief Defines vectorized bitwise XOR (^).
     *
     * @param x The first shared vector of size S.
     * @param y The second shared vector of size S.
     * @param z The output shared vector of size S.
     */
    virtual void xor_b(const EVector& x, const EVector& y, EVector& z) { z = x ^ y; }

    /**
     * @brief Defines vectorized bitwise AND (&).
     *
     * @param first The first shared vector of size S.
     * @param second The second shared vector of size S.
     * @param result The output shared vector of size S.
     */
    virtual void and_b(const EVector& first, const EVector& second, EVector& result) = 0;

    /**
     * @brief Defines vectorized boolean complement (~).
     *
     * @param in The input shared vector of size S.
     * @param out The output shared vector of size S.
     */
    virtual void not_b(const EVector& in, EVector& out) = 0;

    /**
     * @brief Defines vectorized boolean NOT (!).
     *
     * @param in The input shared vector of size S.
     * @param out The output shared vector of size S.
     */
    virtual void not_b_1(const EVector& in, EVector& out) = 0;

    /**
     * @brief Defines vectorized less-than-zero comparison.
     *
     * @param in The input shared vector of size S.
     * @param out The output shared vector of size S.
     */
    virtual void ltz(const EVector& in, EVector& out) { out = in.ltz(); }

    // **************************************** //
    //          Conversion operations           //
    // **************************************** //

    /**
     * @brief Defines vectorized boolean-to-arithmetic single bit conversion.
     *
     * @param in A B-shared vector of S single-bit elements.
     * @param out The output A-shared vector of size S.
     */
    virtual void b2a_bit(const EVector& in, EVector& out) = 0;

    /**
     * @brief Defines a redistribution of arithmetic secret shares into boolean secret shares.
     *
     * @param x The input arithmetic shared vector.
     * @return Pair of boolean shared vectors representing the redistribution.
     */
    virtual std::pair<EVector, EVector> redistribute_shares_b(const EVector& x) = 0;

    // **************************************** //
    //          Reconstruction operations       //
    // **************************************** //

    /**
     * @brief Defines how to reconstruct a single data value by adding its arithmetic shares.
     *
     * @param shares The input vector containing arithmetic shares of the secret value.
     * @return The plaintext value of type Data.
     *
     * NOTE: This method is useful when a computing party also acts as learner that receives
     * arithmetic shares from other parties and needs to reconstruct a true value.
     */
    virtual Data reconstruct_from_a(const std::vector<Share>& shares) = 0;

    /**
     * @brief Vectorized version of the reconstruct_from_a() method.
     *
     * @param shares A vector of shared vectors (each one of size n) that contain arithmetic
     shares.
     * @return A new vector that contains n plaintext values of type Data.
     *
     * NOTE: This method is useful when a computing party also acts as learner that receives
     * arithmetic shared vectors from other parties and needs to reconstruct the original vector.
     */
    virtual Vector reconstruct_from_a(const std::vector<EVector>& shares) = 0;

    /**
     * @brief Defines how to reconstruct a single data value by XORing its boolean shares.
     *
     * @param shares The input vector containing boolean shares of the secret value.
     * @return The plaintext value of type Data.
     *
     * NOTE: This method is useful when a computing party also acts as learner that receives
     * boolean shares from other parties and needs to reconstruct a true value.
     */
    virtual Data reconstruct_from_b(const std::vector<Share>& shares) = 0;

    /**
     * @brief Vectorized version of the reconstruct_from_b() method.
     *
     * @param shares A vector of shared vectors (each one of size n) that contain boolean shares.
     * @return A new vector that contains n plaintext values of type Data.
     *
     * NOTE: This method is useful when a computing party also acts as learner that receives
     * boolean shared vectors from other parties and needs to reconstruct the original vector.
     */
    virtual Vector reconstruct_from_b(const std::vector<EVector>& shares) = 0;

    // **************************************** //
    //            Opening operations            //
    // **************************************** //
    virtual Vector internal_open_a(const EVector& shares) = 0;
    virtual Vector internal_open_b(const EVector& shares) = 0;

    // **************************************** //
    //        Share generation operations       //
    // **************************************** //

    /**
     * @brief Compute secret shares for a vector of plaintext values.
     *
     * @param data A vector of input values of type Data.
     * @return A vector of shared vectors containing arithmetic shares.
     */
    virtual std::vector<EVector> get_shares_a(const Vector& data) = 0;

    /**
     * @brief Compute secret shares for a vector of plaintext values.
     *
     * @param data A vector of input values of type Data.
     * @return A vector of shared vectors containing boolean shares.
     */
    virtual std::vector<EVector> get_shares_b(const Vector& data) = 0;

    /**
     * @brief Compute secret shares for a vector of plaintext values.
     *
     * @param data The plaintext vector that must be secret-shared among computing parties.
     * @param data_party The party that owns the data.
     * @return The boolean shared vector
     *
     * NOTE: This method is useful for secret-sharing plaintext data in ORQ programs.
     */
    virtual EVector secret_share_b_internal(const Vector& data, const PartyID& data_party) = 0;

    /**
     * @brief Compute secret shares for a vector of plaintext values. Overloaded
     * secret_share_b_internal function that establishes a non-zero fixed-point precision.
     *
     * @param data The plaintext vector that must be secret-shared among computing parties.
     * @param data_party The party that owns the data.
     * @param fixed_point_precision precision of the input values
     * @return The boolean shared fixed-point vector
     */
    virtual EVector secret_share_b_internal(const Vector& data, const PartyID& data_party,
                                            const int& fixed_point_precision) {
        EVector ret = secret_share_b_internal(data, data_party);
        ret.setPrecision(fixed_point_precision);
        return ret;
    }

    /**
     * Defines how to A-share a plaintext vector according to a protocol.
     * @param data - The plaintext vector that must be secret-shared among
     * computing parties.
     * @param data_party data ownner
     * @return The arithmetic shared vector of the party that calls this method.
     *
     * NOTE: This method is useful for secret-sharing plaintext data in ORQ programs.
     */
    virtual EVector secret_share_a_internal(const Vector& data, const PartyID& data_party) = 0;

    /**
     * @brief Compute secret shares for a vector of plaintext values. Overloaded
     * secret_share_a_internal function that establishes a non-zero fixed-point precision.
     *
     * @param data The plaintext vector that must be secret-shared among computing parties.
     * @param data_party The party that owns the data.
     * @param fixed_point_precision precision of the input values
     * @return The arithmetic shared fixed-point vector
     */
    virtual EVector secret_share_a_internal(const Vector& data, const PartyID& data_party,
                                            const int& fixed_point_precision) {
        EVector ret = secret_share_a_internal(data, data_party);
        ret.setPrecision(fixed_point_precision);
        return ret;
    }

    /**
     * @brief Create a "secret" share of a public value, `x`. This
     * implemented by setting one share to `x` and all others to zero;
     * this gives a valid sharing under both arithmetic and boolean.
     *
     * @param x The public vector to share.
     * @param who_knows a party who knows the value
     * @return The shared vector.
     */
    virtual EVector public_share(const Vector& x, const std::set<PartyID>& who_knows) = 0;

    /**
     * @brief Convenience overload that defaults to an empty `who_knows` set.
     */
    virtual EVector public_share(const Vector& x) { return public_share(x, std::set<PartyID>{}); }

    /**
     * An overloaded public_share function that establishes a non-zero
     * fixed-point precision.
     * @param x
     * @param fixed_point_precision
     * @return EVector
     */
    virtual EVector public_share(const Vector& x, const std::set<PartyID>& who_knows,
                                 const int fixed_point_precision) {
        EVector ret = public_share(x, who_knows);
        ret.setPrecision(fixed_point_precision);
        return ret;
    }
};
}  // namespace orq
