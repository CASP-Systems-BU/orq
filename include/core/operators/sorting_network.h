#pragma once

#include "sorting.h"

namespace orq::operators {

namespace pairwise {
    /**
     * @brief Defines a single round of the pairwise sorting network.
     *
     * Vectorized comparisons start at `start_left` and `start_right`, and occur in chunks of size
     * `step`.
     *
     */
    struct PairwiseRound {
        size_t start_left;
        size_t start_right;
        size_t step;
    };

    /**
     * @brief Generate a list of comparison operations for the pairwise sorting network. Each round
     * consists of a series of (sliced) alternating subset references. Works with non-power-of-two
     * inputs.
     *
     * @param n_ input size
     * @return std::vector<PairwiseRound>
     */
    std::vector<PairwiseRound> pw_comparisons(size_t n_) {
        auto n_pow_2 = 1 << std::bit_width(n_ - 1);

        std::vector<PairwiseRound> cmp;
        // depth is ~ log squared
        // this doesn't need to be exact; just a performance optimization
        cmp.reserve(n_pow_2 * (n_pow_2 + 1) / 2);

        // First half of the network
        for (size_t a = 1; a < n_pow_2; a <<= 1) {
            if (a < n_) {
                cmp.push_back({.start_left = 0, .start_right = a, .step = a});
            }
        }

        // Second half of the network (merge)
        for (size_t a = n_pow_2 >> 2, e = 1; a > 0 && e < n_pow_2; a >>= 1, e = 2 * e + 1) {
            for (size_t d = e; d > 0; d >>= 1) {
                size_t start_left = a;
                size_t start_right = (d + 1) * a;
                if (start_right < n_) {
                    cmp.push_back(
                        {.start_left = start_left, .start_right = start_right, .step = a});
                }
            }
        }
        return cmp;
    }

}  // namespace pairwise

/**
 * @brief Pairwise sorting network for tables
 *
 * @tparam KeyT Key type
 * @tparam ADataT Arithmetic shared vector type
 * @tparam BDataT Boolean shared vector type
 * @param keys sorting keys
 * @param data_a arithmetic-shared data
 * @param data_b boolean-shared data
 * @param order whether ascending or descending. must be the same length as keys.
 */
template <typename KeyT, typename ADataT, typename BDataT = KeyT>
void pairwise_sort(std::vector<KeyT*> keys, std::vector<ADataT*> data_a,
                   std::vector<BDataT*> data_b, const std::vector<SortOrder>& order) {
    assert(keys.size() > 0);

    assert(keys.size() == order.size());

    size_t N = keys[0]->size();
    auto cmp = pairwise::pw_comparisons(N);

    std::vector<KeyT> x;
    std::vector<KeyT> y;

    std::vector<BDataT> data_b1;
    std::vector<BDataT> data_b2;

    std::vector<ADataT> data_a1;
    std::vector<ADataT> data_a2;

    for (auto c : cmp) {
        x.clear();
        y.clear();

        auto delta = c.start_right - c.start_left;

        for (auto k : keys) {
            // x vectors start before y, so need to make sure the slices are the right length.
            x.push_back(k->slice(c.start_left, N - delta).alternating_subset_reference(c.step));
            y.push_back(k->slice(c.start_right).alternating_subset_reference(c.step));
        }

        KeyT k_bits = compare_rows(x, y, order);

        // Cast down if BDataT != KeyT. TODO: better type handling here
        // For our standard table operations, BDataT == KeyT, but we want to support arbitrary
        // operations
        BDataT b_bits(k_bits.size());
        b_bits = k_bits;

        // Swap rows in place using the comparison bits
        swap(x, y, k_bits);

        // Sorting data as well. Same control bits, just different data
        if (data_b.size() > 0) {
            data_b1.clear();
            data_b2.clear();

            for (auto d : data_b) {
                data_b1.push_back(
                    d->slice(c.start_left, N - delta).alternating_subset_reference(c.step));
                data_b2.push_back(d->slice(c.start_right).alternating_subset_reference(c.step));
            }
            swap(data_b1, data_b2, b_bits);
        }

        if (data_a.size() > 0) {
            ADataT a_bits = b_bits.b2a_bit();
            data_a1.clear();
            data_a2.clear();

            for (auto d : data_a) {
                data_a1.push_back(
                    d->slice(c.start_left, N - delta).alternating_subset_reference(c.step));
                data_a2.push_back(d->slice(c.start_right).alternating_subset_reference(c.step));
            }
            swap(data_a1, data_a2, a_bits);
        }
    }
}

/**
 * @brief Sort a single vector with the pairwise network
 *
 * @tparam S Shared type
 * @tparam E Container type
 * @param vec
 * @param order
 */
template <typename S, typename E>
void pairwise_sort(BSharedVector<S, E>& vec, SortOrder order = SortOrder::ASC) {
    std::vector<BSharedVector<S, E>*> res;
    res.push_back(&vec);
    pairwise_sort<BSharedVector<S, E>, ASharedVector<S, E>>(res, {}, {}, {order});
}
}  // namespace orq::operators