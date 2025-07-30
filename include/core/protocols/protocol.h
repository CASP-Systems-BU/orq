#ifndef SECRECY_PROTOCOL_H
#define SECRECY_PROTOCOL_H

#include "../../benchmark/stopwatch.h"
#include "../../debug/debug.h"
#include "../communication/communicator.h"
#include "../random/manager.h"
using namespace secrecy::benchmarking;

#include <numeric>
#include <set>
#include <vector>

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

namespace secrecy {

enum class ReshareAction { Send, Receive };

struct ReshareAssignment {
    // whether this party sends (in group) or receives (not in group)
    ReshareAction action;

    // Who to send to, or receive from
    std::vector<int> ranks;

    // if sending, which shares to send
    std::vector<int> shareIdx;
};

// The protocol base
class ProtocolBase {
    // party id -> shares held by that party
    std::vector<std::vector<int>> partyShareMap = {};
    // share id -> parties holding that share
    std::vector<std::vector<int>> sharePartyMap = {};

    /**
     * Defines the mapping from rank to the secret shares held by that rank.
     * Index i in the vector is the set of shares held by rank i.
     * For a replication k, party i holds shares [i, i+1, ..., i+k-1].
     * This is function should be called during construction.
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
     * Called during construction to populated.
     *
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
     * @brief Reshare my shares to other parties
     *
     * @param p
     * @param group
     * @return auto
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
     * @brief Receive fresh shares from parties in the group
     *
     * @param p
     * @param group
     * @return auto
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

   protected:
    // The unique id of the party that created the Protocol instance
    PartyID partyID;
    // The total number of computing parties participating in the protocol
    // execution
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

    virtual ~ProtocolBase() {}

    /**
     * Generates all combinations of a given size of an input size
     * (recursively).
     * @param set - the set of ints to find combinations of.
     * @param partial_set - the partially complete combination at any given
     * point in the recursion.
     * @param size - the size of the combinations to search for, decremented
     * with each recursive call.
     * @param combinations - a vector of combinations to be filled throughout
     * the algorithm.
     */
    static void generateAllCombinations(std::set<int> set, std::set<int> partial_set, int size,
                                        std::vector<std::set<int>> &combinations) {
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
     * Defines the groups for the protocol.
     * This is a STATIC function to be callable during setup before protocol
     * objects exist.
     * @param num_parties - the number of parties in the protocol.
     * @param replication_number - the number of shares that each party holds.
     * @return The vector of groups.
     */
    static std::vector<std::set<int>> generateGroups(int num_parties, int parties_to_reconstruct,
                                                     int num_adversaries) {
        if (num_parties == 1) {
            // special handling.
            return {{0}};
        }

        if ((num_parties >= 4) && (num_adversaries == 1)) {
            // the one adversary optimization
            std::vector<std::set<int>> groups;
            for (int i = 0; i < 2; i++) {
                std::set<int> group;
                for (int j = 0; j < parties_to_reconstruct; j++) {
                    group.insert(i * parties_to_reconstruct + j);
                }
                groups.push_back(group);
            }
            return groups;
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

    // For shuffling.
    virtual std::vector<std::set<int>> getGroups() const {
        return generateGroups(numParties, 2, 1);
    }

    // Also includes the group of everyone.
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

    std::vector<std::vector<int>> getSharePartyMappings() const {
        assert(sharePartyMap.size() > 0);
        return sharePartyMap;
    }

    int getPartyID() const { return partyID; }

    const int getRepNumber() const { return replicationNumber; }

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

    virtual bool malicious_check(const bool should_abort = true) { return true; }
};
/**
 * This is the abstract class that defines the primitive methods each secure
 * protocol must implement.
 * @tparam Data - Plaintext data type.
 * @tparam Share - Share type (e.g., a 32-bit integer, a pair of 64-bit
 * integers, a 256-bit string, etc.).
 * @tparam Vector - Data container type.
 * @tparam EVector - Share container type.
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
    Communicator *communicator;
    // The randomness manager, in place of the old random generator
    random::RandomnessManager *randomnessManager;

    /**
     * Protocol constructor:
     * @param _communicator - A pointer to the communicator.
     * @param _randomnessManager - A pointer to the randomness manager.
     * @param _partyID - The (globally) unique identifier of the party that
     * calls this constructor.
     * @param _numParties - The total number of computing parties participating
     * in the protocol execution.
     * @param _replicationNumber - The protocol's replication factor.
     */
    Protocol(Communicator *_communicator, random::RandomnessManager *_randomnessManager,
             PartyID _partyID, const int &_numParties, const int &_replicationNumber)
        : ProtocolBase(_partyID, _numParties, _replicationNumber) {
        this->communicator = _communicator;
        this->randomnessManager = _randomnessManager;
    }
    /// Destructor
    virtual ~Protocol() {}

    /**
     * The group rerandomizes the vector v and sends shares to all parties
     * that are not in the group.
     *
     * Operates in place - TODO: fix runtime to support this better.
     *
     * @param group - The group of parties that perform the resharing.
     * @param v - The EVector representing each party's view of the vector to be
     * reshared.
     */
    virtual void reshare(EVector &v, const std::set<int> group, bool binary) {
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
                this->communicator->sendShares(v(ra.shareIdx[i]), ra.ranks[i], v.size());
            }
        } else if (ra.action == ReshareAction::Receive) {
            this->communicator->receiveBroadcast(v.contents, ra.ranks);
        } else {
            throw new std::runtime_error("Invalid reshare action.");
        }
    }

