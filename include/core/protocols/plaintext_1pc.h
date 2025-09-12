#pragma once

#include "debug/orq_debug.h"
using namespace orq::debug;

#include <algorithm>

namespace orq {
/**
 * @brief An INSECURE protocol for testing and benchmarking.
 *
 * DO NOT use this protocol for secure computation.
 *
 * @tparam Data Plaintext data type.
 * @tparam Share Replicated share type.
 * @tparam Vector Data container type.
 * @tparam EVector Share container type.
 */
template <typename Data, typename Share, typename Vector, typename EVector>
class Plaintext_1PC : public Protocol<Data, Share, Vector, EVector> {
   public:
    // Configuration Parameters
    int parties_num = 1;

    std::map<std::string, uint64_t> op_counter;
    std::map<std::string, std::optional<uint64_t>> mark_op_counter;

    // In principle, certain ops may cost more than one round, but that's
    // implementation-dependent.
    std::map<std::string, uint64_t> round_counter;
    std::map<std::string, std::optional<uint64_t>> mark_round_counter;

    /**
     * @brief Constructor for Plaintext_1PC protocol.
     *
     * @param _partyID Party identifier.
     * @param _communicator Pointer to communicator (should be null).
     * @param _randomnessManager Pointer to randomness manager.
     */
    Plaintext_1PC(PartyID _partyID, Communicator *_communicator,
                  random::RandomnessManager *_randomnessManager)
        :  // NOTE: should be null communicator
          Protocol<Data, Share, Vector, EVector>(_communicator, _randomnessManager, _partyID, 1,
                                                 1) {}

    /**
     * @brief Print protocol statistics including operation and round counts.
     */
    void print_statistics() {
#ifdef PRINT_PROTOCOL_STATISTICS

        const std::string _spacer = " ";
        const int spacing = _spacer.size();

        // calculate auto sizing
        uint64_t label_width = std::max_element(op_counter.begin(), op_counter.end(),
                                                [](const std::pair<std::string, uint64_t> &a,
                                                   const std::pair<std::string, uint64_t> &b) {
                                                    return a.first.size() < b.first.size();
                                                })
                                   ->first.size();
        ;

        uint64_t max_count = std::max_element(op_counter.begin(), op_counter.end(),
                                              [](const std::pair<std::string, uint64_t> &a,
                                                 const std::pair<std::string, uint64_t> &b) {
                                                  return a.second < b.second;
                                              })
                                 ->second;

        auto bits = std::numeric_limits<std::make_unsigned_t<Data>>::digits;

        uint64_t max_count_width = std::ceil(std::log10(max_count * bits));

        uint64_t val_width = max_count_width + spacing;
        uint64_t total_width =
            std::max<uint64_t>(32ULL, label_width + 5 * spacing + val_width * 3 + 1);

        std::cout << "\n" << std::string(total_width, '=') << "\n";
        std::cout << "Op Counts (" << bits << "b):  [rel|total|total bits]\n";

        for (auto [k, v] : op_counter) {
            auto vr = v - mark_op_counter[k].value_or(0);
            auto tb = v * bits;
            std::cout << _spacer << std::setw(label_width) << std::left << k;
            std::cout << _spacer << std::setw(val_width) << std::right << vr;
            std::cout << _spacer << "|" << std::setw(val_width) << v;
            std::cout << _spacer << "|" << std::setw(val_width) << tb << "\n";
        }

        std::cout << "\nRound Counts:\n";
        uint32_t overall_rounds = 0;
        uint32_t relative_rounds = 0;
        for (auto [k, v] : round_counter) {
            auto vr = v - mark_round_counter[k].value_or(0);
            std::cout << _spacer << std::setw(label_width) << std::left << k;
            std::cout << _spacer << std::setw(val_width) << std::right << vr;
            std::cout << _spacer << "| " << std::setw(val_width) << v << "\n";

            overall_rounds += v;
            relative_rounds += vr;
        }
        std::cout << "Total rounds: " << relative_rounds << " (" << overall_rounds << " overall)\n";
        std::cout << std::string(total_width, '=') << "\n\n";

#endif
    }

