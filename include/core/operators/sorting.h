#pragma once

#include "common.h"
#include "shuffle.h"

// To change the default sort protocol, recompile with this option set
#ifndef DEFAULT_SORT_PROTO
#define DEFAULT_SORT_PROTO QUICKSORT
#endif

namespace orq {

/**
 * @brief Available sorting protocols.
 */
typedef enum {
    BITONICSORT,
    QUICKSORT,
    RADIXSORT,
    BITONICMERGE,
    DEFAULT = DEFAULT_SORT_PROTO
} SortingProtocol;

namespace operators {

    /**
     * @brief Sorting order enumeration.
     */
    enum SortOrder { ASC, DESC };

    // \cond DOXYGEN_IGNORE
    template <typename S, typename E>
    static ElementwisePermutation<E> quicksort(BSharedVector<S, E>& v,
                                               SortOrder order = SortOrder::ASC);

    template <typename S, typename E>
    static ElementwisePermutation<E> radix_sort(
        BSharedVector<S, E>& v, SortOrder order = SortOrder::ASC,
        const size_t bits = std::numeric_limits<std::make_unsigned_t<S>>::digits);
    // \endcond DOXYGEN_IGNORE

    /**
     * @brief Compares two vectors element-wise.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param x_vec Left vector.
     * @param y_vec Right vector.
     * @param order Comparison order.
     * @return Comparison result bits.
     */
    template <typename Share, typename EVector>
    static BSharedVector<Share, EVector> compare(BSharedVector<Share, EVector>& x_vec,
                                                 BSharedVector<Share, EVector>& y_vec,
                                                 const std::vector<SortOrder>& order) {
        std::vector<BSharedVector<Share, EVector>*> x_vec_;
        std::vector<BSharedVector<Share, EVector>*> y_vec_;
        x_vec_.push_back(&x_vec);
        y_vec_.push_back(&y_vec);
        return compare_rows(x_vec_, y_vec_, order);
    }

    /**
     * Compares two `MxN` arrays row-wise by applying `M` greater-than comparisons on `N` keys.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param x_vec The left column-first array with `M` rows and `N` columns.
     * @param y_vec The right column-first array with `M` rows and `N` columns.
     * @param order A vector that denotes the order of comparison per key.
     * @return A new shared vector that contains the result bits of the `M` greater-than
     * comparisons.
     *
     * NOTE: The i-th row, let l, from the left array is greater than the i-th row, let r, from the
     * right array if l's first key is greater than r's first key, or the first keys are the same
     * and l's second key is greater than r's second key, or the first two keys are the same and so
     * forth, for all keys.
     */
    // TODO: use bit compression.
    template <typename Share, typename EVector>
    static BSharedVector<Share, EVector> compare_rows(
        const std::vector<BSharedVector<Share, EVector>*>& x_vec,
        const std::vector<BSharedVector<Share, EVector>*>& y_vec,
        const std::vector<SortOrder>& order) {
        assert((x_vec.size() > 0) && (x_vec.size() == y_vec.size()) &&
               (order.size() == x_vec.size()));
        const int cols_num = x_vec.size();  // Number of keys
        // Compare elements on first key
        BSharedVector<Share, EVector>* t = order[0] == SortOrder::DESC ? y_vec[0] : x_vec[0];
        BSharedVector<Share, EVector>* o = order[0] == SortOrder::DESC ? x_vec[0] : y_vec[0];
        BSharedVector<Share, EVector> eq(t->size());
        BSharedVector<Share, EVector> gt(t->size());
        t->_compare(*o, eq, gt);
        // Compare elements on remaining keys
        for (int i = 1; i < cols_num; ++i) {
            bool invert = order[i] == SortOrder::DESC;
            t = invert ? y_vec[i] : x_vec[i];
            o = invert ? x_vec[i] : y_vec[i];
            BSharedVector<Share, EVector> new_eq(t->size());
            BSharedVector<Share, EVector> new_gt(t->size());
            t->_compare(*o, new_eq, new_gt);
            // Compose 'gt' and `eq` bits
            gt = gt ^ (new_gt & eq);
            eq = eq & new_eq;
        }
        gt.asEVector().mask(1);
        return gt;
    }

