#pragma once

#include "protocol_factory.h"

#ifdef MPC_PROTOCOL_FANTASTIC_FOUR
#pragma message "Using custom 4PC protcol."
#endif

namespace orq {
/**
 * @brief Implements the secure primitives for the 4-party malicious protocol by Dalskov et al.
 * that uses replicated secret sharing.
 *
 * @tparam Data Plaintext data type.
 * @tparam Share Replicated share type.
 * @tparam Vector Data container type.
 * @tparam EVector Share container type.
 */
template <typename Data, typename Share, typename Vector, typename EVector>
class Fantastic_4PC : public Protocol<Data, Share, Vector, EVector> {
    /**
     * @brief Malicious check to assert that two vectors are the same.
     *
     * Given two vectors x and y, assert that they are the same. If they are not,
     * accuse the specified parties and abort.
     *
     * @param x The first vector.
     * @param y The second vector.
     * @param p1 The first party relative ID to accuse if they are not the same.
     * @param p2 The second party relative ID to accuse if they are not the same.
     */
    void malicious_check(const Vector &x, const Vector &y, const int p1, const int p2) {
        if (!x.same_as(y)) {
            printf("party %d accuses (%d,%d)\n", this->partyID, (this->partyID + p1 + 4) % 4,
                   (this->partyID + p2 + 4) % 4);
            exit(-1);
        }
    }

   public:
    /**
     * @brief Override of default groups to achieve malicious security.
     *
     * This group selection gives us two copies of each share per group (redundancy).
     *
     * @return Vector of party groups for malicious security.
     */
    std::vector<std::set<int>> getGroups() const {
        return {{0, 1, 2}, {1, 2, 3}, {2, 3, 0}, {3, 0, 1}};
    }

    // Configuration Parameters
    static int parties_num;

    /**
     * @brief Constructor for Fantastic_4PC protocol.
     *
     * @param _partyID Party identifier.
     * @param _communicator Pointer to communicator.
     * @param _randomnessManager Pointer to randomness manager.
     */
    Fantastic_4PC(PartyID _partyID, Communicator *_communicator,
                  random::RandomnessManager *_randomnessManager)
        : Protocol<Data, Share, Vector, EVector>(_communicator, _randomnessManager, _partyID, 4,
                                                 3) {}

