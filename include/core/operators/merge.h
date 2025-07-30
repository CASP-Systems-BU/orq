#ifndef SECRECY_OPERATORS_MERGE_H
#define SECRECY_OPERATORS_MERGE_H

#include "common.h"

namespace secrecy::operators {

template <typename Share, typename EVector>
static void compare_swap(BSharedVector<Share, EVector> &x, BSharedVector<Share, EVector> &y) {
    BSharedVector<Share, EVector> comparisons = (x >= y);
    BSharedVector<Share, EVector> temp(x.size());
    temp = multiplex(comparisons, x, y);
    y = multiplex(comparisons, y, x);
    x = temp;
}

// **************************************** //
//              Odd Even Merge              //
// **************************************** //

template <typename Share, typename EVector>
static void odd_even_merge(BSharedVector<Share, EVector> &v) {
    int pID = secrecy::service::runTime->getPartyID();
    size_t n = v.size();
    size_t d = n / 2;

    BSharedVector<Share, EVector> c1 = v.slice(0, n - d);
    BSharedVector<Share, EVector> c2 = v.slice(n - d, n);
    compare_swap(c1, c2);

    int k = 1;
    for (d = n >> 2; d >= 1; d >>= 1) {
        BSharedVector<Share, EVector> c3 = v.slice(d, n - d).alternating_subset_reference(d, d);
        BSharedVector<Share, EVector> c4 = v.slice(d * 2, n).alternating_subset_reference(d, d);
        compare_swap(c3, c4);
    }
}


// **************************************** //
//               Bitonic Merge              //
// **************************************** //

namespace{

    /**
     * Sorts an std::vector that has two halves,
     * one in ascending order and the other in descending order.
     * Note: Plain text bitonic merge for implementation reference.
     *
     * @param arr - The array to merge.
     * @param low - The starting index of the array.
     * @param cnt - The number of elements in the array.
     * @param ascending - A boolean indicating whether to sort in ascending order.
     */
    void bitonicMergePlainText(std::vector<int>& arr, int low, int cnt, bool ascending) {
        if (cnt <= 1) return;
    
        int k = cnt / 2;
        for (int i = low; i < low + k; ++i) {
            if (ascending == (arr[i] > arr[i + k])) {
                std::swap(arr[i], arr[i + k]);
            }
        }
    
        bitonicMergePlainText(arr, low, k, ascending);
        bitonicMergePlainText(arr, low + k, k, ascending);
    }
    
    /**
     * Merges two sorted vectors of an std::vector into a single sorted std::vector.
     * Note: Plain text bitonic merge for implementation reference.
     * 
     * @param v1 - The first half of the vector (ascending).
     * @param v2 - The second half of the vector (descending).
     * @return A merged vector in ascending order.
     */
    std::vector<int> bitonicMergePlainText(const std::vector<int>& v1, const std::vector<int>& v2) {
        std::vector<int> merged = v1;
        merged.insert(merged.end(), v2.rbegin(), v2.rend()); // Reverse v2 to make it decreasing -> increasing
    
        bitonicMergePlainText(merged, 0, merged.size(), true); // Merge in ascending order
        return merged;
    }


    /** Reverses the second half of the columns and data vectors.
     * This is a helper function for bitonic_merge.
     *
     * @param _columns - The columns to reverse.
     * @param _data_a - The first data vector to reverse.
     * @param _data_b - The second data vector to reverse.
     */
    template<typename Share, typename EVector>
    static void bitonic_merge_reverse_second_half(
        std::vector<BSharedVector<Share, EVector>*> _columns,
        std::vector<ASharedVector<Share, EVector>*> _data_a,
        std::vector<BSharedVector<Share, EVector>*> _data_b){

        const size_t rows = _columns[0]->size();
        const size_t half = rows / 2;
        
        for (int k = 0; k <_columns.size(); ++k){
            _columns[k]->slice(half, rows).reverse();
        }

        for(int k = 0; k < _data_b.size(); ++k){
            _data_b[k]->slice(half, rows).reverse();
        }

        for(int k = 0; k < _data_a.size(); ++k){
            _data_a[k]->slice(half, rows).reverse();
        }
    }
} // namespace