    /**
     * Same as above but accepts the `N` columns by reference.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param x_vec The left column-first array with `M` rows and `N` columns.
     * @param y_vec The right column-first array with `M` rows and `N` columns.
     * @param order A vector that denotes the order of comparison per key.
     * @return A new shared vector that contains the result bits of the `M` greater-than
     * comparisons.
     */
    // TODO: x_vec and y_vec should be passed as const
    template <typename Share, typename EVector>
    static BSharedVector<Share, EVector> compare_rows(
        std::vector<BSharedVector<Share, EVector>>& x_vec,
        std::vector<BSharedVector<Share, EVector>>& y_vec, const std::vector<SortOrder>& order) {
        std::vector<BSharedVector<Share, EVector>*> x_vec_;
        std::vector<BSharedVector<Share, EVector>*> y_vec_;
        for (int i = 0; i < x_vec.size(); ++i) {
            x_vec_.push_back(&x_vec[i]);
            y_vec_.push_back(&y_vec[i]);
        }

        return compare_rows(x_vec_, y_vec_, order);
    }

    /**
     * Swaps rows of two `MxN` arrays in place using the provided `bits`.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param x_vec The left column-first array with `M` rows and `N` columns.
     * @param y_vec The right column-first array with `M` rows and `N` columns.
     * @param bits The B-shared vector that contains 'M' bits to use for oblivious swapping (if
     * bits[i]=True, the i-th rows will be swapped).
     */
    template <typename Share, typename EVector>
    static void swap(std::vector<BSharedVector<Share, EVector>*>& x_vec,
                     std::vector<BSharedVector<Share, EVector>*>& y_vec,
                     const BSharedVector<Share, EVector>& bits) {
        // Make sure the input arrays have the same dimensions
        assert((x_vec.size() > 0) && (x_vec.size() == y_vec.size()));
        const int cols_num = x_vec.size();  // Number of columns
        for (int i = 0; i < cols_num; ++i) {
            assert((x_vec[i]->size() == y_vec[i]->size()) && (bits.size() == x_vec[i]->size()));
        }
        // Swap elements
        for (int i = 0; i < cols_num; ++i) {
            auto tmp = multiplex(bits, *x_vec[i], *y_vec[i]);
            *y_vec[i] = multiplex(bits, *y_vec[i], *x_vec[i]);
            *x_vec[i] = tmp;
        }
    }

    /**
     * @brief Swaps rows of two arithmetic arrays using selection bits.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param x_vec Left array.
     * @param y_vec Right array.
     * @param bits Selection bits for swapping.
     */
    template <typename Share, typename EVector>
    static void swap(std::vector<ASharedVector<Share, EVector>*>& x_vec,
                     std::vector<ASharedVector<Share, EVector>*>& y_vec,
                     const ASharedVector<Share, EVector>& bits) {
        // Make sure the input arrays have the same dimensions
        assert((x_vec.size() > 0) && (x_vec.size() == y_vec.size()));
        const int cols_num = x_vec.size();  // Number of columns
        for (int i = 0; i < cols_num; ++i) {
            assert((x_vec[i]->size() == y_vec[i]->size()) && (bits.size() == x_vec[i]->size()));
        }

        // Swap elements
        for (int i = 0; i < cols_num; ++i) {
            auto tmp = multiplex(bits, *x_vec[i], *y_vec[i]);
            *y_vec[i] = multiplex(bits, *y_vec[i], *x_vec[i]);
            *x_vec[i] = tmp;
        }
    }