    /**
     * @brief Secure arithmetic multiplication with malicious security.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void multiply_a(const EVector &x, const EVector &y, EVector &z) {
        long long size = x.size();

        // Random values that exclude the NEXT and OPPOSITE party
        Vector r_next(size), r_opp(size), r_prev(size);
        this->randomnessManager->commonPRGManager->get(+1)->getNext(r_next);
        this->randomnessManager->commonPRGManager->get(+2)->getNext(r_opp);
        this->randomnessManager->commonPRGManager->get(-1)->getNext(r_prev);

        // Parties generate the cross terms they know
        // ...shared with previous party, randomness excludes next
        auto cross_10 = x(0) * y(0) + x(0) * y(1) + x(1) * y(0) - r_next;
        // ...shared with next party, randomness excludes opposite
        auto cross_12 = x(1) * y(1) + x(1) * y(2) + x(2) * y(1) - r_opp;
        // ...shared with opposite party.
        // We'll randomize below, since it's different per party.
        auto cross_02 = x(0) * y(2) + x(2) * y(0);

        // Use these vectors to as communicator buffers
        Vector mult_recv(size), mult_recv_check(size);

        // Exchange with next (previous) party
        this->communicator->exchangeShares(cross_10, mult_recv, +1, +3, size);
        // Exchange with opposite party
        this->communicator->exchangeShares(cross_12, mult_recv_check, +2, +2, size);

        malicious_check(mult_recv, mult_recv_check, +2, -1);

        // Each party's share has a common partial sum, plus additional
        // party-specific randomization & cross-terms. Build the common part
        // here.
        // `mult_recv` is the previous party's private cross term
        z(0) = mult_recv + r_next;
        z(1) = cross_10 + r_opp;
        z(2) = cross_12 + r_prev;

        // Next, compute the party-specific adjustment.

        if (this->partyID == 0) {
            // Get additional randomness
            this->randomnessManager->commonPRGManager->get(+1)->getNext(r_next);
            this->randomnessManager->commonPRGManager->get(+2)->getNext(r_opp);

            // Randomize cross term with new randomness and send to P1
            cross_02 -= r_next;
            this->communicator->sendShares(cross_02, +1, size);

            // Adjust shares
            z(0) += r_next;
            z(1) += r_opp;
            z(2) += cross_02;
        } else if (this->partyID == 1) {
            this->randomnessManager->commonPRGManager->get(+1)->getNext(r_next);

            // Send our cross term, and then receive two copies from P0 and
            // P2. Run malicious check. Both values are masked by a random
            // value which *excludes* P1.
            cross_02 -= r_next;
            this->communicator->exchangeShares(cross_02, mult_recv_check, +1, +1, size);
            this->communicator->receiveShares(mult_recv, -1, size);

            malicious_check(mult_recv, mult_recv_check, +2, -1);

            // Final adjustment
            z(0) += r_next;
            z(1) += mult_recv;
            z(2) += cross_02;
        } else if (this->partyID == 2) {
            this->randomnessManager->commonPRGManager->get(-1)->getNext(r_prev);

            cross_02 -= r_prev;

            this->communicator->exchangeShares(cross_02, mult_recv_check, -1, -1, size);
            this->communicator->receiveShares(mult_recv, +1, size);

            malicious_check(mult_recv, mult_recv_check, +1, +2);

            z(0) += cross_02;
            z(1) += mult_recv;
            z(2) += r_prev;
        } else if (this->partyID == 3) {
            this->randomnessManager->commonPRGManager->get(+2)->getNext(r_opp);
            this->randomnessManager->commonPRGManager->get(-1)->getNext(r_prev);

            // Randomize and send our cross term to P2
            cross_02 -= r_prev;
            this->communicator->sendShares(cross_02, -1, size);

            z(0) += cross_02;
            z(1) += r_opp;
            z(2) += r_prev;
        }

        this->handle_precision(x, y, z);
        this->truncate(z);
    }

    /**
     * @brief Input sharing protocol for specific party sequence.
     *
     * Note: this inp is for the div function pattern only.
     * [i precedes j precedes g precedes h].
     *
     * @param x Input vector to be shared.
     * @param i First party in sequence.
     * @param j Second party in sequence.
     * @param g Third party in sequence (receiver).
     * @param h Fourth party in sequence (randomness generator).
     * @return Shared vector.
     */
    EVector inp_a(const Vector &x, const int &i, const int &j, const int &g, const int &h) {
        EVector res(x.size());
        long long size = x.size();

        if (this->partyID == i) {
            // First working as main computer (i in inp)
            // res(0) = r_1, res(1) = res - r_1, res(2) = 0
            this->randomnessManager->commonPRGManager->get(+(g - i))->getNext(res(0));
            res(1) = x - res(0);
            this->communicator->sendShares(res(1), +1, res(1).size());
        } else if (this->partyID == j) {
            // First working as main computer (j in inp)
            // res(0) = 0, res(1) = r_2, res(2) = res - r_2
            this->randomnessManager->commonPRGManager->get(+(g - j))->getNext(res(1));
            res(2) = x - res(1);
            this->communicator->sendShares(res(2), +2, res(2).size());
        } else if (this->partyID == g) {
            // working as receiver (g in inp)
            // res(0) = [res - r_0]_, res(1) = 0, res(2) = 0
            Vector other(x.size());
            this->communicator->receiveShares(res(0), +3, x.size());
            this->communicator->receiveShares(other, +2, x.size());
            if (!other.same_as(res(0))) {
                printf("party %d accuses (%d,%d)\n", this->partyID, (this->partyID + 3) % 4,
                       (this->partyID + 2) % 4);
                exit(-1);
            }
        } else if (this->partyID == h) {
            // working as random generator (h in inp)
            // res(0) = 0, res(1) = 0, res(2) = r_3
            this->randomnessManager->commonPRGManager->get(+(g - h))->getNext(res(2));
        }

        return res;
    }

