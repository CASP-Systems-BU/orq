#pragma once

#include <sodium.h>

#include <type_traits>

#include "backend/common/hash.h"
#include "core/protocols/protocol.h"
#include "op_structs.h"

namespace orq {

class Verifier_4PC;

struct PartyAssignment {
    int hasher;
    int sender;
    int receiver;

    // Default elementwise comparison
    auto operator<=>(const PartyAssignment&) const = default;

    friend std::ostream& operator<<(std::ostream& os, const PartyAssignment& pa) {
        os << "[" << pa.sender << " -> " << pa.receiver << "; " << pa.hasher << "]";
        return os;
    }
};

enum class JmpBehavior { Batched, Unbatched };

namespace detail {
    std::shared_ptr<Verifier_4PC> getVerifierInstance(PartyID partyID, WorkerConfig wc,
                                                      Communicator* communicator,
                                                      random::RandomnessManager* randomnessManager);
}  // namespace detail

/**
 * @brief Implementation of the "Fantastic Four" paper by Dalskov et al.
 *
 * This version implements functionalities exactly as specified in the paper,
 * without any optimizations.
 *
 * @tparam Data Plaintext data type.
 * @tparam Share Replicated share type.
 * @tparam Vector Data container type.
 * @tparam EVector Share container type.
 */
template <typename Data, typename Share, typename Vector, typename EVector,
          bool is_verifier = false>
class Fantastic_4PC : public Protocol<Data, Share, Vector, EVector> {
    std::map<PartyAssignment, std::unique_ptr<Hash>> hash_states;
    bool malicious_check_ok = true;

   protected:
    WorkerConfig wc;

    using EVector_t = EVector;
    using Vector_t = Vector;

    /**
     * @brief Computes the JMP receiver.
     *
     * Designed to give each party a chance to be the receiver.
     * This is required for open to be correct.
     *
     * @param i First party.
     * @param j Second party.
     * @return Party ID of the receiver.
     */
    inline int next_party(int i, int j) {
        int a = std::min(i, j);
        int b = std::max(i, j);

        if (b == (a + 1) % 4) {
            return (b + 1) % 4;
        } else {
            return (a + 1) % 4;
        }
    }

    std::shared_ptr<Verifier_4PC> verifier;

    /**
     * @brief Return the index of the missing party.
     *
     * All party indices will sum to 6 (0 + 1 + 2 + 3), so just subtract the
     * given parties to find the missing one.
     *
     * @param i First party.
     * @param j Second party.
     * @param k Third party.
     * @return Index of the missing party.
     */
    inline int excluded_party(int i, int j, int k) { return (0 + 1 + 2 + 3) - (i + j + k); }

    int excluded_from_set(std::set<int> s) {
        return (0 + 1 + 2 + 3) - std::accumulate(s.begin(), s.end(), 0);
    }

    /**
     * @brief Convert absolute party ID to relative party ID.
     *
     * @param p Absolute party ID.
     * @return Relative party ID.
     */
    inline int abs2rel(int p) { return (p - this->partyID + 4) % 4; }

    /**
     * @brief Convert relative party ID to absolute party ID.
     *
     * @param p Relative party ID.
     * @return Absolute party ID.
     */
    inline int rel2abs(int p) { return (this->partyID + p) % 4; }

    /**
     * @brief Compute which relative share is \f$x_p\f$ (my share excluding party p).
     *
     * If self, return 0, even though that's not a valid share (this is to
     * prevent out-of-bounds array accesses on party-agnostic code).
     *
     * @param p Party ID.
     * @return Relative share index.
     */
    inline int abs2sh(int p) { return p == this->partyID ? 0 : (abs2rel(p) - 1 + 4) % 4; }

    /**
     * @brief Given two JMP senders, decide who will hash and who will send.
     *
     * Different strategies here could change performance by redistributing work.
     * This function must be commutative (who_hashes(a, b) == who_hashes(b, a)).
     *
     * @param a First party.
     * @param b Second party.
     * @return Party who should hash.
     */
    inline int who_hashes(int a, int b) {
        assert(a != b);
        return std::max(a, b);
    }

