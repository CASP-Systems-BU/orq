#pragma once

#include "common.h"
#include "profiling/memory.h"
#include "shuffle.h"

namespace orq {

/**
 * @brief Available sorting protocols.
 */
typedef enum { NETWORK, QUICKSORT, RADIXSORT, BITONICMERGE } SortingProtocol;

// To change the default sort protocol, change Quicksort to something else
constexpr SortingProtocol DEFAULT_SORT_PROTO = CONFIDENTIAL_1PC ? NETWORK : QUICKSORT;

namespace operators {

    /**
     * @brief Sorting order enumeration.
     */
    enum SortOrder { ASC, DESC };

    // \cond DOXYGEN_IGNORE
    template <typename S, typename E>
    static ElementwisePermutation<E> quicksort(BSharedVector<S, E>& v,
                                               SortOrder order = SortOrder::ASC,
                                               bool no_invert = false);

    template <typename S, typename E>
    static ElementwisePermutation<E> radix_sort(
        BSharedVector<S, E>& v, SortOrder order = SortOrder::ASC,
        const size_t bits = std::numeric_limits<std::make_unsigned_t<S>>::digits,
        bool no_invert = false);
    // \endcond DOXYGEN_IGNORE

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
        const std::vector<BSharedVector<Share, EVector>*>& x,
        const std::vector<BSharedVector<Share, EVector>*>& y) {
        assert((x.size() > 0) && (x.size() == y.size()));
        const int cols_num = x.size();      // Number of keys
        const int vec_size = x[0]->size();  // Number of keys

        BSharedVector<Share, EVector> eq(vec_size), gt(vec_size), new_eq(vec_size),
            new_gt(vec_size);

        // Compare elements on first key
        x[0]->_compare(*y[0], eq, gt);

        // Compare elements on remaining keys
        for (int i = 1; i < cols_num; ++i) {
            x[i]->_compare(*y[i], new_eq, new_gt);

            // Compose `gt` and `eq` bits
            new_gt &= eq;
            gt ^= new_gt;
            eq &= new_eq;
        }

        gt.mask(1);
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
            if (order[i] == SortOrder::DESC) {
                x_vec_.push_back(&y_vec[i]);
                y_vec_.push_back(&x_vec[i]);
            } else {
                x_vec_.push_back(&x_vec[i]);
                y_vec_.push_back(&y_vec[i]);
            }
        }

        return compare_rows(x_vec_, y_vec_);
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
    template <typename S, typename E>
    static void swap(std::vector<BSharedVector<S, E>>& x_vec,
                     std::vector<BSharedVector<S, E>>& y_vec, const BSharedVector<S, E>& bits) {
        // Make sure the input arrays have the same dimensions
        assert((x_vec.size() > 0) && (x_vec.size() == y_vec.size()));
        const int cols_num = x_vec.size();

        // Swap elements
        BSharedVector<S, E> bext(bits.size());
        bext.extend_lsb(bits);

        for (int i = 0; i < cols_num; ++i) {
            auto tmp = bext & (x_vec[i] ^ y_vec[i]);
            x_vec[i] ^= tmp;
            y_vec[i] ^= tmp;
        }
    }