    /**
     * @brief Mark current statistics for relative measurements.
     */
    void mark_statistics() {
        for (auto [k, v] : op_counter) {
            mark_op_counter[k] = v;
        }

        for (auto [k, v] : round_counter) {
            mark_round_counter[k] = v;
        }
    }

    /**
     * @brief Plaintext arithmetic addition.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void add_a(const EVector &x, const EVector &y, EVector &z) {
        z = x + y;
        op_counter[__func__] += x.size();
    }

    /**
     * @brief Plaintext arithmetic subtraction.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void sub_a(const EVector &x, const EVector &y, EVector &z) {
        z = x - y;
        op_counter[__func__] += x.size();
    }

    /**
     * @brief Plaintext arithmetic multiplication.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void multiply_a(const EVector &x, const EVector &y, EVector &z) {
        z = x * y;
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;

        // make sure the precisions line up
        if (x.getPrecision() != y.getPrecision()) {
            throw std::runtime_error("Precision mismatch between multiplication inputs");
        }

        // perform truncation
        if (x.getPrecision() > 0) {
            z.matchPrecision(x);
            this->truncate(z);
        }
    }

    void truncate(EVector &x) { x(0) /= (1 << x.getPrecision()); }

    /**
     * @brief Plaintext arithmetic negation.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void neg_a(const EVector &x, EVector &y) {
        op_counter[__func__] += x.size();
        y = -x;
    }

    /**
     * @brief Plaintext division by constant with error correction.
     *
     * @param x Input vector.
     * @param c Constant divisor.
     * @return Pair of vectors (quotient and error correction).
     */
    std::pair<EVector, EVector> div_const_a(const EVector &x, const Data &c) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
        EVector err(x.size());
        // to handle division correction
        err = err - 1;
        return {x / c, err};
    }

    /**
     * @brief Get the number of vectors returned by div_const_a.
     *
     * @return Number of result vectors (always 2).
     */
    int div_const_a_count() { return 2; }

    /**
     * @brief Plaintext dot product.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     * @param aggSize Aggregation size.
     */
    void dot_product_a(const EVector &x, const EVector &y, EVector &z, const int &aggSize) {
        z = x.dot_product(y, aggSize);
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
    }