    /**
     * @brief Initialize hash states for malicious security.
     */
    void init_hashes() {
        for (int i = 0; i < 4; i++) {
            for (int j = i + 1; j < 4; j++) {
                for (int r = 0; r < 4; r++) {
                    if (i == r || j == r) {
                        continue;
                    }

                    auto pa = _jmp_assignments(i, j, r);

                    hash_states[pa] = std::make_unique<Hash>();

                    // Seed the hash with the party ID for domain separation
                    u_char seed = (i << 6) | (j << 3) | r;

                    hash_states[pa]->update(std::span<const u_char>(&seed, 1));
                }
            }
        }
    }

    /**
     * @brief Internal method to open shares using JMP protocol. Malicious check run
     * automatically by the protocol layer.
     *
     * @param sh Input shared vector.
     * @return Opened plaintext vector.
     */
    Vector _open_shares(const EVector& sh) {
        size_t N = sh.size();
        Vector sh3(N);

        // Pairs of parties JMP their common share to the appropriate party
        // missing it.
        for (int Pi = 0; Pi < 4; Pi++) {
            // 0, 1;  1, 2;  2, 3;  3, 0
            int Pj = (Pi + 1) % 4;
            int Pr = next_party(Pi, Pj);

            if (this->partyID == Pr) {
                // Receive into extra vector
                _jmp_recv(sh3, Pi, Pj, Pr, JmpBehavior::Unbatched);
            } else if (this->partyID == Pi || this->partyID == Pj) {
                // Send missing share
                auto rel_sh = abs2sh(Pr);
                _jmp_send(sh(rel_sh), Pi, Pj, Pr, JmpBehavior::Unbatched);
            }
        }
        return sh3;
    }

    /**
     * @brief Generate shares for given data with specified encoding.
     *
     * @tparam E Encoding type (AShared or BShared).
     * @param data Input plaintext vector.
     * @return Vector of shared vectors for all parties.
     */
    template <typename orq::Encoding E>
    std::vector<EVector> get_shares(const Vector& data) {
        auto size = data.size();
        Vector s0(size), s1(size), s2(size);
        this->randomnessManager->localPRG->getNext(s0);
        this->randomnessManager->localPRG->getNext(s1);
        this->randomnessManager->localPRG->getNext(s2);

        Vector s3(size);
        if constexpr (E == Encoding::AShared) {
            s3 = data - s0 - s1 - s2;
        } else if constexpr (E == Encoding::BShared) {
            s3 = data ^ s0 ^ s1 ^ s2;
        }

        return {EVector({s1, s2, s3}), EVector({s2, s3, s0}), EVector({s3, s0, s1}),
                EVector({s0, s1, s2})};
    }

    /**
     * @brief Internal secret sharing method with specified encoding.
     *
     * @tparam E Encoding type (AShared or BShared).
     * @param data Input plaintext vector.
     * @param data_party Party that owns the data.
     * @return This party's shared vector.
     */
    template <orq::Encoding E>
    EVector _secret_share(const Vector& data, const PartyID data_party) {
        auto size = data.size();
        if (this->partyID == data_party) {
            auto shares = get_shares<E>(data);

            // send to everyone else
            for (int rel = 1; rel < 4; rel++) {
                this->communicator->sendShares(shares[rel](0), rel);
                this->communicator->sendShares(shares[rel](1), rel);
                this->communicator->sendShares(shares[rel](2), rel);
            }

            return shares[0];
        } else {
            EVector s(size);
            int recv_from = data_party - this->partyID;
            // Receive second shared vector from the predecessor
            this->communicator->receiveShares(s(0), recv_from);
            this->communicator->receiveShares(s(1), recv_from);
            this->communicator->receiveShares(s(2), recv_from);
            return s;
        }
    }

    /**
     * @brief Compute JMP assignments for parties Pi and Pj.
     *
     * @param Pi First sending party.
     * @param Pj Second sending party.
     * @param Pr Receiver
     * @return Tuple containing hash party, send party, and hash ID.
     */
    PartyAssignment _jmp_assignments(int Pi, int Pj, int Pr) {
        int hash_party = who_hashes(Pi, Pj);
        int send_party = Pi == hash_party ? Pj : Pi;

        return PartyAssignment{
            .hasher = hash_party,
            .sender = send_party,
            .receiver = Pr,
        };
    }

