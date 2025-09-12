#pragma once

#include "profiling/stopwatch.h"
#include "protocol_factory.h"

using namespace orq::benchmarking;

namespace orq {

/**
 * @brief Implements the secure primitives for the 2-party semi-honest protocol that uses
 * Beaver triples.
 *
 * @tparam Data Plaintext data type.
 * @tparam Share Share type.
 * @tparam Vector Data container type.
 * @tparam EVector Share container type.
 */
template <typename Data, typename Share, typename Vector, typename EVector>
class Beaver_2PC : public Protocol<Data, Share, Vector, EVector> {
   public:
    // Configuration Parameters
    static int parties_num;

    random::BeaverTripleGenerator<Data, orq::Encoding::AShared> *BTgen;
    random::BeaverTripleGenerator<Data, orq::Encoding::BShared> *BTANDgen;

    /**
     * @brief Constructor for Beaver_2PC protocol.
     *
     * @param _partyID Party identifier.
     * @param _communicator Pointer to communicator.
     * @param _randomnessManager Pointer to randomness manager.
     */
    Beaver_2PC(PartyID _partyID, Communicator *_communicator,
               random::RandomnessManager *_randomnessManager)
        : Protocol<Data, Share, Vector, EVector>(_communicator, _randomnessManager, _partyID, 2,
                                                 1) {
        BTgen = _randomnessManager->getCorrelation<Data, random::Correlation::BeaverMulTriple>();
        BTANDgen = _randomnessManager->getCorrelation<Data, random::Correlation::BeaverAndTriple>();
    }

    /**
     * @brief Secure arithmetic multiplication using Beaver triples.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void multiply_a(const EVector &x, const EVector &y, EVector &z) {
        auto [a, b, c] = BTgen->getNext(x.size());

        auto A = open_shares_a(x + a);
        auto B = open_shares_a(y + b);

        z = y * A - a * B + c;

        this->handle_precision(x, y, z);
        this->truncate(z);
    }

    /**
     * @brief Division by constant with optional error correction.
     *
     * @param x Input vector.
     * @param c Constant divisor.
     * @return Pair of vectors (quotient and error correction).
     */
    std::pair<EVector, EVector> div_const_a(const EVector &x, const Data &c) {
        auto size = x.size();
        EVector res(size), err(size);

#ifdef USE_DIVISION_CORRECTION
        // Get input sign
        auto input_neg = x(0) < 0;

        // Note: it simplier than the 3pc and 4pc because we do not add shares,
        //      then redistribute them again securely.

        // - Remove a c from input and add to remainder if input is negative
        //   This makes sure the remainder is always positive
        // - Then remove a `c` from the remainder so that if it's still positive,
        //   we need to adjust the result during the error correction phase.
        if (this->partyID == 0) {
            res(0) = x(0) / c - input_neg;
            err(0) = x(0) % c + input_neg * c;
        } else {
            res(0) = x(0) / c - input_neg;
            err(0) = x(0) % c + input_neg * c - c;
        }
#else
        res(0) = x(0) / c;
#endif

        return {res, err};
    }

    /**
     * @brief Secure bitwise AND using Beaver AND triples.
     *
     * Performs bitwise AND between two binary secret shared values.
     * Consumes one Beaver AND triple. Direct analog to multiplication.
     *
     * @param x Binary shared input.
     * @param y Binary shared input.
     * @param z Binary shared output.
     */
    void and_b(const EVector &x, const EVector &y, EVector &z) {
        auto [a, b, c] = BTANDgen->getNext(x.size());

        auto A = open_shares_b(x ^ a);
        auto B = open_shares_b(y ^ b);

        z = (y & A) ^ (a & B) ^ c;

        this->handle_precision(x, y, z);
    }

    /**
     * @brief Boolean complement operation.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void not_b(const EVector &x, EVector &y) {
        if (this->partyID == 0) {
            y = ~x;
        } else {
            y = x;
        }
    }

    /**
     * @brief Boolean NOT operation.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void not_b_1(const EVector &x, EVector &y) {
        if (this->partyID == 0) {
            y = !x;
        } else {
            y = x;
        }
    }

    /**
     * @brief Convert a boolean-shared bit in the least significant position
     * to an arithmetic sharing:
     *
     * First, interpret `x = x0 ^ x1` as arithmetic shares. P1 negates its
     * share. Then we have `x' = x0 - x1`, where `x'` is some unknown value.
     * Squaring this value under MPC gives the arithmetic conversion:
     * ```
     * y = x' * x'
     *   = (x0 - x1) * (x0 - x1)
     *   = x0 * x0 + x1 * x1 - 2 * x0 * x1
     *   = x0 + x1 - 2 * x0 * x1
     * ```
     *
     * The last line holds since `a * a == a` for single-bit values. Then
     * we have the arithmetized XOR expression, as required. Since we
     * perform MPC multiplication, the result is already randomized.
     *
     * @param x Input boolean shared vector.
     * @param y Output arithmetic shared vector.
     */
    void b2a_bit(const EVector &x, EVector &y) {
        // Enforce LSB only
        EVector xm = x & 1;
        xm.setPrecision(0);

        if (this->partyID == 1) {
            xm(0) = -xm(0);
        }

        // compute xm^2
        multiply_a(xm, xm, y);
    }

    /**
     * @brief Redistribute boolean shares.
     *
     * @param x Input vector.
     * @return Pair of redistributed shared vectors.
     */
    std::pair<EVector, EVector> redistribute_shares_b(const EVector &x) {
        return {secret_share_b(x(0), 0), secret_share_b(x(0), 1)};
    }