    /**
     * @brief Division by constant with malicious security.
     *
     * @param x Input vector.
     * @param c Constant divisor.
     * @return Pair of vectors (quotient and error correction).
     */
    std::pair<EVector, EVector> div_const_a(const EVector &x, const Data &c) {
        auto size = x.size();
        EVector res(size), err(size);

        // inp(i,j,g,h) algorithm
        // res(0) = r_1, res(1) = res ^ r_1, res(2) = 0, res(3) = 0
        // inp(0, 3, 1, 2) -- inp(2, 1, 3, 0)
        // shares in 3rd index and random number in 4th index

        // Computation
        if (this->partyID == 3 || this->partyID == 1) {
            res(1) = x(0) + x(1);
#ifdef USE_DIVISION_CORRECTION
            auto x_sum_neg = res(1) < 0;

            auto res_ = res(1) / c - x_sum_neg;
            auto err_ = res(1) % c + x_sum_neg * c;
            if (this->partyID == 3) {
                err_ = err_ - c;
            }

            res = inp_a(res_, 3, 2, 0, 1) + inp_a(res_, 1, 0, 2, 3);

            err = inp_a(err_, 3, 2, 0, 1) + inp_a(err_, 1, 0, 2, 3);
#else
            auto res_ = res(1) / c;
            res = inp_a(res_, 3, 2, 0, 1) + inp_a(res_, 1, 0, 2, 3);
#endif

        } else if (this->partyID == 2 || this->partyID == 0) {
            res(2) = x(1) + x(2);
#ifdef USE_DIVISION_CORRECTION
            auto x_sum_neg = res(2) < 0;

            auto res_ = res(2) / c - x_sum_neg;
            auto err_ = res(2) % c + x_sum_neg * c;
            if (this->partyID == 2) {
                err_ = err_ - c;
            }

            res = inp_a(res_, 3, 2, 0, 1) + inp_a(res_, 1, 0, 2, 3);

            err = inp_a(err_, 3, 2, 0, 1) + inp_a(err_, 1, 0, 2, 3);
#else
            auto res_ = res(2) / c;
            res = inp_a(res_, 3, 2, 0, 1) + inp_a(res_, 1, 0, 2, 3);
#endif
        }

        return {res, err};
    }

    /**
     * @brief Boolean input sharing protocol for specific party sequence.
     *
     * Note: this inp is for the div function pattern only.
     * [i precedes j precedes g precedes h].
     *
     * @param x Input vector to be shared.
     * @param i First party in sequence.
     * @param j Second party in sequence.
     * @param g Third party in sequence (receiver).
     * @param h Fourth party in sequence (randomness generator).
     * @return Shared vector.
     */
    EVector inp_b(const Vector &x, const int &i, const int &j, const int &g, const int &h) {
        EVector res(x.size());
        long long size = x.size();

        // inp(i,j,g,h) algorithm
        // res(0) = r_1, res(1) = res - r_1, res(2) = 0, res(3) = 0

        if (this->partyID == i) {
            // First working as main computer (i in inp)
            // res(0) = r_1, res(1) = res ^ r_1, res(2) = 0
            this->randomnessManager->commonPRGManager->get(+(g - i))->getNext(res(0));
            res(1) = x ^ res(0);
            this->communicator->sendShares(res(1), +1, res(1).size());
        } else if (this->partyID == j) {
            // First working as main computer (j in inp)
            // res(0) = 0, res(1) = r_2, res(2) = res ^ r_2
            this->randomnessManager->commonPRGManager->get(+(g - j))->getNext(res(1));
            res(2) = x ^ res(1);
            this->communicator->sendShares(res(2), +2, res(2).size());
        } else if (this->partyID == g) {
            // working as receiver (g in inp)
            // res(0) = [res ^ r_0]_, res(1) = 0, res(2) = 0
            Vector other(x.size());
            this->communicator->receiveShares(res(0), +3, x.size());
            this->communicator->receiveShares(other, +2, x.size());
            if (!other.same_as(res(0))) {
                printf("party %d accuses (%d,%d)\n", this->partyID, (this->partyID + 3) % 4,
                       (this->partyID + 2) % 4);
                exit(-1);
            }
        } else if (this->partyID == h) {
            // working as random generator (h in inp)
            // res(0) = 0, res(1) = 0, res(2) = r_3
            this->randomnessManager->commonPRGManager->get(+(g - h))->getNext(res(2));
        }

        return res;
    }