    // **************************************** //
    //          Arithmetic operations           //
    // **************************************** //

    /**
     * Defines vectorized arithmetic addition.
     * This method must take two input vectors with arithmetic shares and return
     * a new vector that contains arithmetic shares of the elementwise
     * additions.
     * @param first - The first shared vector of size S.
     * @param second - The second shared vector of size S.
     * @return A new shared vector v of size S such that v[i] = first[i] +
     * second[i], 0 <= i < S.
     */
    virtual void add_a(const EVector &x, const EVector &y, EVector &z) { z = x + y; }
    /**
     * Defines vectorized arithmetic subtraction.
     * This method must take two input vectors with arithmetic shares and return
     * a new vector that contains arithmetic shares of the elementwise
     * subtractions.
     * @param first - The first shared vector of size S.
     * @param second - The second shared vector of size S.
     * @return A new shared vector v of size S such that v[i] = first[i] -
     * second[i], 0 <= i < S.
     */
    virtual void sub_a(const EVector &x, const EVector &y, EVector &z) { z = x - y; }
    /**
     * Defines vectorized arithmetic multiplication.
     * This method must take two input vectors with arithmetic shares and return
     * a new vector that contains arithmetic shares of the elementwise
     * multiplications.
     * @param first - The first shared vector of size S.
     * @param second - The second shared vector of size S.
     * @return A new shared vector v of size S such that v[i] = first[i] *
     * second[i], 0 <= i < S.
     */
    virtual void multiply_a(const EVector &first, const EVector &second, EVector &result) = 0;
    /**
     * Defines vectorized arithmetic negation
     * This method must take one input vector with arithmetic shares and return
     * a new vector with all arithmetic shares negated.
     * @param input - The input shared vector of size S.
     * @return A new shared vector v of size S such that v[i] = -input[i],
     * 0<=i<S.
     */
    virtual void neg_a(const EVector &in, EVector &out) { out = -in; }

    virtual std::pair<EVector, EVector> div_const_a(const EVector &input, const Data &c) = 0;

    /**
     * Defines the vectorized dot product operation for each consecutive m pairs of elements in the
     * input vectors.
     * @param x - The first shared vector of size S.
     * @param y - The second shared vector of size S.
     * @param aggSize - The number of consecutive pairs of elements to aggregate.
     * @return A new shared vector v of size S such that
     * v[i] = x[i*m] * y[i*m] + x[i*m+1] * y[i*m+1] + ... + x[i*m+m-1] * y[i*m+m-1], 0 <= i <
     * ceil(S/m).
     */
    virtual void dot_product_a(const EVector &x, const EVector &y, EVector &z,
                               const size_t &aggSize) {
        EVector res(x.size());
        multiply_a(x, y, res);
        z = res.chunkedSum(aggSize);
    }