    /**
     * Same as above but accepts the `N` columns by reference.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param x_vec The left column-first array with `M` rows and `N` columns.
     * @param y_vec The right column-first array with `M` rows and `N` columns.
     * @param bits The B-shared vector that contains 'M' bits to use for oblivious swapping (if
     * bits[i]=True, the i-th rows will be swapped).
     */
    template <typename Share, typename EVector>
    static void swap(std::vector<BSharedVector<Share, EVector>>& x_vec,
                     std::vector<BSharedVector<Share, EVector>>& y_vec,
                     const BSharedVector<Share, EVector>& bits) {
        std::vector<BSharedVector<Share, EVector>*> x_vec_;
        std::vector<BSharedVector<Share, EVector>*> y_vec_;
        for (int i = 0; i < x_vec.size(); ++i) {
            x_vec_.push_back(&x_vec[i]);
            y_vec_.push_back(&y_vec[i]);
        }
        swap(x_vec_, y_vec_, bits);
    }

    /**
     * @brief Swaps arithmetic arrays using selection bits (reference overload).
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param x_vec Left array.
     * @param y_vec Right array.
     * @param bits Selection bits for swapping.
     */
    template <typename Share, typename EVector>
    static void swap(std::vector<ASharedVector<Share, EVector>>& x_vec,
                     std::vector<ASharedVector<Share, EVector>>& y_vec,
                     const ASharedVector<Share, EVector>& bits) {
        std::vector<ASharedVector<Share, EVector>*> x_vec_;
        std::vector<ASharedVector<Share, EVector>*> y_vec_;
        for (int i = 0; i < x_vec.size(); ++i) {
            x_vec_.push_back(&x_vec[i]);
            y_vec_.push_back(&y_vec[i]);
        }
        swap(x_vec_, y_vec_, bits);
    }

    /**
     * Same as above but accepts the `N` columns by reference.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param x_vec The left column-first array with `M` rows and `N` columns.
     * @param y_vec The right column-first array with `M` rows and `N` columns.
     * @param bits The B-shared vector that contains 'M' bits to use for oblivious swapping (if
     * bits[i]=True, the i-th rows will be swapped).
     */
    template <typename Share, typename EVector>
    void swap(BSharedVector<Share, EVector>& x_vec, BSharedVector<Share, EVector>& y_vec,
              BSharedVector<Share, EVector>& bits) {
        bits.mask((Share)1);  // Mask all bits but the LSB
        std::vector<BSharedVector<Share, EVector>*> x_vec_;
        std::vector<BSharedVector<Share, EVector>*> y_vec_;
        x_vec_.push_back(&x_vec);
        y_vec_.push_back(&y_vec);
        swap(x_vec_, y_vec_, bits);
    }