    /**
     * @brief JMP receive operation for malicious security.
     *
     * @param x Vector to receive data into.
     * @param Pi First sender party.
     * @param Pj Second sender party.
     * @param Pr Receiver party (must be this party).
     */
    virtual void _jmp_recv(Vector& x, const int Pi, const int Pj, const int Pr,
                           const JmpBehavior behavior = JmpBehavior::Batched) {
        // ONLY receiver can call this.
        assert(this->partyID == Pr);

        auto pa = _jmp_assignments(Pi, Pj, Pr);

        // Receive & update my hash.
        this->communicator->receiveShares(x, abs2rel(pa.sender));

        if (behavior == JmpBehavior::Batched) {
            hash_states[pa]->update(x.span());
        } else {
            orq::Vector<uint8_t> h(crypto_generichash_BYTES);
            this->communicator->receiveShares(h, abs2rel(pa.hasher));

            if (!h.same_as(Hash::generateHash(x.span()), false)) {
                malicious_check_ok = false;
            }

#ifndef MAL_TEST_MODE
            assert(malicious_check_ok);
#endif
        }
    }

    /**
     * @brief JMP send operation for malicious security.
     *
     * @param x Vector to send.
     * @param Pi First sender party.
     * @param Pj Second sender party.
     * @param Pr Receiver party.
     */
    virtual void _jmp_send(const Vector& x, const int Pi, const int Pj, const int Pr,
                           const JmpBehavior behavior = JmpBehavior::Batched) {
        auto pa = _jmp_assignments(Pi, Pj, Pr);

        if (this->partyID == pa.sender) {
            this->communicator->sendShares(x, abs2rel(Pr));
        } else if (this->partyID == pa.hasher) {
            if (behavior == JmpBehavior::Batched) {
                hash_states[pa]->update(x.span());
            } else {
                this->communicator->sendShares(Hash::generateHash(x.span()), abs2rel(Pr));
            }
        }
    }

    /**
     * @brief Malicious check for hash consistency.
     *
     * @return True if checks passed, false otherwise.
     */
    bool start_malicious_check_internal() override;

    /**
     * @brief Run the [BS26] CheckEqs verification protocol
     *
     * @return true
     * @return false
     */
    bool finalize_malicious_check_internal() override;

    void reset_malicious_state() override;

   public:
    /**
     * @brief Override of default groups to achieve malicious security.
     *
     * This group selection gives us two copies of each share per group (redundancy).
     *
     * @return Vector of party groups for malicious security.
     */
    std::vector<std::set<int>> getGroups() const override {
        return ProtocolBase::generateGroups(4, 3, 1);
    }

    // Configuration Parameters
    static constexpr int parties_num = 4;

    /**
     * @brief Constructor for Fantastic_4PC protocol (Dalskov implementation).
     *
     * @param _partyID Party identifier.
     * @param _communicator Pointer to communicator.
     * @param _randomnessManager Pointer to randomness manager.
     */
    Fantastic_4PC(PartyID _partyID, WorkerConfig wc, Communicator* _communicator,
                  random::RandomnessManager* _randomnessManager)
        : Protocol<Data, Share, Vector, EVector>(_communicator, _randomnessManager, _partyID, 4, 3),
          wc(wc) {
        // Only the base protocol gets a verifier instance.
        if constexpr (!is_verifier) {
            verifier = detail::getVerifierInstance(_partyID, wc, _communicator, _randomnessManager);
        }
        init_hashes();
    }

    /**
     * @brief Joint message passing protocol.
     *
     * @param x Vector of (non-secret shared) data.
     * @param from Owner of this data.
     * @param also_from Co-owner of data.
     * @param to Party who will receive data from both.
     */
    void jmp(Vector& x, int from, int also_from, int to) {
        if (this->partyID == to) {
            _jmp_recv(x, from, also_from, to);
        } else if (this->partyID == from || this->partyID == also_from) {
            _jmp_send(x, from, also_from, to);
        }
        // excluded party nops (doesn't even need to call, but probably best
        // to do so)
    }

