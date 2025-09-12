#pragma once

#include "common.h"
#include "core/random/permutations/permutation_manager.h"

namespace orq::operators {

template <typename E>
using ASharedPerm = ASharedVector<int, orq::EVector<int, E::replicationNumber>>;
template <typename E>
using BSharedPerm = BSharedVector<int, orq::EVector<int, E::replicationNumber>>;

/**
 * @brief Direct implementation of the AHI+22 radix sort algorithm.
 *
 * @tparam S Share data type.
 * @tparam E Share container type.
 * @param v Vector to sort.
 * @param bits Number of bits to sort on.
 * @param full_width Whether sorting on full bitwidth (affects sign bit handling).
 * @return Permutation representing the sort order.
 */
template <typename S, typename E>
static ElementwisePermutation<E> radix_sort_ccs(BSharedVector<S, E> &v, const int bits,
                                                const bool full_width = true) {
    const size_t n = v.size();

    // need 2 extra permutations for padding/inversion
    orq::random::PermutationManager::get()->reserve(n, bits + 2);

    // Reserve temporaries for gen_bit_perm
    BSharedVector<S, E> v_shift(n);
    BSharedPerm<E> vprime(n);
    ASharedPerm<E> s(2 * n);
    ASharedPerm<E> one = orq::service::runTime->public_share<E::replicationNumber>(
        orq::Vector<int>({1}).repeated_subset_reference(n));

    // Create views of each half of s
    auto s0 = s.slice(0, n);
    auto s1 = s.slice(n);

    // create the global permutation
    ElementwisePermutation<E> total_perm(v.size());

    for (int i = 0; i < bits; i++) {
        // Generate a permutation over the ith bit of v.
        // This was originally a function call, but we need to reuse lots of
        // intermediate storage; best to preallocate outside the loop.

        // shift needs to happen over the (possibly larger) data type.
        v_shift.bit_arithmetic_right_shift(v, i);

        if (full_width && i == bits - 1) {
            // sorting the MSB. flip the sign bit.
            v_shift.inplace_invert();
        }

        // internally call copy-cast operator (data type might not be int)
        vprime = v_shift;

        ASharedPerm<E> f1 = vprime.b2a_bit();

        // Compute f0 := 1 - f1. (However, just reuse s0 storage.)
        s0 = one;
        s0 -= f1;

        // Copy f1 into s1, so we can prefix sum but keep original
        s1 = f1;

        // We want to compute `s.prefix_sum - 1`
        // But instead can just decrement the first element, and then prefix
        // sum; this propagates the -1 to the entire vector without another pass
        s.slice(0, 1) -= 1;
        s.prefix_sum();

        // We want to use f0 and f1 to select the correct values from s0 and s1.
        // - f0 is an indicator vector where f0[i] = 1 if x[i] = 0
        // - f1 is an indicator vector where f1[i] = 1 if x[i] = 1
        // - s0 has the destinations of all 0 values in x (first half of s)
        // - s1 has the destinations of all 1 values in x (second half of s)
        // This is equivalent to:
        //   s0 = (f0 * s0) + (f1 * s1)
        // We can do it with fewer multiplications using the fact that
        // f0 = 1 - f1:
        //   perm = (s0 + f1 * (s1 - s0));
        // (effectively multiplex)
        s1 -= s0;
        f1 *= s1;
        s0 += f1;

        // s0 is the bit perm now.
        ElementwisePermutation<E> bit_perm(s0);

        // apply it to v
        oblivious_apply_elementwise_perm(v, bit_perm);

        // compose the permutations
        total_perm = compose_permutations(total_perm, bit_perm);
    }

    return total_perm;
}
// end AHI+22 radixsort algorithm

/**
 * @brief The radix sort protocol.
 *
 * Implements the radix sort protocol for a given number of bits.
 *
 * @tparam S Share data type.
 * @tparam E Share container type.
 * @param v Vector to sort.
 * @param bits Number of bits to sort on.
 * @param full_width Whether sorting on full bitwidth (affects sign bit handling).
 */
template <typename S, typename E>
static void radix_sort_body(BSharedVector<S, E> &v, const int bits, const bool full_width = true) {
    const size_t n = v.size();

    // need 1 permutation for padding
    int num_permutations = 1;
    if (runTime->getNumParties() == 2) {
        // 2PC doesn't need one for remove_padding, uses direct b2a conversion
        num_permutations -= 1;
    }
    // 1 pair per call to oblivious_apply_elementwise_perm + 1 pair for invert
    int num_pairs = bits + 1;
    orq::random::PermutationManager::get()->reserve(n, num_permutations, num_pairs);

    // Reserve temporaries for gen_bit_perm
    BSharedVector<S, E> v_shift(n);
    BSharedPerm<E> vprime(n);
    ASharedPerm<E> s(2 * n);
    ASharedPerm<E> one = orq::service::runTime->public_share<E::replicationNumber>(
        orq::Vector<int>({1}).repeated_subset_reference(n));

    // Create views of each half of s
    auto s0 = s.slice(0, n);
    auto s1 = s.slice(n);

    for (int i = 0; i < bits; i++) {
        // Generate a permutation over the ith bit of v.
        // This was originally a function call, but we need to reuse lots of
        // intermediate storage; best to preallocate outside the loop.

        // shift needs to happen over the (possibly larger) data type.
        v_shift.bit_arithmetic_right_shift(v, 32 + i);

        if (full_width && i == bits - 1) {
            // sorting the MSB. flip the sign bit.
            v_shift.inplace_invert();
        }

        // internally call copy-cast operator (data type might not be int)
        vprime = v_shift;

        ASharedPerm<E> f1 = vprime.b2a_bit();

        // Compute f0 := 1 - f1. (However, just reuse s0 storage.)
        s0 = one;
        s0 -= f1;

        // Copy f1 into s1, so we can prefix sum but keep original
        s1 = f1;

        // We want to compute `s.prefix_sum - 1`
        // But instead can just decrement the first element, and then prefix
        // sum; this propagates the -1 to the entire vector without another pass
        s.slice(0, 1) -= 1;
        s.prefix_sum();

        // We want to use f0 and f1 to select the correct values from s0 and s1.
        // - f0 is an indicator vector where f0[i] = 1 if x[i] = 0
        // - f1 is an indicator vector where f1[i] = 1 if x[i] = 1
        // - s0 has the destinations of all 0 values in x (first half of s)
        // - s1 has the destinations of all 1 values in x (second half of s)
        // This is equivalent to:
        //   s0 = (f0 * s0) + (f1 * s1)
        // We can do it with fewer multiplications using the fact that
        // f0 = 1 - f1:
        //   perm = (s0 + f1 * (s1 - s0));
        // (effectively multiplex)
        s1 -= s0;
        f1 *= s1;
        s0 += f1;

        // s0 is the bit perm now.
        ElementwisePermutation<E> perm(s0);

        // apply it to v
        oblivious_apply_elementwise_perm(v, perm);
    }
}

/**
 * @brief The radix sort protocol entry point.
 *
 * @tparam S Share data type.
 * @tparam E Share container type.
 * @param v Vector to sort.
 * @param order Sort order (ascending or descending).
 * @param bits Number of bits to sort on.
 * @return Permutation representing the applied sort order.
 */
template <typename S, typename E>
static ElementwisePermutation<E> radix_sort(BSharedVector<S, E> &v, SortOrder order,
                                            const size_t bits) {
    auto reversed = order == SortOrder::DESC;

    // Are we sorting on the full width?
    // If so, sign bit will need to be sorted backwards.
    const bool full_width = (sizeof(S) * 8 == bits);

    // pad the input
    PaddedBSharedVector<E> padded = pad_input(v, reversed);

    radix_sort_body(padded, bits, full_width);

    if (reversed) {
        padded.reverse();
    }

    // unpad the result to obtain the original sorted list and the
    // secret-shared applied permutation
    ElementwisePermutation<E> permutation = remove_padding(v, padded, reversed);
    permutation.invert();

    return permutation;
}
}  // namespace orq::operators