    /**
     * @brief Secure bitwise AND with malicious security.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void and_b(const EVector &x, const EVector &y, EVector &z) {
        long long size = x.size();

        // Random values that exclude the NEXT and OPPOSITE party
        Vector r_next(size), r_opp(size), r_prev(size);
        this->randomnessManager->commonPRGManager->get(+1)->getNext(r_next);
        this->randomnessManager->commonPRGManager->get(+2)->getNext(r_opp);
        this->randomnessManager->commonPRGManager->get(-1)->getNext(r_prev);

        // Parties generate the cross terms they know
        // ...shared with previous party, randomness excludes next
        auto cross_10 = x(0) & y(0) ^ x(0) & y(1) ^ x(1) & y(0) ^ r_next;
        // ...shared with next party, randomness excludes opposite
        auto cross_12 = x(1) & y(1) ^ x(1) & y(2) ^ x(2) & y(1) ^ r_opp;
        // ...shared with opposite party.
        // We'll randomize below, since it's different per party.
        auto cross_02 = x(0) & y(2) ^ x(2) & y(0);

        // Use these vectors to as communicator buffers
        Vector mult_recv(size), mult_recv_check(size);

        // Exchange with next (previous) party
        this->communicator->exchangeShares(cross_10, mult_recv, +1, -1, size);
        // Exchange with opposite party
        this->communicator->exchangeShares(cross_12, mult_recv_check, +2, +2, size);

        malicious_check(mult_recv, mult_recv_check, +2, -1);

        // Each party's share has a common partial sum, plus additional
        // party-specific randomization & cross-terms. Build the common part
        // here.
        // `mult_recv` is the previous party's private cross term
        z(0) = mult_recv ^ r_next;
        z(1) = cross_10 ^ r_opp;
        z(2) = cross_12 ^ r_prev;

        // Next, compute the party-specific adjustment.

        if (this->partyID == 0) {
            // Get additional randomness
            this->randomnessManager->commonPRGManager->get(+1)->getNext(r_next);
            this->randomnessManager->commonPRGManager->get(+2)->getNext(r_opp);

            // Randomize cross term with new randomness and send to P1
            cross_02 ^= r_next;
            this->communicator->sendShares(cross_02, +1, size);

            // Adjust shares
            z(0) ^= r_next;
            z(1) ^= r_opp;
            z(2) ^= cross_02;
        } else if (this->partyID == 1) {
            this->randomnessManager->commonPRGManager->get(+1)->getNext(r_next);

            // Send our cross term, and then receive two copies from P0 and
            // P2. Run malicious check. Both values are masked by a random
            // value which *excludes* P1.
            cross_02 ^= r_next;
            this->communicator->exchangeShares(cross_02, mult_recv_check, +1, +1, size);
            this->communicator->receiveShares(mult_recv, -1, size);

            malicious_check(mult_recv, mult_recv_check, +2, -1);

            // Final adjustment
            z(0) ^= r_next;
            z(1) ^= mult_recv;
            z(2) ^= cross_02;
        } else if (this->partyID == 2) {
            this->randomnessManager->commonPRGManager->get(-1)->getNext(r_prev);

            cross_02 ^= r_prev;

            this->communicator->exchangeShares(cross_02, mult_recv_check, -1, -1, size);
            this->communicator->receiveShares(mult_recv, +1, size);

            malicious_check(mult_recv, mult_recv_check, +1, +2);

            z(0) ^= cross_02;
            z(1) ^= mult_recv;
            z(2) ^= r_prev;
        } else if (this->partyID == 3) {
            this->randomnessManager->commonPRGManager->get(+2)->getNext(r_opp);
            this->randomnessManager->commonPRGManager->get(-1)->getNext(r_prev);

            // Randomize and send our cross term to P2
            cross_02 ^= r_prev;
            this->communicator->sendShares(cross_02, -1, size);

            z(0) ^= cross_02;
            z(1) ^= r_opp;
            z(2) ^= r_prev;
        }

        this->handle_precision(x, y, z);
    }

    /**
     * @brief Boolean NOT operation.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void not_b(const EVector &x, EVector &y) {
        if (this->partyID == 0) {
            y = {x(0), x(1), ~x(2)};
        } else if (this->partyID == 1) {
            y = {x(0), ~x(1), x(2)};
        } else if (this->partyID == 2) {
            y = {~x(0), x(1), x(2)};
        } else {
            y = x;
        }
    }

    /**
     * @brief Boolean NOT operation for the least significant bit.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void not_b_1(const EVector &x, EVector &y) {
        if (this->partyID == 0) {
            y = {x(0) & 1, x(1) & 1, !(x(2) & 1)};
        } else if (this->partyID == 1) {
            y = {x(0) & 1, !(x(1) & 1), x(2) & 1};
        } else if (this->partyID == 2) {
            y = {!(x(0) & 1), x(1) & 1, x(2) & 1};
        } else {
            y = {x(0) & 1, x(1) & 1, x(2) & 1};
        }
    }

    /**
     * @brief Convert a boolean-shared bit to arithmetic sharing.
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
     * @param x Input boolean shared vector.
     * @param y Output arithmetic shared vector.
     */
    void b2a_bit(const EVector &x, EVector &y) {
        EVector x_prime(x);
        x_prime.mask(1);

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
        // Reusing vectors since inp_a operates on a new vector internally
        auto share0 = inp_a(s0, 1, 0, 2, 3);
        auto share1 = inp_a(s1, 3, 2, 0, 1);

        // precision is 0 automatically

        // XOR: (x-y)^2
        share0 -= share1;
        // y = share0^2
        multiply_a(share0, share0, y);
    }