    template <typename S, typename E>
    static void swap(std::vector<ASharedVector<S, E>>& x_vec,
                     std::vector<ASharedVector<S, E>>& y_vec, const ASharedVector<S, E>& bits) {
        // Make sure the input arrays have the same dimensions
        assert((x_vec.size() > 0) && (x_vec.size() == y_vec.size()));
        const int cols_num = x_vec.size();

        // Swap elements
        for (int i = 0; i < cols_num; ++i) {
            auto tmp = bits * (x_vec[i] - y_vec[i]);
            x_vec[i] -= tmp;
            y_vec[i] += tmp;
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
    void swap(BSharedVector<Share, EVector>& x_vec, BSharedVector<Share, EVector>& y_vec,
              BSharedVector<Share, EVector>& bits) {
        bits.mask((Share)1);  // Mask all bits but the LSB
        std::vector<BSharedVector<Share, EVector>> x_vec_;
        std::vector<BSharedVector<Share, EVector>> y_vec_;
        x_vec_.push_back(x_vec);
        y_vec_.push_back(y_vec);
        swap(x_vec_, y_vec_, bits);
    }

    /**
     * Sorts rows in the given array on all columns. Updates array in place.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param key The sorting columns.
     * @param order The sorting direction per column.
     */
    template <typename KeyT, typename ADataT, typename BDataT = KeyT>
    static void bitonic_sort(std::vector<KeyT*> keys, std::vector<ADataT*> _data_a,
                             std::vector<BDataT*> _data_b, const std::vector<SortOrder>& order) {
        assert(keys.size() > 0);

        // Vector sizes must be a power of two
        // TODO (john): Modify sorter to support arbitrary vector sizes
        for (int i = 0; i < keys.size(); ++i) {
            ASSERT_POWER_OF_TWO(keys[i]->size());
        }

        // Number of rounds of bitonic sort
        int rounds = (int)log2(keys[0]->size());

        // The left (x) and right (y) rows to compare
        std::vector<KeyT> x;
        std::vector<KeyT> y;

        std::vector<BDataT> data_b1;
        std::vector<BDataT> data_b2;

        std::vector<ADataT> data_a1;
        std::vector<ADataT> data_a2;

        size_t comparisons = 0;

        // For each round
        for (int i = 0; i < rounds; i++) {
            // For each column within a round
            for (int j = 0; j <= i; j++) {
                const int spacing = 1 << (i - j);

                x.clear();
                y.clear();

                for (int k = 0; k < keys.size(); ++k) {
                    x.push_back(keys[k]->alternating_subset_reference(spacing));
                    y.push_back(keys[k]->slice(spacing).alternating_subset_reference(spacing));

                    // TODO: why do we reverse when j = 0?
                    if (j == 0) {
                        y.back().reverse();
                    }
                }

                // Compare rows on all columns
                KeyT k_bits = compare_rows(x, y, order);
                comparisons += x[0].size();
                // Cast down if BDataT != KeyT. TODO: don't do this
                BDataT b_bits(k_bits.size());
                b_bits = k_bits;

                // Swap rows in place using the comparison bits
                swap(x, y, k_bits);

                // Sorting Data as well
                if (_data_b.size() > 0) {
                    data_b1.clear();
                    data_b2.clear();

                    for (int k = 0; k < _data_b.size(); ++k) {
                        data_b1.push_back(_data_b[k]->alternating_subset_reference(spacing));

                        data_b2.push_back(
                            _data_b[k]->slice(spacing).alternating_subset_reference(spacing));

                        if (j == 0) {
                            data_b2.back().reverse();
                        }
                    }
                    swap(data_b1, data_b2, b_bits);
                }

                if (_data_a.size() > 0) {
                    ADataT a_bits = b_bits.b2a_bit();
                    data_a1.clear();
                    data_a2.clear();

                    for (int k = 0; k < _data_a.size(); ++k) {
                        data_a1.push_back(_data_a[k]->alternating_subset_reference(spacing));

                        data_a2.push_back(
                            _data_a[k]->slice(spacing).alternating_subset_reference(spacing));

                        if (j == 0) {
                            data_a2.back().reverse();
                        }
                    }
                    swap(data_a1, data_a2, a_bits);
                }
            }
        }
    }

    /**
     * Sorts rows in the given array on all columns. Updates array in place.
     *
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     * @param _columns The columns of the array.
     * @param order The sorting direction per column (default ascending).
     */
    template <typename S, typename E>
    static void bitonic_sort(BSharedVector<S, E>& vec, SortOrder order = SortOrder::ASC) {
        std::vector<BSharedVector<S, E>*> res;
        res.push_back(&vec);
        bitonic_sort<BSharedVector<S, E>, ASharedVector<S, E>>(res, {}, {}, {order});
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
     * @param convert_to_arithmetic Indicates whether to apply b2a on the permutation or not.
     * @return The extracted permutation.
     */
    template <typename S, typename E>
    static ElementwisePermutation<E> remove_padding(BSharedVector<S, E>& v,
                                                    PaddedBSharedVector<E>& padded,
                                                    bool reverse_order,
                                                    bool convert_to_arithmetic = true) {
        ElementwisePermutation<E> permutation(v.size(), Encoding::BShared);

        // Masking is implicit in the type conversion (copy 32 LSBs)
        permutation = padded;

        // Unpad and copy MSBs into result
        padded >>= 32;
        v = padded;

        // we must convert to arithmetic if we are in reverse order, otherwise leave it as optional
        if (convert_to_arithmetic || reverse_order) {
            permutation.b2a();
        }

        if (reverse_order) {
            permutation.negate();
        }

        return permutation;
    }

    /**
     * @brief Get the permutation counts for the given specification.
     * @tparam Share Share data type.
     * @tparam EVector Share container type.
     *
     * @param _columns The columns to sort by.
     * @param _data_a The AShared columns of the array to be sorted.
     * @param _data_b The BShared columns of the array to be sorted.
     * @param order The sorting direction per column.
     * @param single_bit The single-bit columns.
     * @param protocol The sorting protocol to use.
     * @return The number of permutations and pairs required.
     */
    template <typename Share, typename EVector>
    static std::pair<int, int> get_perm_counts(std::vector<BSharedVector<Share, EVector>*> _columns,
                                               std::vector<ASharedVector<Share, EVector>*> _data_a,
                                               std::vector<BSharedVector<Share, EVector>*> _data_b,
                                               const std::vector<SortOrder>& order,
                                               const std::vector<bool>& single_bit,
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

        return std::make_pair(perms_required, pairs_required);
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

        auto [perms_required, pairs_required] =
            get_perm_counts(_columns, _data_a, _data_b, order, single_bit, protocol);

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

        // number of single-bit sort keys
        int ns = std::count(single_bit.begin(), single_bit.end(), true);
        // number of multibit sort keys
        int nk = _columns.size() - ns;

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

        runTime->malicious_check();

        // At this point, should be zero permutations left in the queue, and the table is sorted
    }
}  // namespace operators
}  // namespace orq