    /**
     * @brief Shared-input function.
     *
     * Two parties, who both know a plaintext value x, secret-share it with the other two
     * parties.
     *
     * @tparam E Encoding type (A- or B-shared).
     * @param x Plaintext data.
     * @param Pi First owner.
     * @param Pj Second owner.
     * @param Pg Optional third party (computed if not provided).
     * @param Ph Optional fourth party (computed if not provided).
     * @return Shared vector.
     */
    template <orq::Encoding E>
    EVector inp(const Vector& x, int Pi, int Pj, std::optional<int> Pg = {},
                std::optional<int> Ph = {}) {
        size_t n = x.size();
        EVector r(n);

        // Shares are: (assuming i < j < g < h)
        //   Pi:  . 0 xg xh
        //   Pj:  0 . xg xh
        //   Pg:  0 0 .  xh
        //   Ph:  0 0 xg .
        // with xi = xj = 0

        if (!Pg) {
            Pg = next_party(Pi, Pj);
        }

        if (!Ph) {
            Ph = excluded_party(Pi, Pj, *Pg);
        }

        // Pg, Ph respectively will just have junk here, but it's ignored
        Vector xg = r(abs2sh(*Pg));
        Vector xh = r(abs2sh(*Ph));

        if (this->partyID != Pg) {
            // Pi, Pj, Ph generate random, excluding Pg
            this->randomnessManager->commonPRGManager->get(abs2rel(*Pg))->getNext(xg);

            if (this->partyID != Ph) {
                // Pi Pj generate xh
                if constexpr (E == Encoding::AShared) {
                    xh = x - xg;
                } else if constexpr (E == Encoding::BShared) {
                    xh = x ^ xg;
                }
            }
        }

        // Ph does nothing in this function
        jmp(xh, Pi, Pj, *Pg);

        return r;
    }

    /**
     * @brief Generic binary operation on shared vectors using the 4PC protocol.
     *
     * Template function to avoid code duplication for structurally identical operations
     * (e.g., multiply_a and and_b) that differ only in their operators.
     *
     * The Ops struct should define:
     *   - operator(): accumulation operation (e.g., + for arithmetic, ^ for boolean)
     *   - op1: first binary operation (e.g., * for arithmetic, & for boolean)
     *   - op2: second binary operation (e.g., + for arithmetic, ^ for boolean)
     *   - do_truncate: boolean constant indicating if truncate() should be called
     *
     * @tparam EncodingType The encoding type (AShared or BShared).
     * @tparam Ops Operator functor struct.
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    template <orq::Encoding EncodingType, typename Ops>
    void generic_multiply(const EVector& x, const EVector& y, EVector& z, Ops ops = {}) {
        int Pi, Pj, Pg, Ph, hi, gi;
        EVector r(z.size());

        // Iteration order:
        // (0, 1) (0, 2) (0, 3)
        // (1, 2) (1, 3)
        // (2, 3)
        for (Pi = 0; Pi < 4; Pi++) {
            for (Pj = Pi + 1; Pj < 4; Pj++) {
                // need to convert absolute share indices to relative...
                // other parties
                Pg = next_party(Pi, Pj);
                Ph = excluded_party(Pi, Pj, Pg);

                if (this->partyID == Pg || this->partyID == Ph) {
                    // hack for now: these parties pass in nothing on this call
                    // just need a vector to pull the size out of
                    // TODO: make an alternative signature that just takes `n`
                    ops.accumulateOp(r, inp<EncodingType>(r(0), Pi, Pj, Pg, Ph));
                } else {
                    // compute relative share indices
                    hi = abs2sh(Ph);
                    gi = abs2sh(Pg);

                    // Compute r += x(hi)*y(gi) + x(gi)*y(hi)
                    ops.accumulateOp(r, inp<EncodingType>(ops.addOp(ops.multiplyOp(x(hi), y(gi)),
                                                                    ops.multiplyOp(x(gi), y(hi))),
                                                          Pi, Pj, Pg, Ph));
                }
            }
        }

        // self terms: z = r + x * y
        z = ops.addOp(r, ops.multiplyOp(x, y));

        this->handle_precision(x, y, z);
        if constexpr (Ops::do_truncate) {
            this->truncate(z);
        }
    }

    /**
     * @brief Multiply two arithmetic-shared vectors.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void multiply_a(const EVector& x, const EVector& y, EVector& z) override {
        this->template generic_multiply<Encoding::AShared, ArithmeticOps>(x, y, z);
    }

    /**
     * @brief Divide an arithmetic-shared vector by a constant.
     *
     * @param x Input vector.
     * @param c Constant divisor.
     * @return Pair of vectors (quotient and error correction).
     */
    std::pair<EVector, EVector> div_const_a(const EVector& x, const Data c) override {
        auto size = x.size();
        EVector res(size), err(size);

        // Computation
        if (this->partyID == 3 || this->partyID == 1) {
            res(1) = x(0) + x(1);
            auto [res_, err_] = res(1).divrem(c);

#ifdef USE_DIVISION_CORRECTION
            auto x_sum_neg = res(1) < 0;

            res_ -= x_sum_neg;
            err_ += x_sum_neg * c;

            if (this->partyID == 3) {
                err_ = err_ - c;
            }

            err = inp<Encoding::AShared>(err_, 3, 2) + inp<Encoding::AShared>(err_, 1, 0);
#endif
            res = inp<Encoding::AShared>(res_, 3, 2) + inp<Encoding::AShared>(res_, 1, 0);

        } else if (this->partyID == 2 || this->partyID == 0) {
            res(2) = x(1) + x(2);
            auto [res_, err_] = res(2).divrem(c);

#ifdef USE_DIVISION_CORRECTION
            auto x_sum_neg = res(2) < 0;

            res_ -= x_sum_neg;
            err_ += x_sum_neg * c;

            if (this->partyID == 2) {
                err_ = err_ - c;
            }
            err = inp<Encoding::AShared>(err_, 3, 2) + inp<Encoding::AShared>(err_, 1, 0);
#endif
            res = inp<Encoding::AShared>(res_, 3, 2) + inp<Encoding::AShared>(res_, 1, 0);
        }

        return {res, err};
    }

