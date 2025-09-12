#pragma once

#include "debug/orq_debug.h"
using namespace orq::debug;

namespace orq {
/**
 * @brief A DUMMY protocol which DOES NOTHING. Results WILL be nonsense.
 *
 * @tparam Data Plaintext data type.
 * @tparam Share Replicated share type.
 * @tparam Vector Data container type.
 * @tparam EVector Share container type.
 */
template <typename Data, typename Share, typename Vector, typename EVector>
class Dummy_0PC : public Protocol<Data, Share, Vector, EVector> {
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
     * @brief Constructor for Dummy_0PC protocol.
     *
     * @param _partyID Party identifier.
     * @param _communicator Pointer to communicator (should be null).
     * @param _randomnessManager Pointer to randomness manager.
     */
    Dummy_0PC(PartyID _partyID, Communicator *_communicator,
              random::RandomnessManager *_randomnessManager)
        : Protocol<Data, Share, Vector, EVector>(_communicator, _randomnessManager, _partyID, 1,
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
     * @brief Dummy arithmetic addition (counts operations only).
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector (unused).
     */
    void add_a(const EVector &x, const EVector &y, EVector &z) { op_counter[__func__] += x.size(); }

    /**
     * @brief Dummy arithmetic subtraction (counts operations only).
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector (unused).
     */
    void sub_a(const EVector &x, const EVector &y, EVector &z) { op_counter[__func__] += x.size(); }

    /**
     * @brief Dummy arithmetic multiplication (counts operations and rounds).
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector (unused).
     */
    void multiply_a(const EVector &x, const EVector &y, EVector &z) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
    }

    /**
     * @brief Dummy division by constant (counts operations and rounds).
     *
     * @param x Input vector.
     * @param c Constant divisor.
     * @return Dummy pair of vectors.
     */
    std::pair<EVector, EVector> div_const_a(const EVector &x, const Data &c) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;

        return {x, x};
    }

    /**
     * @brief Get the number of vectors returned by div_const_a.
     *
     * @return Number of result vectors (always 2).
     */
    int div_const_a_count() { return 2; }