    /**
     * @brief Redistribute boolean shares.
     *
     * @param x Input vector.
     * @return Pair of redistributed shared vectors.
     */
    std::pair<EVector, EVector> redistribute_shares_b(const EVector &x) {
        auto size = x.size();
        EVector res_1(size), res_2(size);

        // inp(i,j,g,h) algorithm
        // res(0) = r_1, res(1) = res - r_1, res(2) = 0, res(3) = 0
        // inp(0, 3, 1, 2) -- inp(2, 1, 3, 0)
        // shares in 3rd index and random number in 4th index

        // Computation
        if (this->partyID == 0 || this->partyID == 2) {
            auto res_ = x(0) + x(1);

            res_1 = inp_b(res_, 0, 3, 1, 2);
            res_2 = inp_b(res_, 2, 1, 3, 0);

        } else if (this->partyID == 3 || this->partyID == 1) {
            auto res_ = x(1) + x(2);

            res_1 = inp_b(res_, 0, 3, 1, 2);
            res_2 = inp_b(res_, 2, 1, 3, 0);
        }

        return {res_1, res_2};
    }

    /**
     * @brief Reconstruct plaintext from arithmetic shares.
     *
     * @param shares Input shares from all four parties.
     * @return Reconstructed plaintext value.
     */
    Data reconstruct_from_a(const std::vector<Share> &shares) {
        return shares[0][0] + shares[1][0] + shares[2][0] + shares[3][0];
    }

    /**
     * @brief Reconstruct plaintext vector from arithmetic shares.
     *
     * @param shares Input shared vectors from all four parties.
     * @return Reconstructed plaintext vector.
     */
    Vector reconstruct_from_a(const std::vector<EVector> &shares) {
        return shares[0](0) + shares[1](0) + shares[2](0) + shares[3](0);
    }

    /**
     * @brief Reconstruct plaintext from boolean shares.
     *
     * @param shares Input shares from all four parties.
     * @return Reconstructed plaintext value.
     */
    Data reconstruct_from_b(const std::vector<Share> &shares) {
        return shares[0][0] ^ shares[1][0] ^ shares[2][0] ^ shares[3][0];
    }