    /**
     * Sorts rows in the given array on all columns. Updates array in place.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param _columns The columns of the array.
     * @param order The sorting direction per column.
     */
    template <typename Share, typename EVector>
    static void bitonic_sort(std::vector<BSharedVector<Share, EVector>*> _columns,
                             std::vector<ASharedVector<Share, EVector>*> _data_a,
                             std::vector<BSharedVector<Share, EVector>*> _data_b,
                             const std::vector<SortOrder>& order) {
        assert(_columns.size() > 0);
        // Vector sizes must be a power of two
        // TODO (john): Modify sorter to support arbitrary vector sizes
        for (int i = 0; i < _columns.size(); ++i)
            assert(ceil(log2(_columns[i]->size())) == floor(log2(_columns[i]->size())));
        // Number of rounds of bitonic sort
        int rounds = (int)log2(_columns[0]->size());
        // For each round
        for (int i = 0; i < rounds; i++) {
            // For each column within a round
            for (int j = 0; j <= i; j++) {
                const int half_box_size = 1 << (i - j);
                const int box_direction_2 = (j == 0) ? -1 : 1;
                // The left (x) and right (y) rows to compare
                std::vector<BSharedVector<Share, EVector>> x;
                std::vector<BSharedVector<Share, EVector>> y;
                for (int k = 0; k < _columns.size(); ++k) {
                    x.push_back(
                        _columns[k]->alternating_subset_reference(half_box_size, half_box_size));
                    if (box_direction_2 == -1) {
                        y.push_back(_columns[k]
                                        ->simple_subset_reference(half_box_size)
                                        .reversed_alternating_subset_reference(half_box_size,
                                                                               half_box_size));
                    } else {
                        y.push_back(
                            _columns[k]
                                ->simple_subset_reference(half_box_size)
                                .alternating_subset_reference(half_box_size, half_box_size));
                    }
                }
                // Compare rows on all columns
                BSharedVector<Share, EVector> bits = compare_rows(x, y, order);

                // Swap rows in place using the comparison bits
                swap(x, y, bits);

                // Sorting Data as well
                if (_data_b.size() > 0) {
                    std::vector<BSharedVector<Share, EVector>> _data_b_1;
                    std::vector<BSharedVector<Share, EVector>> _data_b_2;
                    for (int k = 0; k < _data_b.size(); ++k) {
                        _data_b_1.push_back(
                            _data_b[k]->alternating_subset_reference(half_box_size, half_box_size));
                        if (box_direction_2 == -1) {
                            _data_b_2.push_back(_data_b[k]
                                                    ->simple_subset_reference(half_box_size)
                                                    .reversed_alternating_subset_reference(
                                                        half_box_size, half_box_size));
                        } else {
                            _data_b_2.push_back(
                                _data_b[k]
                                    ->simple_subset_reference(half_box_size)
                                    .alternating_subset_reference(half_box_size, half_box_size));
                        }
                    }
                    swap(_data_b_1, _data_b_2, bits);
                }

                if (_data_a.size() > 0) {
                    ASharedVector<Share, EVector> bits_a = bits.b2a_bit();
                    std::vector<ASharedVector<Share, EVector>> _data_a_1;
                    std::vector<ASharedVector<Share, EVector>> _data_a_2;
                    for (int k = 0; k < _data_a.size(); ++k) {
                        _data_a_1.push_back(
                            _data_a[k]->alternating_subset_reference(half_box_size, half_box_size));
                        if (box_direction_2 == -1) {
                            _data_a_2.push_back(_data_a[k]
                                                    ->simple_subset_reference(half_box_size)
                                                    .reversed_alternating_subset_reference(
                                                        half_box_size, half_box_size));
                        } else {
                            _data_a_2.push_back(
                                _data_a[k]
                                    ->simple_subset_reference(half_box_size)
                                    .alternating_subset_reference(half_box_size, half_box_size));
                        }
                    }
                    swap(_data_a_1, _data_a_2, bits_a);
                }
            }
        }
    }

    /**
     * Same as above but accepts the columns by reference.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param _columns The columns of the array.
     * @param order The sorting direction per column.
     */
    template <typename Share, typename EVector>
    static void bitonic_sort(std::vector<BSharedVector<Share, EVector>> _columns,
                             std::vector<ASharedVector<Share, EVector>> _data_a,
                             std::vector<BSharedVector<Share, EVector>> _data_b,
                             const std::vector<SortOrder>& order) {
        std::vector<BSharedVector<Share, EVector>*> res;
        for (BSharedVector<Share, EVector>& c : _columns) {
            res.push_back(&c);
        }

        std::vector<ASharedVector<Share, EVector>*> _data_a_;
        for (ASharedVector<Share, EVector>& c : _columns) {
            res.push_back(&c);
        }

        std::vector<BSharedVector<Share, EVector>*> _data_b_;
        for (BSharedVector<Share, EVector>& c : _columns) {
            res.push_back(&c);
        }

        bitonic_sort(res, _data_a_, _data_b_, order);
    }

