#ifndef SECRECY_BEAVER_2PC_H
#define SECRECY_BEAVER_2PC_H

#include "../../benchmark/stopwatch.h"
#include "protocol_factory.h"

using namespace secrecy::benchmarking;

namespace secrecy {

/**
 * Implements the secure primitives for the 2-party semi-honest protocol that uses Beaver triples.
 * @tparam Data - Plaintext data type.
 * @tparam Share - Share type.
 * @tparam Vector - Data container type.
 * @tparam EVector - Share container type.
 */
template <typename Data, typename Share, typename Vector, typename EVector>
class Beaver_2PC : public Protocol<Data, Share, Vector, EVector> {
   public:
    // Configuration Parameters
    static int parties_num;

    random::BeaverTripleGenerator<Data, secrecy::Encoding::AShared> *BTgen;
    random::BeaverTripleGenerator<Data, secrecy::Encoding::BShared> *BTANDgen;

    Beaver_2PC(PartyID _partyID, Communicator *_communicator,
               random::RandomnessManager *_randomnessManager)
        : Protocol<Data, Share, Vector, EVector>(_communicator, _randomnessManager, _partyID, 2,
                                                 1) {
        BTgen = _randomnessManager->getCorrelation<Data, random::Correlation::BeaverMulTriple>();
        BTANDgen = _randomnessManager->getCorrelation<Data, random::Correlation::BeaverAndTriple>();
    }

    void multiply_a(const EVector &x, const EVector &y, EVector &z) {
        auto [a, b, c] = BTgen->getNext(x.size());

        auto A = open_shares_a(x + a);
        auto B = open_shares_a(y + b);

        z = y * A - a * B + c;
    }

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
     * @brief Perform bitwise AND between two binary secret shared values.
     * Consumes one Beaver AND triple. Direct analog to multiplication.
     *
     * @param x binary shared input
     * @param y binary shared input
     * @param z binary shared output
     */
    void and_b(const EVector &x, const EVector &y, EVector &z) {
        auto [a, b, c] = BTANDgen->getNext(x.size());

        auto A = open_shares_b(x ^ a);
        auto B = open_shares_b(y ^ b);

        z = (y & A) ^ (a & B) ^ c;
    }

    // TODO: check how to apply ~ instead
    void not_b(const EVector &x, EVector &y) {
        if (this->partyID == 0) {
            y = ~x;
        } else {
            y = x;
        }
    }

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
     * TODO: add full-width algorithm, and maybe specialize b2a_bit via that
     * function
     *
     * @param x
     * @return EVector
     */
    void b2a_bit(const EVector &x, EVector &y) {
        // Enforce LSB only
        EVector xm = x & 1;

        if (this->partyID == 1) {
            xm(0) = -xm(0);
        }

        // compute xm^2
        multiply_a(xm, xm, y);
    }

    std::pair<EVector, EVector> redistribute_shares_b(const EVector &x) {
        return {secret_share_b(x(0), 0), secret_share_b(x(0), 1)};
    }

    // Shares Opening without communication
    Data reconstruct_from_a(const std::vector<Share> &shares) {
        return shares[0][0] + shares[1][0];
    }

    Vector reconstruct_from_a(const std::vector<EVector> &shares) {
        return shares[0](0) + shares[1](0);
    }

    Data reconstruct_from_b(const std::vector<Share> &shares) {
        return shares[0][0] ^ shares[1][0];
    }

    Vector reconstruct_from_b(const std::vector<EVector> &shares) {
        return shares[0](0) ^ shares[1](0);
    }

    Vector open_shares_a(const EVector &shares) {
        Vector shares_2(shares(0).size());
        this->communicator->exchangeShares(shares(0), shares_2, 1, shares.size());
        return shares(0) + shares_2;
    }

    Vector open_shares_b(const EVector &shares) {
        Vector shares_2(shares(0).size());
        this->communicator->exchangeShares(shares(0), shares_2, 1, shares.size());
        return shares(0) ^ shares_2;
    }

    // Shares Generation
    std::vector<Share> get_share_a(const Data &data) {
        Data share_1;
        // this->randomGenerator->getNext(share_1);
        Data share_2 = data - share_1;
        return {{share_1}, {share_2}};
    }

    std::vector<EVector> get_shares_a(const Vector &data) {
        Vector share_1(data.size());
        this->randomnessManager->localPRG->getNext(share_1);
        auto share_2 = data - share_1;
        return {std::vector<Vector>({share_1}), std::vector<Vector>({share_2})};
    }

    std::vector<Share> get_share_b(const Data &data) {
        Data share_1;
        // this->randomGenerator->getNext(share_1);
        Data share_2 = data ^ share_1;
        return {{share_1}, {share_2}};
    }

    std::vector<EVector> get_shares_b(const Vector &data) {
        Vector share_1(data.size());
        this->randomnessManager->localPRG->getNext(share_1);
        auto share_2 = data ^ share_1;
        return {std::vector<Vector>({share_1}), std::vector<Vector>({share_2})};
    }

    void replicate_shares() {
        // TODO (john): implement this function
        std::cerr << "Method 'replicate_shares()' is not supported by Beaver_2PC." << std::endl;
        exit(-1);
    }

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

template <template <typename> class Share, template <typename> class Vector,
          template <typename> class EVector>
using Beaver_2PC_Factory = DefaultProtocolFactory<Beaver_2PC, Share, Vector, EVector>;

}  // namespace secrecy

#endif  // SECRECY_BEAVER_2PC_H