    /**
     * @brief Reconstruct plaintext vector from boolean shares.
     *
     * @param shares Input shared vectors from all four parties.
     * @return Reconstructed plaintext vector.
     */
    Vector reconstruct_from_b(const std::vector<EVector> &shares) {
        return shares[0](0) ^ shares[1](0) ^ shares[2](0) ^ shares[3](0);
    }

    /**
     * @brief Open arithmetic shares to reveal plaintext.

     * TODO: malicious check
     *
     * @param shares Input shared vector.
     * @return Opened plaintext vector.
     */
    Vector open_shares_a(const EVector &shares) {
        // shares have 0 & 1 & 2 ... go fetch 3
        size_t size = shares.size();
        Vector shares_4(size);
        this->communicator->exchangeShares(shares(0), shares_4, 1, +3, size);
        return shares(0) + shares(1) + shares(2) + shares_4;
    }

    /**
     * @brief Open boolean shares to reveal plaintext.
     *
     * TODO: malicious check
     *
     * @param shares Input shared vector.
     * @return Opened plaintext vector.
     */
    Vector open_shares_b(const EVector &shares) {
        // shares have 0 & 1 & 2 ... go fetch 3
        size_t size = shares.size();
        Vector shares_4(size);
        this->communicator->exchangeShares(shares(0), shares_4, 1, +3, size);
        return shares(0) ^ shares(1) ^ shares(2) ^ shares_4;
    }

    /**
     * @brief Generate arithmetic shares for a single value.
     *
     * @param data Input data value.
     * @return Vector of shares for all parties.
     */
    std::vector<Share> get_share_a(const Data &data) {
        Data share_1, share_2, share_3;
        this->randomnessManager->localPRG->getNext(share_1);
        this->randomnessManager->localPRG->getNext(share_2);
        this->randomnessManager->localPRG->getNext(share_3);
        Data share_4 = data - share_1 - share_2 - share_3;
        return {{share_1, share_2, share_3},
                {share_2, share_3, share_4},
                {share_3, share_4, share_1},
                {share_4, share_1, share_2}};
    }

    /**
     * @brief Generate arithmetic shares for a vector.
     *
     * @param data Input data vector.
     * @return Vector of shares for all parties.
     */
    std::vector<EVector> get_shares_a(const Vector &data) {
        Vector share_1(data.size()), share_2(data.size()), share_3(data.size());
        this->randomnessManager->localPRG->getNext(share_1);
        this->randomnessManager->localPRG->getNext(share_2);
        this->randomnessManager->localPRG->getNext(share_3);
        auto share_4 = data - share_1 - share_2 - share_3;
        return {std::vector<Vector>({share_1, share_2, share_3}),
                std::vector<Vector>({share_2, share_3, share_4}),
                std::vector<Vector>({share_3, share_4, share_1}),
                std::vector<Vector>({share_4, share_1, share_2})};
    }

    /**
     * @brief Generate boolean shares for a single value.
     *
     * @param data Input data value.
     * @return Vector of shares for all parties.
     */
    std::vector<Share> get_share_b(const Data &data) {
        Data share_1, share_2, share_3;
        this->randomnessManager->localPRG->getNext(share_1);
        this->randomnessManager->localPRG->getNext(share_2);
        this->randomnessManager->localPRG->getNext(share_3);
        Data share_4 = data ^ share_1 ^ share_2 ^ share_3;
        return {{share_1, share_2, share_3},
                {share_2, share_3, share_4},
                {share_3, share_4, share_1},
                {share_4, share_1, share_2}};
    }

    /**
     * @brief Generate boolean shares for a vector.
     *
     * @param data Input data vector.
     * @return Vector of shares for all parties.
     */
    std::vector<EVector> get_shares_b(const Vector &data) {
        Vector share_1(data.size()), share_2(data.size()), share_3(data.size());
        this->randomnessManager->localPRG->getNext(share_1);
        this->randomnessManager->localPRG->getNext(share_2);
        this->randomnessManager->localPRG->getNext(share_3);
        auto share_4 = data ^ share_1 ^ share_2 ^ share_3;
        return {std::vector<Vector>({share_1, share_2, share_3}),
                std::vector<Vector>({share_2, share_3, share_4}),
                std::vector<Vector>({share_3, share_4, share_1}),
                std::vector<Vector>({share_4, share_1, share_2})};
    }