    /**
     * @brief Dummy dot product (counts operations and rounds).
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector (unused).
     * @param aggSize Aggregation size.
     */
    void dot_product_a(const EVector &x, const EVector &y, EVector &z, const int &aggSize) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
    }

    /**
     * @brief Dummy bitwise XOR (counts operations only).
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector (unused).
     */
    void xor_b(const EVector &x, const EVector &y, EVector &z) { op_counter[__func__] += x.size(); }

    /**
     * @brief Dummy bitwise AND (counts operations and rounds).
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector (unused).
     */
    void and_b(const EVector &x, const EVector &y, EVector &z) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
    }

    /**
     * @brief Dummy boolean complement (counts operations only).
     *
     * @param x Input vector.
     * @param y Output vector (unused).
     */
    void not_b(const EVector &x, EVector &y) { op_counter[__func__] += x.size(); }

    /**
     * @brief Dummy boolean NOT (counts operations only).
     *
     * @param x Input vector.
     * @param y Output vector (unused).
     */
    void not_b_1(const EVector &x, EVector &y) { op_counter[__func__] += x.size(); }

    /**
     * @brief Dummy less-than-zero comparison (counts operations only).
     *
     * @param x Input vector.
     * @param y Output vector (unused).
     */
    void ltz(const EVector &x, EVector &y) { op_counter[__func__] += x.size(); }

    /**
     * @brief Dummy share redistribution (counts operations and rounds).
     *
     * @param x Input vector.
     * @return Dummy pair of vectors.
     */
    std::pair<EVector, EVector> redistribute_shares_b(const EVector &x) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
        return {x, x};
    }

    /**
     * @brief Dummy boolean-to-arithmetic conversion (counts operations and rounds).
     *
     * @param x Input vector.
     * @param y Output vector (unused).
     */
    void b2a_bit(const EVector &x, EVector &y) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
    }

    /**
     * @brief Dummy reconstruction from arithmetic shares.
     *
     * @param shares Input shares.
     * @return First element of first share.
     */
    Data reconstruct_from_a(const std::vector<Share> &shares) { return shares[0][0]; }

    /**
     * @brief Dummy vectorized reconstruction from arithmetic shares.
     *
     * @param shares Input shared vectors.
     * @return First share's first element.
     */
    Vector reconstruct_from_a(const std::vector<EVector> &shares) { return shares[0](0); }

    /**
     * @brief Dummy reconstruction from boolean shares.
     *
     * @param shares Input shares.
     * @return First element of first share.
     */
    Data reconstruct_from_b(const std::vector<Share> &shares) { return shares[0][0]; }

    /**
     * @brief Dummy vectorized reconstruction from boolean shares.
     *
     * @param shares Input shared vectors.
     * @return First share's first element.
     */
    Vector reconstruct_from_b(const std::vector<EVector> &shares) { return shares[0](0); }

    /**
     * @brief Dummy opening of arithmetic shares (counts operations and rounds).
     *
     * @param shares Input shared vector.
     * @return First share's vector.
     */
    Vector open_shares_a(const EVector &shares) {
        op_counter[__func__] += shares.size();
        round_counter[__func__] += 1;
        return shares(0);
    }

    /**
     * @brief Dummy opening of boolean shares (counts operations and rounds).
     *
     * @param shares Input shared vector.
     * @return First share's vector.
     */
    Vector open_shares_b(const EVector &shares) {
        op_counter[__func__] += shares.size();
        round_counter[__func__] += 1;
        return shares(0);
    }

    /**
     * @brief Generate dummy arithmetic share for single value.
     *
     * @param data Input data value.
     * @return Vector containing the data.
     */
    std::vector<Share> get_share_a(const Data &data) { return {{data}}; }

    /**
     * @brief Generate dummy arithmetic shares for vector.
     *
     * @param data Input data vector.
     * @return Vector of shared vectors.
     */
    std::vector<EVector> get_shares_a(const Vector &data) { return {std::vector<Vector>({data})}; }

    /**
     * @brief Generate dummy boolean share for single value.
     *
     * @param data Input data value.
     * @return Vector containing the data.
     */
    std::vector<Share> get_share_b(const Data &data) { return {{data}}; }

    /**
     * @brief Generate dummy boolean shares for vector.
     *
     * @param data Input data vector.
     * @return Vector of shared vectors.
     */
    std::vector<EVector> get_shares_b(const Vector &data) { return {std::vector<Vector>({data})}; }

    /**
     * @brief Dummy boolean secret sharing (counts operations and rounds).
     *
     * @param data Input data vector.
     * @param data_party Party owning the data.
     * @return Dummy shared vector.
     */
    EVector secret_share_b(const Vector &data, const PartyID &data_party = 0) {
        op_counter[__func__] += data.size();
        round_counter[__func__] += 1;
        return get_shares_b(data)[0];
    }

    /**
     * @brief Dummy arithmetic secret sharing (counts operations and rounds).
     *
     * @param data Input data vector.
     * @param data_party Party owning the data.
     * @return Dummy shared vector.
     */
    EVector secret_share_a(const Vector &data, const PartyID &data_party = 0) {
        op_counter[__func__] += data.size();
        round_counter[__func__] += 1;
        return get_shares_a(data)[0];
    }

    /**
     * @brief Create dummy public share.
     *
     * @param data Input data vector.
     * @return Dummy shared vector.
     */
    EVector public_share(const Vector &data) {
        // doesn't matter if a or b here.
        return get_shares_a(data)[0];
    }

    /**
     * @brief Dummy resharing (counts operations and rounds).
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
 * @brief Factory type alias for Dummy_0PC protocol.
 *
 * @tparam Share Share type template.
 * @tparam Vector Vector type template.
 * @tparam EVector Encoding vector type template.
 */
template <template <typename> class Share, template <typename> class Vector,
          template <typename> class EVector>
using Dummy_0PC_Factory = DefaultProtocolFactory<Dummy_0PC, Share, Vector, EVector>;

}  // namespace orq