    /**
     * @brief Boolean AND operation.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void and_b(const EVector& x, const EVector& y, EVector& z) override {
        this->template generic_multiply<Encoding::BShared, BooleanOps>(x, y, z);
    }

    /**
     * @brief Boolean NOT operation.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void not_b(const EVector& x, EVector& y) override {
        int p = this->partyID;
        y = {
            (p == 2 ? ~x(0) : x(0)),
            (p == 1 ? ~x(1) : x(1)),
            (p == 0 ? ~x(2) : x(2)),
        };
    }

    /**
     * @brief Boolean NOT operation for the least significant bit.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void not_b_1(const EVector& x, EVector& y) override {
        int p = this->partyID;
        Vector x0 = x(0) & 1, x1 = x(1) & 1, x2 = x(2) & 1;

        y = {
            (p == 2 ? !x0 : x0),
            (p == 1 ? !x1 : x1),
            (p == 0 ? !x2 : x2),
        };
    }

    /**
     * @brief Convert a boolean-shared bit in the least significant position
     * to an arithmetic sharing:
     *
     * 4PC share distribution:
     * P0: [B, C, D]  ->       B   C   D
     * P1: [C, D, A]  ->   A       C   D
     * P2: [D, A, B]  ->   A   B       D
     * P3: [A, B, C]  ->   A   B   C
     *
     * Reducing the 4-share to a 2-share:
     * share0 = C ^ D
     * share1 = A ^ B
     *
     * P0 and P1 can both calculate share0.
     * P2 and P3 can both calculate share1.
     *
     * Since 2 parties can calculate each of the above, INP can be
     * used to generate a secret-share for share0 and share1.
     *
     * XOR evaluated as: x ^ y = (x - y)^2 (holds true for binary input)
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void b2a_bit(const EVector& x, EVector& y) override {
        EVector x_prime = x & 1;

        Vector s0(x.size()), s1(x.size());

        // Reducing to a 2-share
        if (this->partyID == 0) {
            s0 = x_prime(1) ^ x_prime(2);
        } else if (this->partyID == 1) {
            s0 = x_prime(0) ^ x_prime(1);
        } else if (this->partyID == 2) {
            s1 = x_prime(1) ^ x_prime(2);
        } else if (this->partyID == 3) {
            s1 = x_prime(0) ^ x_prime(1);
        }

        // Distributing reduced shares
        // Reusing vectors since inp<Encoding::AShared> operates on a new vector internally
        auto share0 = inp<Encoding::AShared>(s0, 1, 0);
        auto share1 = inp<Encoding::AShared>(s1, 3, 2);

        // precision is 0 automatically

        // XOR: (x-y)^2
        share0 -= share1;
        multiply_a(share0, share0, y);  // Reusing share1 to avoid extra vector allocation
    }

    /**
     * @brief Redistribute boolean shares.
     *
     * @param x Input vector.
     * @return Pair of vectors (redistributed shares).
     */
    std::pair<EVector, EVector> redistribute_shares_b(const EVector& x) override {
        auto size = x.size();
        EVector res_1(size), res_2(size);

        // Computation
        if (this->partyID == 0 || this->partyID == 2) {
            res_2(0) = x(0) + x(1);
        } else if (this->partyID == 3 || this->partyID == 1) {
            res_2(0) = x(1) + x(2);
        }

        res_1 = inp<Encoding::BShared>(res_2(0), 0, 3);
        res_2 = inp<Encoding::BShared>(res_2(0), 2, 1);

        return {res_1, res_2};
    }