    /**
     * Sorts vectors based on some keys.
     * Each key needs to have two already sorted halves in the same order.
     *
     * @param _columns - The keys to merge based on.
     * @param _data_a - The AShared data vectors to merge.
     * @param _data_b - The BShared data vectors to merge.
     * @param order - The sorting direction per column.
     */
    template<typename Share, typename EVector>
    static void bitonic_merge(std::vector<BSharedVector<Share, EVector>*> _columns,
                             std::vector<ASharedVector<Share, EVector>*> _data_a,
                             std::vector<BSharedVector<Share, EVector>*> _data_b,
                             const std::vector<SortOrder> &order) {
        assert(_columns.size() > 0);

        // Vector sizes must be a power of two
        // TODO (john): Modify sorter to support arbitrary vector sizes
        for(int i = 0; i < _columns.size(); ++i)
            assert(ceil(log2(_columns[i]->size())) == floor(log2(_columns[i]->size())));


        // For ascending, the function expects both halves to be in ascending order.
        // However, the second half needs to be in descending order for the bitonic merge.
        bitonic_merge_reverse_second_half(_columns, _data_a, _data_b);
 
        const size_t rows = _columns[0]->size();
        int distance = rows;
        int rounds = (int) log2(rows);
        for(int i = 0; i < rounds; ++i){
            distance /= 2;

            std::vector<BSharedVector<Share, EVector>> x;
            std::vector<BSharedVector<Share, EVector>> y;
            for (int k = 0; k <_columns.size(); ++k){
                x.push_back(_columns[k]->alternating_subset_reference(distance, distance));
                y.push_back(_columns[k]->slice(distance, rows).alternating_subset_reference(distance, distance));
            }

            // Compare rows on all columns
            BSharedVector<Share, EVector> bits = compare_rows(x,y, order);

            // Swap rows in place using the comparison bits
            swap(x,y,bits);

            // Sorting Data as well
            if(_data_b.size() > 0){
                std::vector<BSharedVector<Share, EVector>> _data_b_1;
                std::vector<BSharedVector<Share, EVector>> _data_b_2;
                for(int k = 0; k < _data_b.size(); ++k){
                    _data_b_1.push_back(_data_b[k]->alternating_subset_reference(distance, distance));
                    _data_b_2.push_back(_data_b[k]->slice(distance, rows).alternating_subset_reference(distance, distance));
                }
                swap(_data_b_1, _data_b_2, bits);
            }

            if(_data_a.size() > 0){
                ASharedVector<Share, EVector> bits_a = bits.b2a_bit();
                std::vector<ASharedVector<Share, EVector>> _data_a_1;
                std::vector<ASharedVector<Share, EVector>> _data_a_2;
                for(int k = 0; k < _data_a.size(); ++k){
                    _data_a_1.push_back(_data_a[k]->alternating_subset_reference(distance, distance));
                    _data_a_2.push_back(_data_a[k]->slice(distance, rows).alternating_subset_reference(distance, distance));
                }
                swap(_data_a_1, _data_a_2, bits_a);
            }
        }
    }

    /**
     * Sorts vectors based on some keys.
     * Each key needs to have two already sorted halves in the same order.
     *
     * @param _columns - The keys to merge based on.
     * @param _data_a - The AShared data vectors to merge.
     * @param _data_b - The BShared data vectors to merge.
     * @param order - The sorting direction per column (default ascending)
     */
    template<typename Share, typename EVector>
    static void bitonic_merge(std::vector<BSharedVector<Share, EVector>> _columns,
                             std::vector<ASharedVector<Share, EVector>> _data_a,
                             std::vector<BSharedVector<Share, EVector>> _data_b,
                             const std::vector<SortOrder> &order) {
        std::vector<BSharedVector<Share, EVector>*> res;
        for(BSharedVector<Share, EVector>& c : _columns){
            res.push_back(&c);
        }

        std::vector<ASharedVector<Share, EVector>*> _data_a_;
        for(ASharedVector<Share, EVector>& c : _columns){
            res.push_back(&c);
        }

        std::vector<BSharedVector<Share, EVector>*> _data_b_;
        for(BSharedVector<Share, EVector>& c : _columns){
            res.push_back(&c);
        }


        bitonic_merge(res, _data_a_, _data_b_, order);
    }

    /**
     * Sorts a vector that has two already sorted halves in the same order.
     *
     * @param vec - The vector to merge based on.
     * @param order - The sorting direction (default ascending)
     */
    template<typename Share, typename EVector>
    static void bitonic_merge(BSharedVector<Share, EVector>& vec,
                             SortOrder order=SortOrder::ASC) {
        std::vector<BSharedVector<Share, EVector>*> res;
        res.push_back(&vec);
        bitonic_merge(res, {}, {}, {order});
    }


}  // namespace secrecy::operators

#endif  // SECRECY_OPERATORS_MERGE_H