    /**
     * @brief Secret share data using the custom 4PC protocol.
     *
     * @param data Input data vector.
     * @param data_party Party ID of the data party.
     * @return Secret shared vector.
     */
    EVector secret_share_b(const Vector &data, const PartyID &data_party = 0) {
        auto size = data.size();
        if (this->partyID == data_party) {
            // Generate shares
            auto boolean_shares = get_shares_b(data);
            // Send first shared vector to the successor
            this->communicator->sendShares(boolean_shares[1](0), this->partyID + 1, size);
            this->communicator->sendShares(boolean_shares[1](1), this->partyID + 1, size);
            this->communicator->sendShares(boolean_shares[1](2), this->partyID + 1, size);
            // Send second shared vector to the successor + 1
            this->communicator->sendShares(boolean_shares[2](0), this->partyID + 2, size);
            this->communicator->sendShares(boolean_shares[2](1), this->partyID + 2, size);
            this->communicator->sendShares(boolean_shares[2](2), this->partyID + 2, size);
            // Send second shared vector to the successor + 2
            this->communicator->sendShares(boolean_shares[3](0), this->partyID + 3, size);
            this->communicator->sendShares(boolean_shares[3](1), this->partyID + 3, size);
            this->communicator->sendShares(boolean_shares[3](2), this->partyID + 3, size);
            return boolean_shares[0];
        } else {
            EVector s(size);
            // Receive second shared vector from the predecessor
            this->communicator->receiveShares(s(0), data_party - this->partyID, size);
            this->communicator->receiveShares(s(1), data_party - this->partyID, size);
            this->communicator->receiveShares(s(2), data_party - this->partyID, size);
            return s;
        }
    }

    /**
     * @brief Secret share data using the custom 4PC protocol.
     *
     * @param data Input data vector.
     * @param data_party Party ID of the data party.
     * @return Secret shared vector.
     */
    EVector secret_share_a(const Vector &data, const PartyID &data_party = 0) {
        auto size = data.size();
        if (this->partyID == data_party) {
            // Generate shares
            auto boolean_shares = get_shares_a(data);
            // Send first shared vector to the successor
            this->communicator->sendShares(boolean_shares[1](0), this->partyID + 1, size);
            this->communicator->sendShares(boolean_shares[1](1), this->partyID + 1, size);
            this->communicator->sendShares(boolean_shares[1](2), this->partyID + 1, size);
            // Send second shared vector to the successor + 1
            this->communicator->sendShares(boolean_shares[2](0), this->partyID + 2, size);
            this->communicator->sendShares(boolean_shares[2](1), this->partyID + 2, size);
            this->communicator->sendShares(boolean_shares[2](2), this->partyID + 2, size);
            // Send second shared vector to the successor + 2
            this->communicator->sendShares(boolean_shares[3](0), this->partyID + 3, size);
            this->communicator->sendShares(boolean_shares[3](1), this->partyID + 3, size);
            this->communicator->sendShares(boolean_shares[3](2), this->partyID + 3, size);
            return boolean_shares[0];
        } else {
            EVector s(size);
            // Receive second shared vector from the predecessor
            this->communicator->receiveShares(s(0), data_party - this->partyID, size);
            this->communicator->receiveShares(s(1), data_party - this->partyID, size);
            this->communicator->receiveShares(s(2), data_party - this->partyID, size);
            return s;
        }
    }

    /**
     * @brief Public sharing of a vector x.
     *
     * P0 gets: (0, 0, 0   )
     * P1 gets: (   0, 0, x)
     * P2 gets: (0,    0, x)
     * P3 gets: (0, 0,    x)
     *
     * @param x Input data vector.
     * @return EVector
     */
    EVector public_share(const Vector &x) {
        auto size = x.size();
        auto zero1 = Vector(size);
        auto zero2 = Vector(size);
        auto zero3 = Vector(size);
        switch (this->partyID) {
            case 0:
                return std::vector<Vector>({zero1, zero2, zero3});
            case 1:
                return std::vector<Vector>({zero2, zero3, x});
            case 2:
                return std::vector<Vector>({zero3, x, zero1});
            case 3:
                return std::vector<Vector>({x, zero1, zero2});
            default:
                throw std::runtime_error("Invalid party ID");
        }
    }