    /**
     * @brief Reconstruct plaintext from arithmetic shares.
     *
     * @param shares Input shares from both parties.
     * @return Reconstructed plaintext value.
     */
    Data reconstruct_from_a(const std::vector<Share> &shares) {
        return shares[0][0] + shares[1][0];
    }

    /**
     * @brief Reconstruct plaintext vector from arithmetic shares.
     *
     * @param shares Input shared vectors from both parties.
     * @return Reconstructed plaintext vector.
     */
    Vector reconstruct_from_a(const std::vector<EVector> &shares) {
        return shares[0](0) + shares[1](0);
    }

    /**
     * @brief Reconstruct plaintext from boolean shares.
     *
     * @param shares Input shares from both parties.
     * @return Reconstructed plaintext value.
     */
    Data reconstruct_from_b(const std::vector<Share> &shares) {
        return shares[0][0] ^ shares[1][0];
    }

    /**
     * @brief Reconstruct plaintext vector from boolean shares.
     *
     * @param shares Input shared vectors from both parties.
     * @return Reconstructed plaintext vector.
     */
    Vector reconstruct_from_b(const std::vector<EVector> &shares) {
        return shares[0](0) ^ shares[1](0);
    }

    /**
     * @brief Open arithmetic shares to reveal plaintext.
     *
     * @param shares Input shared vector.
     * @return Opened plaintext vector.
     */
    Vector open_shares_a(const EVector &shares) {
        Vector shares_2(shares(0).size());
        this->communicator->exchangeShares(shares(0), shares_2, 1, shares.size());
        return shares(0) + shares_2;
    }

    /**
     * @brief Open boolean shares to reveal plaintext.
     *
     * @param shares Input shared vector.
     * @return Opened plaintext vector.
     */
    Vector open_shares_b(const EVector &shares) {
        Vector shares_2(shares(0).size());
        this->communicator->exchangeShares(shares(0), shares_2, 1, shares.size());
        return shares(0) ^ shares_2;
    }

    /**
     * @brief Generate arithmetic shares for a single value.
     *
     * @param data Input plaintext value.
     * @return Vector of arithmetic shares for both parties.
     */
    std::vector<Share> get_share_a(const Data &data) {
        Data share_1;
        // this->randomGenerator->getNext(share_1);
        Data share_2 = data - share_1;
        return {{share_1}, {share_2}};
    }

    /**
     * @brief Generate arithmetic shares for a vector.
     *
     * @param data Input plaintext vector.
     * @return Vector of arithmetic shared vectors for both parties.
     */
    std::vector<EVector> get_shares_a(const Vector &data) {
        Vector share_1(data.size());
        this->randomnessManager->localPRG->getNext(share_1);
        auto share_2 = data - share_1;
        return {std::vector<Vector>({share_1}), std::vector<Vector>({share_2})};
    }

    /**
     * @brief Generate boolean shares for a single value.
     *
     * @param data Input plaintext value.
     * @return Vector of boolean shares for both parties.
     */
    std::vector<Share> get_share_b(const Data &data) {
        Data share_1;
        // this->randomGenerator->getNext(share_1);
        Data share_2 = data ^ share_1;
        return {{share_1}, {share_2}};
    }

    /**
     * @brief Generate boolean shares for a vector.
     *
     * @param data Input plaintext vector.
     * @return Vector of boolean shared vectors for both parties.
     */
    std::vector<EVector> get_shares_b(const Vector &data) {
        Vector share_1(data.size());
        this->randomnessManager->localPRG->getNext(share_1);
        auto share_2 = data ^ share_1;
        return {std::vector<Vector>({share_1}), std::vector<Vector>({share_2})};
    }

    /**
     * @brief Secret share a boolean vector.
     *
     * @param data Input plaintext vector.
     * @param data_party Party that owns the data.
     * @return This party's boolean shared vector.
     */
    EVector secret_share_b(const Vector &data, const PartyID &data_party = 0) {
        auto size = data.size();
        if (this->partyID == data_party) {
            auto boolean_shares = get_shares_b(data);

            this->communicator->sendShares(boolean_shares[1](0), 1, size);
            return boolean_shares[0];
        } else {
            EVector s(size);
            this->communicator->receiveShares(s(0), -1, size);
            return s;
        }
    }

    /**
     * @brief Secret share an arithmetic vector.
     *
     * @param data Input plaintext vector.
     * @param data_party Party that owns the data.
     * @return This party's arithmetic shared vector.
     */
    EVector secret_share_a(const Vector &data, const PartyID &data_party = 0) {
        auto size = data.size();
        if (this->partyID == data_party) {
            auto arith_shares = get_shares_a(data);

            this->communicator->sendShares(arith_shares[1](0), 1, size);
            return arith_shares[0];
        } else {
            EVector s(size);
            this->communicator->receiveShares(s(0), -1, size);
            return s;
        }
    }

    /**
     * @brief Create public shares from plaintext data.
     *
     * @param data Input plaintext vector.
     * @return Public shared vector.
     */
    EVector public_share(const Vector &data) {
        if (this->partyID == 0) {
            // The public data
            return std::vector<Vector>({data});
        } else {
            // Vector of zeros
            return std::vector<Vector>({Vector(data.size())});
        }
    }
};

template <typename Data, typename Share, typename Vector, typename EVector>
int Beaver_2PC<Data, Share, Vector, EVector>::parties_num = 2;

/**
 * @brief Factory type alias for Beaver_2PC protocol.
 *
 * @tparam Share Share type template.
 * @tparam Vector Vector type template.
 * @tparam EVector Encoding vector type template.
 */
template <template <typename> class Share, template <typename> class Vector,
          template <typename> class EVector>
using Beaver_2PC_Factory = DefaultProtocolFactory<Beaver_2PC, Share, Vector, EVector>;

}  // namespace orq
