#ifndef SECRECY_PLAINTEXT_1PC_H
#define SECRECY_PLAINTEXT_1PC_H

#include "../../debug/debug.h"
using namespace secrecy::debug;

#include <algorithm>

namespace secrecy {
/**
 * An INSECURE protocol for testing, benchmarking, and cost models.
 * DO NOT use this protocol for secure computation
 *
 * @tparam Data - Plaintext data type.
 * @tparam Share - Replicated share type.
 * @tparam Vector - Data container type.
 * @tparam EVector - Share container type.
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
     * @param _randomnessManager - A pointer to the randomness manager.
     */
    Plaintext_1PC(PartyID _partyID, Communicator *_communicator,
                  random::RandomnessManager *_randomnessManager)
        :  // NOTE: should be null communicator
          Protocol<Data, Share, Vector, EVector>(_communicator, _randomnessManager, _partyID, 1,
                                                 1) {}

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

    void mark_statistics() {
        for (auto [k, v] : op_counter) {
            mark_op_counter[k] = v;
        }

        for (auto [k, v] : round_counter) {
            mark_round_counter[k] = v;
        }
    }

    void add_a(const EVector &x, const EVector &y, EVector &z) {
        z = x + y;
        op_counter[__func__] += x.size();
    }

    void sub_a(const EVector &x, const EVector &y, EVector &z) {
        z = x - y;
        op_counter[__func__] += x.size();
    }

    void multiply_a(const EVector &x, const EVector &y, EVector &z) {
        z = x * y;
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
    }

    void neg_a(const EVector &x, EVector &y) {
        op_counter[__func__] += x.size();
        y = -x;
    }
    
    std::pair<EVector, EVector> div_const_a(const EVector &x, const Data &c) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
        EVector err(x.size());
        // to handle division correction
        err = err - 1;
        return {x / c, err};
    }

    int div_const_a_count() { return 2; }

    void dot_product_a(const EVector &x, const EVector &y, EVector &z, const int &aggSize) {
        z = x.dot_product(y, aggSize);
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
    }

    void xor_b(const EVector &x, const EVector &y, EVector &z) {
        op_counter[__func__] += x.size();
        z = x ^ y;
    }

    void and_b(const EVector &x, const EVector &y, EVector &z) {
        z = x & y;
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
    }

    void not_b(const EVector &x, EVector &y) {
        op_counter[__func__] += x.size();
        y = ~x;
    }

    void not_b_1(const EVector &x, EVector &y) {
        op_counter[__func__] += x.size();
        y = !x;
    }

    void ltz(const EVector &x, EVector &y) {
        op_counter[__func__] += x.size();
        y = x.ltz();
    }

    void b2a_bit(const EVector &x, EVector &y) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
        y = x & 1;
    }

    std::pair<EVector, EVector> redistribute_shares_b(const EVector &x) {
        op_counter[__func__] += x.size();
        round_counter[__func__] += 1;
        EVector z(x.size());  // zero
        return {x, z};
    }

    Data reconstruct_from_a(const std::vector<Share> &shares) { return shares[0][0]; }

    Vector reconstruct_from_a(const std::vector<EVector> &shares) { return shares[0](0); }

    Data reconstruct_from_b(const std::vector<Share> &shares) { return shares[0][0]; }

    Vector reconstruct_from_b(const std::vector<EVector> &shares) { return shares[0](0); }

    Vector open_shares_a(const EVector &shares) {
        assert(!shares.has_mapping());
        op_counter[__func__] += shares.size();
        round_counter[__func__] += 1;
        return shares(0);
    }

    Vector open_shares_b(const EVector &shares) {
        assert(!shares.has_mapping());
        op_counter[__func__] += shares.size();
        round_counter[__func__] += 1;
        return shares(0);
    }

    std::vector<Share> get_share_a(const Data &data) { return {{data}}; }

    std::vector<EVector> get_shares_a(const Vector &data) { return {std::vector<Vector>({data})}; }

    std::vector<Share> get_share_b(const Data &data) { return {{data}}; }

    std::vector<EVector> get_shares_b(const Vector &data) { return {std::vector<Vector>({data})}; }

    void replicate_shares() {
        // TODO (john): implement this function
        std::cerr << "Method 'replicate_shares()' is not supported by plaintext 1pc." << std::endl;
        exit(-1);
    }

    EVector secret_share_b(const Vector &data, const PartyID &data_party = 0) {
        op_counter[__func__] += data.size();
        round_counter[__func__] += 1;
        return get_shares_b(data)[0];
    }

    EVector secret_share_a(const Vector &data, const PartyID &data_party = 0) {
        op_counter[__func__] += data.size();
        round_counter[__func__] += 1;
        return get_shares_a(data)[0];
    }

    EVector public_share(const Vector &data) {
        // doesn't matter if a or b here.
        return get_shares_a(data)[0];
    }

    void reshare(EVector &v, const std::set<int> group, bool binary) {
        op_counter[__func__] += v.size();
        round_counter[__func__] += 1;
    }
};

template <template <typename> class Share, template <typename> class Vector,
          template <typename> class EVector>
using Plaintext_1PC_Factory = DefaultProtocolFactory<Plaintext_1PC, Share, Vector, EVector>;

}  // namespace secrecy

#endif  // SECRECY_PLAINTEXT_1PC_H