    /**
     * Sorts rows in the given array on all columns. Updates array in place.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param _columns The columns of the array.
     * @param order The sorting direction per column (default ascending).
     */
    template <typename Share, typename EVector>
    static void bitonic_sort(BSharedVector<Share, EVector>& vec, SortOrder order = SortOrder::ASC) {
        std::vector<BSharedVector<Share, EVector>*> res;
        res.push_back(&vec);
        bitonic_sort(res, {}, {}, {order});
    }

    // **************************************** //
    //        New Sorting Functionality         //
    // **************************************** //

    // For int64, pad up to 128. Otherwise use int64_t by default.
    // (We've decided to use 32 bits for padding, so we can support vectors of
    // at most 4B elements. So even an 8 bit vector would need 8+32 = 40 bits
    // of padding.)
    template <typename T>
    using PadWidth =
        typename std::conditional<std::is_same<T, int64_t>::value, __int128_t, int64_t>::type;

    template <typename E>
    using PaddedBSharedVector =
        BSharedVector<PadWidth<typename E::ShareT>,
                      orq::EVector<PadWidth<typename E::ShareT>, E::replicationNumber>>;

    /**
     * Extend <=32 bit elements to 64 bit elements.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param v The input vector with <=32 bits.
     * @param reverse_order Indicates whether the upcoming sort is in reverse order.
     * @return The 64 bit shared vector.
     *
     * Shift original value into most significant 32 bits.
     * Set least significant 32 bits equal to the initial index.
     * If reverse_order is set, pad with the values -1 to -n, otherwise pad with 0 to n-1.
     */
    template <typename Share, typename EVector>
    static PaddedBSharedVector<EVector> pad_input(BSharedVector<Share, EVector>& v,
                                                  bool reverse_order) {
        auto _size = v.size();
        PaddedBSharedVector<EVector> ret(_size);

        orq::Vector<int> idx(_size);
        for (int i = 0; i < _size; i++) {
            idx[i] = reverse_order ? (-1 - i) : i;
        }

        auto k = orq::service::runTime->public_share<EVector::replicationNumber>(idx);

        // copy-cast
        ret = v;
        // Shift actual values up
        ret <<= 32;

        // 32 LSBs is the index vector
        // Due to data type conversion this isn't really doable directly
        for (int j = 0; j < EVector::replicationNumber; j++) {
            for (int i = 0; i < _size; i++) {
                ret.vector(j)[i] |= (uint32_t)k(j)[i];
            }
        }

        if (reverse_order) {
            ret.reverse();
        }

        return ret;
    }

    /**
     * Remove the padding from the 64 bit result to obtain the original <=32 bit
     * values.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param v The original input vector to place the result in.
     * @param padded The 64 bit shared vector.
     * @param reverse_order Indicates whether the upcoming sort is in reverse order.
     * @return The extracted permutation.
     */
    template <typename S, typename E>
    static ElementwisePermutation<E> remove_padding(BSharedVector<S, E>& v,
                                                    PaddedBSharedVector<E>& padded,
                                                    bool reverse_order) {
        ElementwisePermutation<E> permutation(v.size(), Encoding::BShared);

        // Masking is implicit in the type conversion (copy 32 LSBs)
        permutation = padded;

        // Unpad and copy MSBs into result
        padded >>= 32;
        v = padded;

        permutation.b2a();

        if (reverse_order) {
            permutation.negate();
        }

        return permutation;
    }