    /**
     * @brief Plaintext bitwise XOR.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void xor_b(const EVector &x, const EVector &y, EVector &z) {
        op_counter[__func__] += x.size();
        z = x ^ y;
    }

    /**
     * @brief Plaintext bitwise AND.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void and_b(const EVector &x, const EVector &y, EVector &z) {
        z = x & y;
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
    }

    /**
     * @brief Plaintext boolean complement.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void not_b(const EVector &x, EVector &y) {
        op_counter[__func__] += x.size();
        y = ~x;
    }

    /**
     * @brief Plaintext boolean NOT.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void not_b_1(const EVector &x, EVector &y) {
        op_counter[__func__] += x.size();
        y = !x;
    }

    /**
     * @brief Plaintext less-than-zero comparison.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void ltz(const EVector &x, EVector &y) {
        op_counter[__func__] += x.size();
        y = x.ltz();
    }

    /**
     * @brief Plaintext boolean-to-arithmetic conversion.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void b2a_bit(const EVector &x, EVector &y) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
        y = x & 1;
    }

    /**
     * @brief Plaintext share redistribution.
     *
     * @param x Input vector.
     * @return Pair of vectors (original and zero).
     */
    std::pair<EVector, EVector> redistribute_shares_b(const EVector &x) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
        EVector z(x.size());  // zero
        return {x, z};
    }

    /**
     * @brief Plaintext reconstruction from arithmetic shares.
     *
     * @param shares Input shares.
     * @return First element of first share.
     */
    Data reconstruct_from_a(const std::vector<Share> &shares) { return shares[0][0]; }

    /**
     * @brief Plaintext vectorized reconstruction from arithmetic shares.
     *
     * @param shares Input shared vectors.
     * @return First share's first element.
     */
    Vector reconstruct_from_a(const std::vector<EVector> &shares) { return shares[0](0); }

    /**
     * @brief Plaintext reconstruction from boolean shares.
     *
     * @param shares Input shares.
     * @return First element of first share.
     */
    Data reconstruct_from_b(const std::vector<Share> &shares) { return shares[0][0]; }

    /**
     * @brief Plaintext vectorized reconstruction from boolean shares.
     *
     * @param shares Input shared vectors.
     * @return First share's first element.
     */
    Vector reconstruct_from_b(const std::vector<EVector> &shares) { return shares[0](0); }

    /**
     * @brief Plaintext opening of arithmetic shares.
     *
     * @param shares Input shared vector.
     * @return First share's vector.
     */
    Vector open_shares_a(const EVector &shares) {
        assert(!shares.has_mapping());
        op_counter[__func__] += shares.size();
        round_counter[__func__] += 1;
        return shares(0);
    }

    /**
     * @brief Plaintext opening of boolean shares.
     *
     * @param shares Input shared vector.
     * @return First share's vector.
     */
    Vector open_shares_b(const EVector &shares) {
        assert(!shares.has_mapping());
        op_counter[__func__] += shares.size();
        round_counter[__func__] += 1;
        return shares(0);
    }

    /**
     * @brief Generate plaintext arithmetic share for single value.
     *
     * @param data Input data value.
     * @return Vector containing the data.
     */
    std::vector<Share> get_share_a(const Data &data) { return {{data}}; }

    /**
     * @brief Generate plaintext arithmetic shares for vector.
     *
     * @param data Input data vector.
     * @return Vector of shared vectors.
     */
    std::vector<EVector> get_shares_a(const Vector &data) { return {std::vector<Vector>({data})}; }

    /**
     * @brief Generate plaintext boolean share for single value.
     *
     * @param data Input data value.
     * @return Vector containing the data.
     */
    std::vector<Share> get_share_b(const Data &data) { return {{data}}; }

    /**
     * @brief Generate plaintext boolean shares for vector.
     *
     * @param data Input data vector.
     * @return Vector of shared vectors.
     */
    std::vector<EVector> get_shares_b(const Vector &data) { return {std::vector<Vector>({data})}; }

    /**
     * @brief Plaintext boolean secret sharing.
     *
     * @param data Input data vector.
     * @param data_party Party owning the data.
     * @return Shared vector.
     */
    EVector secret_share_b(const Vector &data, const PartyID &data_party = 0) {
        op_counter[__func__] += data.size();
        round_counter[__func__] += 1;
        return get_shares_b(data)[0];
    }

    /**
     * @brief Plaintext arithmetic secret sharing.
     *
     * @param data Input data vector.
     * @param data_party Party owning the data.
     * @return Shared vector.
     */
    EVector secret_share_a(const Vector &data, const PartyID &data_party = 0) {
        op_counter[__func__] += data.size();
        round_counter[__func__] += 1;
        return get_shares_a(data)[0];
    }

    /**
     * @brief Create plaintext public share.
     *
     * @param data Input data vector.
     * @return Shared vector.
     */
    EVector public_share(const Vector &data) {
        // doesn't matter if a or b here.
        return get_shares_a(data)[0];
    }

    /**
     * @brief Plaintext resharing (counts operations and rounds).
     *
     * @param v Input/output vector.
     * @param group Party group.
     * @param binary Whether shares are binary.
     */
    void reshare(EVector &v, const std::set<int> group, bool binary) {
        op_counter[__func__] += v.size();
        round_counter[__func__] += 1;
    }
};

/**
 * @brief Factory type alias for Plaintext_1PC protocol.
 *
 * @tparam Share Share type template.
 * @tparam Vector Vector type template.
 * @tparam EVector Encoding vector type template.
 */
template <template <typename> class Share, template <typename> class Vector,
          template <typename> class EVector>
using Plaintext_1PC_Factory = DefaultProtocolFactory<Plaintext_1PC, Share, Vector, EVector>;

}  // namespace orq