    // **************************************** //
    //            Boolean operations            //
    // **************************************** //

    /**
     * Defines vectorized bitwise XOR (^).
     * This method must take two input vectors with boolean shares and return a
     * new vector that contains boolean shares of the elementwise XORs.
     * @param first - The first shared vector of size S.
     * @param second - The second shared vector of size S.
     * @return A new shared vector v of size S such that v[i] = first[i] ^
     * second[i], 0 <= i < S.
     */
    virtual void xor_b(const EVector &x, const EVector &y, EVector &z) { z = x ^ y; }
    /**
     * Defines vectorized bitwise AND (&).
     * This method must take two input vectors with boolean shares and return a
     * new vector that contains boolean shares of the elementwise ANDs.
     * @param first - The first shared vector of size S.
     * @param second - The second shared vector of size S.
     * @return A new shared vector v of size S such that v[i] = first[i] &
     * second[i], 0 <= i < S.
     */
    virtual void and_b(const EVector &first, const EVector &second, EVector &result) = 0;
    /**
     * Defines vectorized boolean complement (~).
     * This method must take one input vector with boolean shares and return a
     * new vector with all boolean shares complemented.
     * @param input - The input shared vector of size S.
     * @return A new shared vector v of size S such that v[i] = ~input[i], 0 <=
     * i < S.
     */
    virtual void not_b(const EVector &in, EVector &out) = 0;
    /**
     * Defines vectorized boolean NOT (!).
     * This method must take one input vector with boolean shares and return a
     * new vector with all boolean shares negated.
     * @param input - The input shared vector of size S.
     * @return A new shared vector v of size S such that v[i] = !input[i], 0 <=
     * i < S.
     */
    virtual void not_b_1(const EVector &in, EVector &out) = 0;

    /**
     * Defines vectorized less-than-zero comparison.
     * This method must take one input vector with boolean shares and return a
     * new vector that contains boolean shares of the elementwise less-than-zero
     * comparisons.
     * @param input - The input shared vector of size S.
     * @return A new shared vector v of size S such that v[i] = 1 if input[i]<0,
     * otherwise v[i] = 0, 0 <= i < S.
     */
    virtual void ltz(const EVector &in, EVector &out) { out = in.ltz(); }

    // **************************************** //
    //          Conversion operations           //
    // **************************************** //

    /**
     * Defines vectorized boolean-to-arithmetic single bit conversion.
     * @param x - A B-shared vector of S single-bit elements.
     * @return A new A-shared vector v of size S such that v[i] = x[i], 0 <= i <
     * S.
     */
    virtual void b2a_bit(const EVector &in, EVector &out) = 0;

    /**
     * Defines a redistribution of arithmetic secret shares into boolean secret
     *shares.
     **/
    virtual std::pair<EVector, EVector> redistribute_shares_b(const EVector &x) = 0;

    // **************************************** //
    //          Reconstruction operations       //
    // **************************************** //