    /**
     * Sorts rows in the given array on all columns. Updates array in place.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param _columns The columns to sort by.
     * @param _data_a The AShared columns of the array to be sorted.
     * @param _data_b The BShared columns of the array to be sorted.
     * @param single_bit which columns are single-bit columns (thus use 1-bit radixsort)
     * @param protocol which sorting protocol to use
     * @param order The sorting direction per column.
     */
    template <typename Share, typename EVector>
    static void table_sort(std::vector<BSharedVector<Share, EVector>*> _columns,
                           std::vector<ASharedVector<Share, EVector>*> _data_a,
                           std::vector<BSharedVector<Share, EVector>*> _data_b,
                           const std::vector<SortOrder>& order, const std::vector<bool>& single_bit,
                           const SortingProtocol protocol) {
        size_t size = _columns[0]->size();

        int ns = std::count(single_bit.begin(), single_bit.end(), true);
        // number of multibit sort keys
        int nk = _columns.size() - ns;
        // number of data columns
        int nc = _data_a.size() + _data_b.size();
        // bitwidth
        int L = sizeof(Share) * 8;

        // Preallocate perms and pairs

        // See Project Wiki for derivation. At a high level:
        // - Quicksort needs extra perms for shuffle
        // - Multibit radixsort needs one extra pair per bit
        // - 2PC doesn't need perms for b2a, but all other protocols do.
        int perms_required = nk + ns - 1;
        int pairs_required = 4 * ns + 3 * nk + nc - 1;
        if (protocol == SortingProtocol::QUICKSORT) {
            perms_required += nk;
        } else if (protocol == SortingProtocol::RADIXSORT) {
            pairs_required += nk * L;
        }

#ifndef MPC_PROTOCOL_BEAVER_TWO
        // Non-2PC. Note: this is a compile time check because we may add other
        // two-party protocols in the future for which this edge case does not
        // apply...
        perms_required += nk + ns;
#endif

#ifdef INSTRUMENT_TABLES
        single_cout("[TABLE_GENPERM] p=" << perms_required << " n=" << size
                                         << " pairs=" << pairs_required);
#endif
        orq::random::PermutationManager::get()->reserve(size, perms_required, pairs_required);

        // sort subroutine, to pick the right algorithm
        auto sort_sub = [&, protocol](const int sort_col) {
            if (single_bit[sort_col]) {
                // single-bit column; only need to sort 1 bit
                return radix_sort(*(_columns[sort_col]), order[sort_col], 1);
            } else if (protocol == SortingProtocol::QUICKSORT) {
                return quicksort(*(_columns[sort_col]), order[sort_col]);
            } else if (protocol == SortingProtocol::RADIXSORT) {
                return radix_sort(*(_columns[sort_col]), order[sort_col]);
            } else {
                throw std::runtime_error("Unknown table sort protocol");
            }
        };

        int C = nk + ns - 1;

        BSharedVector<Share, EVector> orig(size);

        // Save the original column
        orig = *_columns[C];

        // sort the first column and initialize the sorting permutation
        ElementwisePermutation<EVector> sort_permutation = sort_sub(C);

        // After sorting (...extracting permutation), revert to original
        *_columns[C] = orig;

        // iterate over the sort columns in reverse order
        // starting with the second-to-last
        for (C--; C >= 0; C--) {
            orig = *_columns[C];
            // apply the existing permutation to the column before sorting
            oblivious_apply_elementwise_perm(*_columns[C], sort_permutation);

            // perform the sort and get the next permutation
            ElementwisePermutation<EVector> next_permutation = sort_sub(C);

            // Revert to the original column after getting the perm
            *_columns[C] = orig;

            // compose the next permutation with the existing total permutation to get the new total
            // permutation
            sort_permutation = compose_permutations(sort_permutation, next_permutation);
        }

        // apply sort perm to key columns...
        for (int j = 0; j < _columns.size(); j++) {
            oblivious_apply_elementwise_perm(*_columns[j], sort_permutation);
        }

        // ...all arithmetic columns
        for (auto& a_column : _data_a) {
            oblivious_apply_elementwise_perm(*a_column, sort_permutation);
        }
        // ...and all binary columns
        for (auto& b_column : _data_b) {
            oblivious_apply_elementwise_perm(*b_column, sort_permutation);
        }

        // At this point, should be zero permutations left in the queue
    }
}  // namespace operators
}  // namespace orq