    /**
     * @brief Reconstruct arithmetic shares from a vector of shares.
     *
     * @param shares Input shares.
     * @return Reconstructed arithmetic shares.
     */
    Data reconstruct_from_a(const std::vector<Share>& shares) override {
        return shares[0][0] + shares[1][0] + shares[2][0] + shares[3][0];
    }

    /**
     * @brief Reconstruct arithmetic shares from a vector of shares.
     *
     * @param shares Input shares.
     * @return Reconstructed arithmetic shares.
     */
    Vector reconstruct_from_a(const std::vector<EVector>& shares) override {
        return shares[0](0) + shares[1](0) + shares[2](0) + shares[3](0);
    }

    /**
     * @brief Reconstruct boolean shares from a vector of shares.
     *
     * @param shares Input shares.
     * @return Reconstructed boolean shares.
     */
    Data reconstruct_from_b(const std::vector<Share>& shares) override {
        return shares[0][0] ^ shares[1][0] ^ shares[2][0] ^ shares[3][0];
    }

    /**
     * @brief Reconstruct boolean shares from a vector of shares.
     *
     * @param shares Input shares.
     * @return Reconstructed boolean shares.
     */
    Vector reconstruct_from_b(const std::vector<EVector>& shares) override {
        return shares[0](0) ^ shares[1](0) ^ shares[2](0) ^ shares[3](0);
    }

    /**
     * @brief Open arithmetic shares to reveal plaintext.
     *
     * @param shares Input shared vector.
     * @return Opened plaintext vector.
     */
    Vector internal_open_a(const EVector& shares) override {
        auto sh3 = _open_shares(shares);
        return shares(0) + shares(1) + shares(2) + sh3;
    }

    /**
     * @brief Open boolean shares to reveal plaintext.
     *
     * @param shares Input shared vector.
     * @return Opened plaintext vector.
     */
    Vector internal_open_b(const EVector& shares) override {
        auto sh3 = _open_shares(shares);
        return shares(0) ^ shares(1) ^ shares(2) ^ sh3;
    }

    /**
     * @brief Generate arithmetic shares for a vector.
     *
     * @param data Input data vector.
     * @return Vector of shares for all parties.
     */
    std::vector<EVector> get_shares_a(const Vector& data) override {
        return get_shares<Encoding::AShared>(data);
    }

    /**
     * @brief Generate boolean shares for a vector.
     *
     * @param data Input data vector.
     * @return Vector of shares for all parties.
     */
    std::vector<EVector> get_shares_b(const Vector& data) override {
        return get_shares<Encoding::BShared>(data);
    }

    /**
     * @brief Secret share data using the custom 4PC protocol.
     *
     * @param data Input data vector.
     * @param data_party Party ID of the data party.
     * @return Secret shared vector.
     */
    EVector secret_share_b_internal(const Vector& data, const PartyID& data_party = 0) override {
        return _secret_share<Encoding::BShared>(data, data_party);
    }

    /**
     * @brief Secret share data using the custom 4PC protocol.
     *
     * @param data Input data vector.
     * @param data_party Party ID of the data party.
     * @return Secret shared vector.
     */
    EVector secret_share_a_internal(const Vector& data, const PartyID& data_party = 0) override {
        return _secret_share<Encoding::AShared>(data, data_party);
    }

    /**
     * @brief Public sharing of a vector x.
     *
     * @param x Input data vector.
     * @param who_knows set of parties who know the value
     * @return EVector
     */
    EVector public_share(const Vector& x, const std::set<PartyID>& who_knows) override {
        auto me = this->partyID;
        auto size = x.size();

        // zero initialized
        EVector r(size);

        // I hold shares x_{me+1}, x_{me+2}, x_{me+3} mod 4

        int k = 0;
        if (!who_knows.empty()) {
            // the share will be whichever party is not in the group
            assert(who_knows.size() == 3);

            // Use subtraction trick from excluded_party above
            k = excluded_from_set(who_knows);
        }

        if (me != k) {
            r(abs2sh(k)) = x;
        }

        return r;
    }