    /**
     * Defines how to reconstruct a single data value by adding its arithmetic
     * shares stored in the input vector.
     * @param shares - The input vector containing arithmetic shares of the
     * secret value.
     * @return The plaintext value of type Data.
     *
     * NOTE: This method is useful when a computing party also acts as learner
     * that receives arithmetic shares from other parties and needs to
     * reconstruct a true value.
     */
    virtual Data reconstruct_from_a(const std::vector<Share> &shares) = 0;
    /**
     * Vectorized version of the reconstruct_from_a() method.
     * This method defines how to reconstruct a vector of *n* data values
     * by adding their respective arithmetic shares in the input shared vectors.
     * @param shares - A vector of shared vectors (each one of size *n*) that
     * contain arithmetic shares.
     * @return A new vector that contains *n* plaintext values of type Data.
     *
     * NOTE: This method is useful when a computing party also acts as learner
     * that receives arithmetic shared vectors from other parties and needs to
     * reconstruct the original vector.
     */
    virtual Vector reconstruct_from_a(const std::vector<EVector> &shares) = 0;
    /**
     * Defines how to reconstruct a single data value by XORing its boolean
     * shares stored in the input vector.
     * @param shares - The input vector containing boolean shares of the secret
     * value.
     * @return The plaintext value of type Data.
     *
     * NOTE: This method is useful when a computing party also acts as learner
     * that receives boolean shares from other parties and needs to reconstruct
     * a true value.
     */
    virtual Data reconstruct_from_b(const std::vector<Share> &shares) = 0;
    /**
     * Vectorized version of the reconstruct_from_b() method.
     * This method defines how to reconstruct a vector of *n* data values
     * by XORing their respective boolean shares in the input shared vectors.
     * @param shares - A vector of shared vectors (each one of size *n*) that
     * contain boolean shares.
     * @return A new vector that contains *n* plaintext values of type Data.
     *
     * NOTE: This method is useful when a computing party also acts as learner
     * that receives boolean shared vectors from other parties and needs to
     * reconstruct the original vector.
     */
    virtual Vector reconstruct_from_b(const std::vector<EVector> &shares) = 0;

    // **************************************** //
    //            Opening operations            //
    // **************************************** //
    /**
     * @param shares - A shared vector that contains boolean shares of the
     * secret values.
     * @return A new vector that contains the plaintext values of type Data.
     *
     * NOTE: This method is useful when computing parties need to reveal
     * a secret-shared vector to each other.
     */
    virtual Vector open_shares_a(const EVector &shares) = 0;

    /**
     * @param shares - A shared vector that contains boolean shares of the
     * secret values.
     * @return A new vector that contains the plaintext values of type Data.
     *
     * NOTE: This method is useful when computing parties need to reveal
     * a secret-shared vector to each other.
     */
    virtual Vector open_shares_b(const EVector &shares) = 0;

    // **************************************** //
    //        Share generation operations       //
    // **************************************** //

    /**
     * Vectorized version of the get_share_a() method.
     * @param data A vector of input values of type Data.
     * @return A vector of shared vectors containing arithmetic shares.
     */
    virtual std::vector<EVector> get_shares_a(const Vector &data) = 0;

    /**
     * Vectorized version of the get_share_b() method.
     * @param data A vector of input values of type Data.
     * @return A vector of shared vectors containing boolean shares.
     */
    virtual std::vector<EVector> get_shares_b(const Vector &data) = 0;
    /**
     * Defines how to replicate local shares across parties according to a
     * protocol.
     *
     * NOTE: Computing parties must have received or generated the local shares
     * before calling this method.
     */
    virtual void replicate_shares() = 0;

    /**
     * Defines how to B-share a plaintext vector according to a protocol.
     * @param data - The plaintext vector that must be secret-shared among
     * computing parties.
     * @return The boolean shared vector of the party that calls this method.
     *
     * NOTE: This method is useful for secret-sharing plaintext data in Secrecy
     * programs.
     */
    virtual EVector secret_share_b(const Vector &data, const PartyID &data_party) = 0;

    /**
     * Defines how to A-share a plaintext vector according to a protocol.
     * @param data - The plaintext vector that must be secret-shared among
     * computing parties.
     * @return The arithmetic shared vector of the party that calls this method.
     *
     * NOTE: This method is useful for secret-sharing plaintext data in Secrecy
     * programs.
     */
    virtual EVector secret_share_a(const Vector &data, const PartyID &data_party) = 0;

    /**
     * @brief Create a "secret" share of a public value, `x`. This
     * implemented by setting one share to `x` and all others to zero;
     * this gives a valid sharing under both arithmetic and boolean.
     *
     * @param x
     * @return EVector
     */
    virtual EVector public_share(const Vector &x) = 0;
};
}  // namespace secrecy

#endif  // SECRECY_PROTOCOL_H
