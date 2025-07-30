#ifndef SECRECY_OPERATORS_QUICKSORT_H
#define SECRECY_OPERATORS_QUICKSORT_H

#include "../../benchmark/stopwatch.h"
#include "common.h"
#include <random>

// #define DEBUG_QUICKSORT

using namespace secrecy::benchmarking;

namespace secrecy::operators {

// start and end are inclusive indices
// between start and end (sublist) and a list of comparison results, swap
// elements based on that
template <typename T, typename E>
void partition(BSharedVector<T, E> &v, Vector<T> &comparisons, int start, int end,
               Vector<long> &pivots) {
    int i = start;
    for (int j = start + 1; j <= end; j++) {
        if (comparisons[j] == 0) {
            // swap element i+1 with element j
            i++;
            for (int k = 0; k < E::replicationNumber; k++) {
                std::swap(v.vector(k)[j], v.vector(k)[i]);
            }
        }
    }

    // swap element i with start
    for (int k = 0; k < E::replicationNumber; k++) {
        std::swap(v.vector(k)[start], v.vector(k)[i]);
    }

    // Set the next pivots
    pivots[i] = i;
    if (i + 1 <= end) {
        pivots[i + 1] = i + 1;
    }
}

template <typename T, typename EVector>
static void quicksort_body(BSharedVector<T, EVector> &v) {
    v.shuffle();

    v.vector.materialize_inplace();

    size_t N = v.size();

#ifdef DEBUG_QUICKSORT
    auto pid = runTime->getPartyID();
    single_cout_nonl("input: ");
    print(v.open(), pid);
#endif
    // Semantically, this should be size_t, but we need to be able to
    // represent -1 canonically.
    Vector<long> pivots(N, -1);
    Vector<long> pivot_temp(N);
    pivots[0] = 0;

    int num_comparisons = 0;
    Vector<T> exp_cmp_plaintext(N);

    // temp is for internal use by `_compare` (specifically storage for `eq`)
    BSharedVector<T, EVector> temp(N);
    BSharedVector<T, EVector> comparisons(N);

    int num_pivots = 1;

#ifdef MPC_PROTOCOL_DUMMY_ZERO
    std::binomial_distribution<> binom(N, 0.5);
    std::random_device rd;
#endif

    while (true) {
        int size = N - num_pivots;

#ifdef DEBUG_QUICKSORT
        single_cout("== QS iter size: " << size << " ==");
#endif

        temp.resize(size);
        comparisons.resize(size);

        // We want to select non-pivot (-1) elements
        auto non_pivots = pivots < 0;

        // copy
        pivot_temp = pivots;
        pivot_temp.prefix_sum(std::max);
        auto pivot_vec = v.mapping_reference(pivot_temp.included_reference(non_pivots));
        auto reduced_vec = v.included_reference(non_pivots);

#ifdef MPC_PROTOCOL_DUMMY_ZERO
        pivot_vec.resize(size);
        reduced_vec.resize(size);
#endif

#ifdef DEBUG_QUICKSORT
        single_cout_nonl("  v ");
        print(v.open(), pid);
        single_cout_nonl(" p= ");
        print(pivots, pid);
        single_cout_nonl(" pv ");
        print(pivot_vec.open(), pid);
        single_cout_nonl(" rv ");
        print(reduced_vec.open(), pid);
#endif

        // pivot_vec <= reduced_vec
#ifdef QUICKSORT_USE_SUBTRACTION_CMP
        auto comparisons = *rca_compare(pivot_vec, reduced_vec);
#else
        reduced_vec._compare(pivot_vec, temp, comparisons);
#endif

        auto cmp_plaintext = comparisons.open();
        num_comparisons += cmp_plaintext.size();

#ifdef DEBUG_QUICKSORT
        single_cout_nonl(" <= ");
        print(cmp_plaintext, pid);
#endif

        // cmp_plaintext[j] == 1 means the element is in the correct
        // location relative to the pivots. == 0 means swap.
        // Pivots should also be set to 1, but we did this on the last
        // iteration inside `partition`.
        exp_cmp_plaintext.included_reference(non_pivots) = cmp_plaintext;

#ifdef DEBUG_QUICKSORT
        single_cout_nonl(" xc ");
        print(exp_cmp_plaintext, pid);
#endif

        int index = -1;
        for (int p = 0; p < pivots.size(); p++) {
            if (pivots[p] < 0) {
                continue;
            }
            if ((p - index) == 1) {
                // adjacent pivots case
                index++;
                continue;
            }
            partition(v, exp_cmp_plaintext, index, p - 1, pivots);
            index = p;
        }

        // Leftover iteration
        partition(v, exp_cmp_plaintext, index, pivots.size() - 1, pivots);

#ifdef DEBUG_QUICKSORT
        single_cout_nonl(" up ");
        print(exp_cmp_plaintext, pid);

#endif

#ifdef MPC_PROTOCOL_DUMMY_ZERO
        binom.param(std::binomial_distribution<>::param_type(num_pivots * 2 / 3 + 1, 1 - (double) num_pivots / N));
        num_pivots += binom(rd);
#else
        // Pivots are stored as non-negative values. Count them.
        num_pivots = std::ranges::count_if(pivots, [](int x) { return x >= 0; });
#endif

#ifdef DEBUG_QUICKSORT
        single_cout_nonl(" v' ");
        print(v.open(), pid);
#endif

        if (num_pivots >= N) {
#ifdef DEBUG_QUICKSORT
            single_cout("Num comparisons: " << num_comparisons);
#endif
            return;
        }
    }
}

// the quicksort entry point which calls the body
template <typename Share, typename EVector>
static ElementwisePermutation<EVector> quicksort(BSharedVector<Share, EVector> &v,
                                                 SortOrder order) {
    // 1 for shuffle, 1 for remove_padding (b2a)
    int num_permutations = 2;
    if (runTime->getNumParties() == 2) {
        // 2PC doesn't need one for remove_padding, uses direct b2a conversion
        num_permutations -= 1;
    }
    // 1 pair for invert (calls obliv_apply_elementwise_perm)
    int num_pairs = 1;
    secrecy::random::PermutationManager::get()->reserve(v.size(), num_permutations, num_pairs);

    auto reversed = order == SortOrder::DESC;

    // pad the input to ensure unique elements
    auto padded = pad_input(v, reversed);

    quicksort_body(padded);

    if (reversed) {
        padded.reverse();
    }

    // unpad the result to obtain the original sorted list and the secret-shared applied
    // permutation
    ElementwisePermutation<EVector> permutation = remove_padding(v, padded, reversed);
    permutation.invert();

    return permutation;
}

}  // namespace secrecy::operators

#endif  // SECRECY_OPERATORS_QUICKSORT_H