    /**
     * @brief An override version of reshare for malicious security in 4PC.
     *
     * @param v The vector to be rerandomized and reshared.
     * @param group The group performing the resharing.
     * @param binary A flag indicating an arithmetic or binary encoding of the vector.
     */
    virtual void reshare(EVector& v, const std::set<int> group, const bool binary) override {
        assert(group.size() == 3);

        // find receive party
        int receiver = excluded_from_set(group);

        // all parties in the group generate a zero sharing to rerandomize the vector
        if (receiver != this->partyID) {
            // generate randomness
            std::vector<Vector> rand;
            for (int i = 0; i < 4; i++) {
                rand.push_back(Vector(v.size()));
            }

            if (binary) {
                this->randomnessManager->zeroSharingGenerator->groupGetNextBinary(rand, group);
            } else {
                this->randomnessManager->zeroSharingGenerator->groupGetNextArithmetic(rand, group);
            }

            auto my_shares = this->getPartyShareMappings()[this->partyID];
            // Generating too many random values here: each party only needs
            // RepNum random vectors, but is generating PartyNum
            for (int i = 0; i < my_shares.size(); i++) {
                if (binary) {
                    v(i) ^= rand[my_shares[i]];
                } else {
                    v(i) += rand[my_shares[i]];
                }
            }
        }

        // distribute new shares - by absolute index
        for (int sh = 0; sh < 4; sh++) {
            // receiver does not get or have this share
            if (receiver == sh) {
                continue;
            }

            int Pi = -1;
            int Pj = -1;

            // Pi, Pj are the two parties who have this share
            // Order not important because jmp assigns canonical roles
            for (auto g : group) {
                if (sh == g) continue;
                if (Pi < 0) {
                    Pi = g;
                } else if (Pj < 0) {
                    Pj = g;
                    break;
                }
            }

            auto rel_sh = abs2sh(sh);
            jmp(v(rel_sh), Pi, Pj, receiver);
        }
    }

    void dot_product_a(const EVector& x, const EVector& y, EVector& z,
                       const size_t aggSize = 0) override {
        auto ops = DotProductOps(aggSize);
        this->template generic_multiply<Encoding::AShared, DotProductOps>(x, y, z, ops);
    }
};

}  // namespace orq

#include "core/protocols/verifier_4pc.h"

namespace orq {

template <typename Data, typename Share, typename Vector, typename EVector, bool is_verifier>
bool Fantastic_4PC<Data, Share, Vector, EVector, is_verifier>::start_malicious_check_internal() {
    if (wc.worker_id == 0) {
        verifier->start_malicious_check();
    }

    bool ok = true;
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            for (int r = 0; r < 4; r++) {
                // must be distinct
                if (r == i || r == j) {
                    continue;
                }

                // I'm not involved in this hash
                if (excluded_party(i, j, r) == this->partyID) {
                    continue;
                }

                auto pa = _jmp_assignments(i, j, r);
                auto hash = hash_states[pa]->finalize();
                verifier->template aggregate_hash<Data>(wc.worker_id, pa, hash);
            }
        }
    }

    return true;
}

template <typename Data, typename Share, typename Vector, typename EVector, bool is_verifier>
bool Fantastic_4PC<Data, Share, Vector, EVector, is_verifier>::finalize_malicious_check_internal() {
    // Only thread 0 calls into the verifier (which checks all threads' hashes)
    // Other threads can just return true.
    // TODO: atomic flags inside finalize() may have rendered this check unnecessary.
    malicious_check_ok &= wc.worker_id == 0 ? verifier->finalize_malicious_check() : true;
    init_hashes();
    return malicious_check_ok;
}

template <typename Data, typename Share, typename Vector, typename EVector, bool is_verifier>
void Fantastic_4PC<Data, Share, Vector, EVector, is_verifier>::reset_malicious_state() {
    // Clear out the hashes,
    init_hashes();

    // Raise the flag,
    malicious_check_ok = true;

    // And reset the underlying verifier state.
    if (wc.worker_id == 0) {
        verifier->reset_malicious_state();
    }
}

/**
 * @brief Factory type alias for Fantastic_4PC protocol (Dalskov implementation).
 *
 * @tparam Share Share type template.
 * @tparam Vector Vector type template.
 * @tparam EVector Encoding vector type template.
 */
template <template <typename> class Share, template <typename> class Vector,
          template <typename> class EVector>
using Fantastic_4PC_Factory = DefaultProtocolFactory<Fantastic_4PC, Share, Vector, EVector>;

}  // namespace orq