    /**
     * @brief An override version of reshare for malicious security in 4PC.
     *
     * @param v The vector to be rerandomized and reshared.
     * @param group The group performing the resharing.
     * @param binary A flag indicating an arithmetic or binary encoding of the vector.
     */
    void reshare(EVector &v, const std::set<int> group, bool binary) {
        assert(group.size() == 3);

        // find receive party
        int receiver_party = -1;
        for (int i = 0; i < 4; ++i) {
            if (group.find(i) == group.end()) {
                receiver_party = i;  // return the value that is not in the set
                break;
            }
        }

        // calculate the relative rank of the receive party
        int receiver_rel_rank = (receiver_party - this->partyID + 4) % 4;

        // all parties in the group -> generate a zero sharing to rerandomize the vector
        if (receiver_rel_rank != 0) {
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
            for (int i = 0; i < 3; i++) {
                if (binary) {
                    v(i) ^= rand[my_shares[i]];
                } else {
                    v(i) += rand[my_shares[i]];
                }
            }
        }

        /*
        SHARE MAPPING
            rank -3:  i+1, i+2, i-1     -- sends (i+1, i+2)
            rank -2:  i+2, i-1, i       -- sends (i+2, i)
            rank -1:  i-1, i,   i+1     -- sends (i,   i+1)
            receiver: i,   i+1, i+2
        */

        // the three sending parties each send two shares to the receiving party
        if (receiver_rel_rank == 3) {  // rank -3
            // send shares at indices 0 and 1
            this->communicator->sendShares(v(0), receiver_rel_rank, v.size());  // share i+1
            this->communicator->sendShares(v(1), receiver_rel_rank, v.size());  // share i+2
        } else if (receiver_rel_rank == 2) {                                    // rank -2
            // send shares at indices 0 and 2
            this->communicator->sendShares(v(0), receiver_rel_rank, v.size());  // share i+2
            this->communicator->sendShares(v(2), receiver_rel_rank, v.size());  // share i
        } else if (receiver_rel_rank == 1) {                                    // rank -1
            // send shares at indices 1 and 2
            this->communicator->sendShares(v(1), receiver_rel_rank, v.size());  // share i
            this->communicator->sendShares(v(2), receiver_rel_rank, v.size());  // share i+1
        } else if (receiver_rel_rank == 0) {
            // this party is the receiver
            std::vector<Vector> received_shares = {v(0), Vector(v.size()), v(2), Vector(v.size()),
                                                   v(1), Vector(v.size())};
            std::vector<int> sender_ids = {-1, -1, -2, -2, -3, -3};

            this->communicator->receiveBroadcast(received_shares, sender_ids);

            // malicious checks for each received share
            // assert that shares i from parties -1 and -2 match
            malicious_check(received_shares[0], received_shares[3], -1, -2);
            // assert that shares i+1 from parties -3 and -1 match
            malicious_check(received_shares[4], received_shares[1], -3, -1);
            // assert that shares i+2 from parties -2 and -3 match
            malicious_check(received_shares[2], received_shares[5], -2, -3);

            // if we get here, no malicious behavior occurred
        }
    }
};

template <typename Data, typename Share, typename Vector, typename EVector>
int Fantastic_4PC<Data, Share, Vector, EVector>::parties_num = 4;

/**
 * @brief Factory type alias for Fantastic_4PC protocol.
 *
 * @tparam Share Share type template.
 * @tparam Vector Vector type template.
 * @tparam EVector Encoding vector type template.
 */
template <template <typename> class Share, template <typename> class Vector,
          template <typename> class EVector>
using Fantastic_4PC_Factory = DefaultProtocolFactory<Fantastic_4PC, Share, Vector, EVector>;

}  // namespace orq
