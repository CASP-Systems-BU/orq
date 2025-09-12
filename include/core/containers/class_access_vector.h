#pragma once

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#ifdef __BMI__
#include <immintrin.h>
#endif

#include <cmath>

#define define_binary_vector_op(_op_)                    \
    ;                                                    \
    inline Vector operator _op_(const Vector &y) const { \
        size_t size = this->size();                      \
        Vector res(size);                                \
        for (size_t i = 0; i < size; ++i) {              \
            res[i] = (*this)[i] _op_ y[i];               \
        }                                                \
        res.matchPrecision(*this);                       \
        return res;                                      \
    }

#define define_unary_vector_op(_op_)        \
    ;                                       \
    inline Vector operator _op_() const {   \
        size_t size = this->size();         \
        Vector res(size);                   \
        for (size_t i = 0; i < size; ++i) { \
            res[i] = _op_(*this)[i];        \
        }                                   \
        res.matchPrecision(*this);          \
        return res;                         \
    }

#define define_binary_vector_element_op(_op_)                   \
    ;                                                           \
    template <typename OtherType>                               \
    inline Vector operator _op_(const OtherType &other) const { \
        size_t size = this->size();                             \
        Vector res(size);                                       \
        for (size_t i = 0; i < size; ++i) {                     \
            res[i] = (*this)[i] _op_ other;                     \
        }                                                       \
        res.matchPrecision(*this);                              \
        return res;                                             \
    }

#define define_binary_vector_assignment_op(_op_)          \
    template <typename OtherType>                         \
    inline Vector operator _op_(const OtherType &other) { \
        size_t size = this->size();                       \
        for (size_t i = 0; i < size; i++) {               \
            (*this)[i] _op_ other[i];                     \
        }                                                 \
        return *this;                                     \
    }

namespace orq {

// Forward declarations
namespace service {
    class RunTime;
    template <typename InputType, typename ReturnType, typename ObjectType>
    class Task_1;
    template <typename InputType, typename ReturnType, typename ObjectType>
    class Task_2;
    template <typename InputType, typename ReturnType, typename... T>
    class Task_ARGS_RTR_1;
    template <typename InputType, typename ReturnType, typename... T>
    class Task_ARGS_RTR_2;
    template <typename InputType>
    class Task_ARGS_VOID_1;
    template <typename InputType>
    class Task_ARGS_VOID_2;
}  // namespace service

static inline int div_ceil(int x, int y) { return x / y + (x % y > 0); }

/**
 * Extracts the bit at `bitIndex` from the given element. Use dedicated
 * hardware instruction if available.
 *
 * @param share The vector element whose bit we want to extract.
 * @param bitIndex The zero-based index (0 is the LSB).
 * @return The extracted bit as a single-bit T element.
 */
template <typename T>
static inline T getBit(const T &share, const int &bitIndex) {
#ifdef __BMI__
    return _bextr_u64(share, bitIndex, 1);
#else
    using Unsigned_type = typename std::make_unsigned<T>::type;
    return ((Unsigned_type)share >> bitIndex) & (Unsigned_type)1;
#endif
}

/**
 * Sets the bit at `bitIndex` in element `share` equal to the LSB of element `bit`.
 * @param share The element whose bit we want to update.
 * @param bit The element whose LSB must be copied into `share`.
 * @param bitIndex The zero-based index (0 is the LSB) of the bit to be updated in element `share`.
 */
template <typename T>
static inline void setBit(T &share, const T &bit, const int &bitIndex) {
    using Unsigned_type = typename std::make_unsigned<T>::type;
    share = (share & ~(((Unsigned_type)1 << bitIndex))) | (bit << bitIndex);
}

/**
 * @brief Clear the bit at position `bitIndex` (i.e., set it to zero)
 *
 * @tparam T
 * @param share
 * @param bitIndex
 */
template <typename T>
static inline void clrBit(T &share, const int &bitIndex) {
    using Unsigned_type = typename std::make_unsigned<T>::type;
    share &= ~((Unsigned_type)1 << bitIndex);
}

/**
 * @brief Set the the bit at the given index to the value provided.
 *
 * @tparam T
 * @param share Vector element to modify
 * @param value binary value to set the specified bit to
 * @param bitIndex which bit to modify
 */
template <typename T>
static inline void setBitValue(T &share, const T &value, const int &bitIndex) {
    using Unsigned_type = typename std::make_unsigned<T>::type;
    clrBit(share, bitIndex);
    share |= (value << bitIndex);
}

/**
 * @brief Conditionally set bits in `share`, masked by `mask`, based on the
 * boolean `value` flag.
 *
 * Optimized version from
 * https://graphics.stanford.edu/~seander/bithacks.html
 *
 * It's not immediately clear to me that this is faster, but perhaps better
 * on certain processors, or with certain optimizations enabled.
 *
 * @param share the element to operate on
 * @param value whether the bits should be set or cleared
 * @param mask which bits to modify
 */
template <typename T>
static inline void setBitMask(T &share, const bool &value, const std::make_unsigned_t<T> &mask) {
    share = (share & ~mask) | (-value & mask);
}

/**
 * ORQ's wrapper of std::vector<T> that provides vectorized plaintext operations.
 * @tparam T The type of elements in the Vector (e.g., int, long, long long, etc.)
 */
template <typename T>
class Vector {
    using Unsigned_type = typename std::make_unsigned<T>::type;

    // The start index of the batch that is currently being processed
    int batch_start = 0;

    //  The end index of the batch that is currently being processed
    int batch_end = 0;

    const int MAX_BITS_NUMBER = std::numeric_limits<Unsigned_type>::digits;

    // The fixed point precision
    size_t precision = 0;

    /**
     * Creates a new Vector whose i-th element is generated by:
     * 1. splitting the bit representation of the i-th element of `this` Vector into parts of size
     * `level_size`, and
     * 2. setting all bits of the most significant half of each part equal to the MSB of the least
     * significant half.
     *
     * @param level_size The number of bits (2^k, k>0) of each part within the bit representation of
     * an element.
     * @return A new vector that contains elements generated as described above.
     *
     * NOTE: This method is bit_level_shift but from right to left, and is used in the parallel
     * prefix adder for boolean addition.
     */

    inline Vector reverse_bit_level_shift(const int &level_size) const {
        // NOTE: Rounding is needed because signed data types have one digit less.
        size_t size = this->size();
        Vector res(size);
        size_t half_level = level_size / 2;

        for (size_t i = 0; i < size; ++i) {
            res[i] = (*this)[i];
            for (size_t j = half_level - 1; j < MAX_BITS_NUMBER - half_level; j += level_size) {
                T bit = getBit((*this)[i], j);
                for (size_t k = 1; k <= half_level; ++k) {
                    setBit(res[i], bit, j + k);
                }
            }
        }
        return res;
    }

    /**
     * Creates a new Vector that contains all elements of `this` Vector
     * right-shifted by `shift_size`. Arithmetic shift is used: signed types
     * will have their MSB copied. To shift in zero instead, use
     * `bit_logical_right_shift`.
     * @param shift_size The number of bits to right-shift each element of `this` Vector.
     * @return A new Vector that contains the right-shifted elements.
     *
     * NOTE: This method works relatively to the current batch.
     */
    inline Vector bit_arithmetic_right_shift(const int &shift_size) const {
        size_t size = this->size();
        Vector res(size);
        for (size_t i = 0; i < size; ++i) {
            res[i] = ((*this)[i]) >> shift_size;
        }

        return res;
    }

    /**
     * Creates a new Vector that contains all elements of `this` Vector
     * right-shifted by `shift_size`. This performs logical shift: zeros are
     * shifted into the MSB. To copy the sign, use `bit_arithmetic_right_shift`
     * @param shift_size The number of bits to right-shift each element of `this` Vector.
     * @return A new Vector that contains the right-shifted elements.
     *
     * NOTE: This method works relatively to the current batch.
     */
    inline Vector bit_logical_right_shift(const int &shift_size) const {
        size_t size = this->size();
        Vector res(size);
        for (size_t i = 0; i < size; ++i) {
            res[i] = ((Unsigned_type)(*this)[i]) >> shift_size;
        }

        return res;
    }

    /**
     * Creates a new Vector that contains all elements of `this` Vector left-shifted by
     * `shift_size`.
     * @param shift_size The number of bits to left-shift each element of `this` Vector.
     * @return A new Vector that contains the left-shifted elements.
     *
     * NOTE: This method works relatively to the current batch.
     */

    inline Vector bit_left_shift(const int &shift_size) const {
        size_t size = this->size();
        Vector res(size);
        for (size_t i = 0; i < size; ++i) {
            res[i] = ((Unsigned_type)(*this)[i]) << shift_size;
        }

        return res;
    }

    /**
     * Creates a new Vector whose i-th element is a single bit generated by XORing all bits of the
     * i-th element of `this` Vector, 0 <= i < size(). (Basically parity check of each element.)
     * @return A new Vector that contains single-bit elements generated as described above.
     *
     * NOTE: This method works relatively to the current batch.
     */
    inline Vector bit_xor() const {
        size_t size = this->size();
        Vector res(size);

        for (size_t i = 0; i < size; ++i) {
            T v = 0;
            auto t = (*this)[i];

            while (t) {
                v ^= 1;
                t &= t - 1;
            }

            res[i] = v;
            // for (int j = 0; j < MAX_BITS_NUMBER; ++j) {
            //     *r ^= getBit(t, j);
            // }
        }

        return res;
    }

    // TODO: check for a (different by one index) bug for `end`.
    /**
     * Returns a new vector containing elements in the range [start, end] that are `step` positions
     * apart.
     * @param start The index of the first element to be included in the output vector.
     * @param step The distance between two consecutive elements.
     * @param end The maximum possible index of the last element to be included in the output
     * vector.
     * @return A new vector that contains the selected elements.
     *
     * NOTE: This method works relatively to the current batch.
     */
    Vector simple_subset(const int &start, const int &step, const int &end) const {
        size_t res_size = end - start + 1;

        auto res = Vector(res_size);

        for (size_t i = 0; i < res_size; i += step) {
            res.data[i / step] = (*this)[start + i];
        }
        return res;
    }

    /**
     * Sets the current batch equal to the whole vector.
     */
    void reset_batch() {
        batch_start = 0;
        batch_end = this->total_size();
    }

    // TODO: check the last index usage for consistency.
    /**
     * Sets start and end index of the current batch.
     * If the start index is negative, the start index is set to zero. If the end index is greater
     * than the Vector's size, the end index is set the max possible index.
     * @param _start_ind The index of the first element in the current batch.
     * @param _end_ind The index of the last element in the current batch.
     */
    void set_batch(const int &_start_ind, const int &_end_ind) {
        batch_start = (_start_ind >= 0) ? _start_ind : 0;
        batch_end = (_end_ind <= this->total_size()) ? _end_ind : this->total_size();
    }

   public:
    // allows the use of orq::Vector<T>::value_type
    using value_type = T;

    /**
     * Sets the fixed-point precision.
     * @param fixed_point_precision - the number of fixed-point fractional bits.
     */
    inline void setPrecision(const int fixed_point_precision) { precision = fixed_point_precision; }

    /**
     * Gets the fixed-point precision.
     */
    inline size_t getPrecision() const { return precision; }

    /**
     * Helper that sets this vector's precision to match another Vector.
     * @param other The Vector whose precision should be copied.
     */
    void matchPrecision(const Vector<T> &other) { precision = other.getPrecision(); }

    /**
     * @return The total number of elements in the vector.
     */
    inline size_t total_size() const { return this->data.get()->size(); }

    // TODO: fix this using the VectorData Class
    /**
     * @return An iterator pointing to the first element.
     *
     * NOTE: This method is used by the communicator.
     */
    inline typename std::vector<T>::iterator begin() { return data.get()->begin() + batch_start; }

    // TODO: fix this using the VectorData Class
    /**
     * @return An iterator pointing to the last element.
     *
     * NOTE: This method is used by the communicator.
     */
    inline typename std::vector<T>::iterator end() { return data.get()->begin() + batch_end; }

    //        /**
    //         * This is the generic function in order create a new mapping for this vector.
    //         * Note: that the function does not allocate a new memory location for data.
    //         * Note: the mapping maps from the new index space to the original index space for
    //         data.
    //         * In other words, composition will not work; new mapping pattern replaces the
    //         previous one.
    //         * @param _subset_offset the index of the first element of the original vector to
    //         apply the pattern.
    //         * @param _subset_step the index difference between the mapped to elements within each
    //         chunks.
    //         * @param _subset_included_size the maximum size of each included chunk.
    //         * @param _subset_excluded_size the maximum size of each excluded chunk.
    //         * (included and excluded chunks alternate after offset).
    //         * @param _subset_direction the direction of choosing elements (increasing index = 1)
    //         (decreasing index = -1).
    //         * @param _subset_repetition number of times to repeat same mapped-to-elements after
    //         each other.
    //         * @param _subset_cycles number of times concatenate the whole mapped-to-elements in
    //         the new reference.
    //         * @return  Vector that points to the same memory location as the new one but
    //         different mapping for the indices.
    //         */
    //        Vector subset_reference(const int &_subset_offset,              // = 1
    //                                const int &_subset_step,                // = 1
    //                                const int &_subset_included_size,       // = -1
    //                                const int &_subset_excluded_size,       // = -1
    //                                const int &_subset_direction,           // = 1
    //                                const int &_subset_repetition,          // = 1
    //                                const int &_subset_cycles) const {      // = 1
    //
    //            Vector res = *this;  // Shallow copy constructor (res will point to the same
    //            memory as `this`)
    //
    //            // First set the variables that do not change
    //            res.subset_offset = _subset_offset;
    //            res.subset_step = _subset_step;
    //            res.subset_direction = _subset_direction;
    //            res.subset_repetition = _subset_repetition;
    //            res.subset_cycles = _subset_cycles;
    //
    //
    //            // Set Variables with default variables changeable
    //            res.subset_included_size = _subset_included_size;
    //            res.subset_excluded_size = _subset_excluded_size;
    //
    //            // Check if default variable should be modified
    //            if (res.subset_included_size == -1) {
    //                res.subset_included_size = this->total_size();
    //            }
    //
    //            if (res.subset_excluded_size == -1) {
    //                res.subset_excluded_size = this->total_size() - res.subset_included_size;
    //            }
    //
    //            // Set the batch size
    //            res.reset_batch();
    //            return res;
    //        }

    /**
     * Remaps the index space to choose a number of elements of the current vector.
     * Note: returned Vector points to the same memory location.
     * @param _subset_offset the index of the first element of the original vector to apply the
     * pattern. (default = 0)
     * @param _subset_step the index of the difference between each two included elements.
     * (default = 1)
     * @param _subset_included_size the size of the elements on which the pattern is applied.
     * (default = total_size())
     * @return Vector that has different index mapping to the original vector elements.
     */
    Vector simple_subset_reference(const int _start_index, const int _step,
                                   const int _end_index) const {
        Vector res(std::shared_ptr<VectorDataBase<T>>(
            new SimpleVectorData<T>(data, _start_index, _end_index, _step)));
        res.reset_batch();
        return res;
    }

    inline Vector simple_subset_reference(const int _start_index, const int _step) const {
        Vector res(std::shared_ptr<VectorDataBase<T>>(new SimpleVectorData<T>(
            data, _start_index, std::max(0, data.get()->size() - 1), _step)));
        res.reset_batch();
        return res;
    }

    inline Vector simple_subset_reference(const int _start_index) const {
        Vector res(std::shared_ptr<VectorDataBase<T>>(
            new SimpleVectorData<T>(data, _start_index, std::max(0, data.get()->size() - 1), 1)));
        res.reset_batch();
        return res;
    }

    /**
     * Applies an alternating pattern to include and exclude elements. It applies the pattern
     * starting with the element with `index = _subset_offset`. It then keeps alternating elements
     * as included in the pattern of or excluded from the pattern using the `_subset_included_size`
     * and the `_subset_excluded_size` for each included or excluded chunk.
     * @param _subset_offset the index of the first element to apply the pattern.
     * @param _subset_step the difference two each two included elements within the included the
     * chunks.
     * @param _subset_included_size the size of a number of elements that we choosing from.
     * @param _subset_excluded_size the size of a number of elements that we are totally not
     * choosing.
     * @return `Vector` that points to the same memory location as the original vector but with
     * different index mapping.
     */
    Vector alternating_subset_reference(const size_t _subset_included_size,
                                        const size_t _subset_excluded_size) const {
        if (_subset_excluded_size != -1) {
            Vector res(std::shared_ptr<VectorDataBase<T>>(
                new AlternatingVectorData<T>(data, _subset_included_size, _subset_excluded_size)));
            res.reset_batch();
            return res;
        } else {
            Vector res(std::shared_ptr<VectorDataBase<T>>(new AlternatingVectorData<T>(
                data, _subset_included_size, this->total_size() - _subset_included_size)));
            res.reset_batch();
            return res;
        }
    }

    Vector reversed_alternating_subset_reference(const size_t _subset_included_size,
                                                 const size_t _subset_excluded_size) const {
        if (_subset_excluded_size != -1) {
            Vector res(std::shared_ptr<VectorDataBase<T>>(new ReversedAlternatingVectorData<T>(
                data, _subset_included_size, _subset_excluded_size)));
            res.reset_batch();
            return res;
        } else {
            Vector res(std::shared_ptr<VectorDataBase<T>>(new ReversedAlternatingVectorData<T>(
                data, _subset_included_size, this->total_size() - _subset_included_size)));
            res.reset_batch();
            return res;
        }
    }

    /**
     * Applies a new indexing mapping to the current vector so that each element is repeated a
     * number of times consecutively.
     * @param _subset_repetition the number of times each element is repeated.
     * @return `Vector` that points to the same memory location as the original vector but with
     * different index mapping.
     */
    Vector repeated_subset_reference(const size_t _subset_repetition) const {
        Vector res(std::shared_ptr<VectorDataBase<T>>(
            new RepeatedVectorData<T>(data, _subset_repetition)));
        res.reset_batch();
        return res;
    }

    /**
     * Applies a new indexing mapping such that after accessing the last element, we access the
     * first element again and keep accessing the elements in cycles.
     * @param _subset_cycles the number of cycles the new indexing mapping will contain.
     * @return `Vector` that points to the same memory location as the original vector but with
     * different index mapping.
     */
    Vector cyclic_subset_reference(const size_t _subset_cycles) const {
        Vector res(
            std::shared_ptr<VectorDataBase<T>>(new CyclicVectorData<T>(data, _subset_cycles)));
        res.reset_batch();
        return res;
    }

    /**
     * Applies a new mapping indexing that controls the order in which the elements accessed.
     * @param _subset_direction set to (1) to keep current order or (-1) to reverse the order.
     * @return `Vector` that points to the same memory location as the original vector but with
     * different index mapping.
     */
    Vector directed_subset_reference(const size_t _subset_direction) const {
        if (_subset_direction == -1) {
            Vector res(std::shared_ptr<VectorDataBase<T>>(new ReversedVectorData<T>(data)));
            res.reset_batch();
            return res;
        } else {
            return *this;
        }
    }

    /**
     * This function extracts bits from current vector and append them in sequence into
     * another vector. The functions chooses the bits by getting the needed parameters to
     * loop through the bits in each element.
     * @param start index of the first bit to be included (lowest significant).
     * @param step difference in index between each two consecutive bits.
     * @param end index of the last bit bit to be included (most significant)
     * @param repetition number of times each bit will be included.
     * @return a new `Vector` that has only the chosen bits in its elements (less size than input).
     */
    Vector simple_bit_compress(const size_t &start, const size_t &step, const size_t &end,
                               const size_t &repetition) const {
        const size_t _step = step;
        const size_t _repetition = repetition;

        const size_t bits_per_element = std::abs(((end - start + 1) / step) * repetition);
        const size_t total_bits = bits_per_element * this->size();
        const size_t total_new_elements = div_ceil(total_bits, MAX_BITS_NUMBER);

        Vector res(total_new_elements);

        // TODO: factor out division here, keep two counters?
        for (size_t i = 0, j = 0; j < total_bits; i++, j += MAX_BITS_NUMBER) {
            auto r = &res[i];
            for (size_t k = j, p = 0; p < MAX_BITS_NUMBER && k < total_bits; k++, p++) {
                setBitValue(*r,
                            getBit((*this)[k / bits_per_element],
                                   start + (k % bits_per_element) / _repetition * _step),
                            p);
            }
        }

        return res;
    }

    /**
     * @brief simple_bit_compress, optimized for the (i, 1, i, 1) case. This
     * version further operates on a passed vector, rather than returning a
     * new Vector.
     *
     * @param res vector to compress into
     * @param position single bit position to compress (= start = end)
     */
    void simple_bit_compress(Vector &res, const size_t &position) const {
        const size_t total_bits = this->size();

        for (size_t i = 0, j = 0; j < total_bits; i++, j += MAX_BITS_NUMBER) {
            T r = 0;
            // auto r = &res[i];
            for (size_t k = j, p = 0; p < MAX_BITS_NUMBER && k < total_bits; k++, p++) {
                // // This works but is a bit slower:
                // r |= _pdep_u64((*this)[j + k] >> position, 1 << k);

                // Extract the bit at `position` and shift it into `r`
                r |= getBit((*this)[k], position) << p;
            }
            res[i] = r;
        }
    }

    /**
     * Function to reverse the simple_bit_compress function. it takes an already
     * compressed `Vector` and assign from it the corresponding bits to the this called on
     * `Vector`.
     * @param other the vector that has the compressed bits.
     * @param start index of the first bit to be included (lowest significant).
     * @param step difference in index between each two consecutive bits.
     * @param end index of the last bit bit to be included (most significant)
     * @param repetition number of times each bit will be included.
     */
    void simple_bit_decompress(const Vector &other, const size_t &start, const size_t &step,
                               const size_t &end, const size_t &repetition) {
        const size_t bits_per_element = std::abs(((end - start + 1) / step) * repetition);
        const size_t total_bits = bits_per_element * this->size();
        const size_t total_new_elements = div_ceil(total_bits, MAX_BITS_NUMBER);

        for (size_t i = 0, j = 0; j < total_bits; i++, j += MAX_BITS_NUMBER) {
            auto r = other[i];
            for (size_t k = j, p = 0; p < MAX_BITS_NUMBER && k < total_bits; k++, p++) {
                setBitValue((*this)[k / bits_per_element], getBit(r, p),
                            start + (k % bits_per_element) / repetition * step);
            }
        }
    }

    /**
     * @brief Optimized version of simple_bit_compress for the single-
     * position case.
     *
     * @param other vector to decompress into this
     * @param position the bit position to decompress
     *
     * Note: in testing, setBitMask was not noticeably faster than
     * setBitValue
     *
     */
    void simple_bit_decompress(const Vector &other, const T &position) {
        const int total_bits = this->size();

        for (int i = 0, j = 0; j < total_bits; i++, j += MAX_BITS_NUMBER) {
            auto r = other[i];
            for (int k = j, p = 0; p < MAX_BITS_NUMBER && k < total_bits; k++, p++) {
                setBitValue((*this)[k], getBit(r, p), position);
            }
        }
    }

    /**
     * This function extracts bits from current vector and append them in sequence into
     * another vector. The function chooses bits as follows. First it skips till the start
     * index (from lowest significant). Then it splits the bits into sequences of included
     * chunks and excluded chunks. From the included bits chunks, bits that `step` index
     * difference apart are chosen. If direction is set to `1`, picking starts from lowest
     * significant bits. If it is set to `-1`, picking starts from most significant bits.
     * @param start index of the first bit to start the included/excluded chunks pattern.
     * @param step difference between each two consecutive bits in each included chunk.
     * @param included_size size of each included chunk.
     * @param excluded_size size of each excluded chunk.
     * @param direction direction for picking up the bits in each included_size chunk. `1` means
     * least significant first. `-1` means most significant first.
     * @return a new `Vector` that has only the chosen bits in its elements (less size than input).
     */
    Vector alternating_bit_compress(const size_t &start, const size_t &step,
                                    const size_t &included_size, const size_t &excluded_size,
                                    const int &direction) const {
        const size_t bits_per_chunk = included_size / step;
        const size_t bits_per_element =
            (MAX_BITS_NUMBER - start) / (included_size + excluded_size) * bits_per_chunk;
        const size_t last_chunk_bits_per_element =
            std::min(included_size, ((MAX_BITS_NUMBER - start) % (included_size + excluded_size))) /
            step;
        const size_t total_bits_per_element = bits_per_element + last_chunk_bits_per_element;

        const int direction_offset = (direction == -1) ? included_size - 1 : 0;

        const size_t total_bits = total_bits_per_element * this->size();
        const size_t total_new_elements = div_ceil(total_bits, MAX_BITS_NUMBER);

        Vector res(total_new_elements);

        for (size_t i = 0; i < total_bits; ++i) {
            const size_t element_index = i / total_bits_per_element;
            const size_t element_chunk_index = (i % total_bits_per_element) / bits_per_chunk;
            const size_t element_bit_index =
                start + direction_offset + element_chunk_index * (included_size + excluded_size) +
                ((i % total_bits_per_element) % bits_per_chunk) * step * direction;
            setBit(res[i / MAX_BITS_NUMBER], getBit((*this)[element_index], element_bit_index),
                   i % MAX_BITS_NUMBER);
        }

        return res;
    }

    inline Vector alternating_bit_compress(const size_t &start, const size_t &step,
                                           const size_t &included_size,
                                           const size_t &excluded_size) const {
        return alternating_bit_compress(start, step, included_size, excluded_size, 1);
    }

    /**
     * Function to reverse the alternating_bit_compress function. it takes an already
     * compressed `Vector` and assign from it the corresponding bits to the this called on
     * `Vector`.
     * @param other the vector that has the compressed bits.
     * @param start index of the first bit to start the included/excluded chunks pattern.
     * @param step difference between each two consecutive bits in each included chunk.
     * @param included_size size of each included chunk.
     * @param excluded_size size of each excluded chunk.
     * @param direction direction for picking up the bits in each included_size chunk. `1` means
     * least significant first. `-1` means most significant first.
     */
    void alternating_bit_decompress(const Vector &other, const size_t &start, const size_t &step,
                                    const size_t &included_size, const size_t &excluded_size,
                                    const int &direction) const {
        const size_t bits_per_chunk = included_size / step;
        const size_t bits_per_element =
            (MAX_BITS_NUMBER - start) / (included_size + excluded_size) * bits_per_chunk;
        const size_t last_chunk_bits_per_element =
            std::min(included_size, ((MAX_BITS_NUMBER - start) % (included_size + excluded_size))) /
            step;
        const size_t total_bits_per_element = bits_per_element + last_chunk_bits_per_element;

        const int direction_offset = (direction == -1) ? included_size - 1 : 0;

        const size_t total_bits = total_bits_per_element * this->size();
        const size_t total_new_elements = div_ceil(total_bits, MAX_BITS_NUMBER);

        for (int i = 0; i < total_bits; ++i) {
            const size_t element_index = i / total_bits_per_element;
            const size_t element_chunk_index = (i % total_bits_per_element) / bits_per_chunk;
            const size_t element_bit_index =
                start + direction_offset + element_chunk_index * (included_size + excluded_size) +
                ((i % total_bits_per_element) % bits_per_chunk) * step * direction;
            setBit((*this)[element_index], getBit(other[i / MAX_BITS_NUMBER], i % MAX_BITS_NUMBER),
                   element_bit_index);
        }
    }

    /**
     * A (shared) pointer to the actual vector contents.
     *
     * NOTE: Shallow copying of this object creates two instances that share the same data.
     */
    std::shared_ptr<VectorDataBase<T>> data;

    //        /**
    //         * This constructor allows for creating a new vector by just passing initialization
    //         * parameters for inner `data` variable.
    //         * @tparam T is a generic type to allow for different constructors for the variable
    //         `data`.
    //         * @param args is the packed parameters passed to `data` initializer.
    //         */
    //        template<typename...T>
    //        Vector(T... args) :
    //                data(std::shared_ptr<std::vector<T>>(
    //                        new std::vector<T>(args...))) { batch_end = data.get()->size(); }

    /**
     * Creates a Vector of `size` values initialize to `init_val` (0 by default).
     * @param size The size of the new Vector.
     */
    Vector(size_t _size, T _init_val = 0)
        : data(std::shared_ptr<VectorDataBase<T>>(new VectorData<T>(_size, _init_val))) {
        batch_end = data.get()->size();
    }

    // TODO: Should we use .size() or .total_size() here?
    /**
     * Move constructor
     * @param other The std::vector<T> whose elements will be moved to the new Vector.
     */
    Vector(std::vector<T> &&_other)
        : data(std::shared_ptr<VectorDataBase<T>>(new VectorData<T>(_other))),
          batch_end(_other.size()) {}

    /**
     * Copy constructor
     * @param other The std::vector<T> whose elements will be copied to the new Vector.
     */
    Vector(std::vector<T> &_other)
        : data(std::shared_ptr<VectorDataBase<T>>(new VectorData<T>(_other))),
          batch_end(_other.size()) {}

    /**
     * Constructs a new Vector from a list of `T` elements.
     * @param elements The list of elements of the new Vector.
     */
    Vector(std::initializer_list<T> &&elements)
        : data(std::shared_ptr<VectorDataBase<T>>(new VectorData<T>(elements))) {
        batch_end = data.get()->size();
    }

    /**
     * This is a shallow copy constructor.
     * @param other The vector that contains the std::vector<T> pointer to be copied.
     *
     * WARNING: The new vector will point to the same memory location used by `other`. To copy the
     * data into a separate memory location, create a new vector first then use assignment operator.
     */
    Vector(const Vector &other)
        : data(other.data),
          batch_start(other.batch_start),
          batch_end(other.batch_end),
          precision(other.precision) {}

    Vector(std::shared_ptr<VectorDataBase<T>> other)
        : data(other), batch_start(0), batch_end(other->size()) {}

    // TODO: move assignment has deleting guaranteed on the called-on variable
    //  Use this info to optimize assignment.
    /**
     * This is a deep move assignment operator.
     * Applies the move assignment operator to T. Assigns the contents of the `other` vector to the
     * this vector. Assumes `other` has the same size as this vector.
     * @param other The Vector that contains the values to be assigned to this vector.
     * @return A reference to this vector after modification.
     *
     * NOTE: This method works relatively to the current batch.
     */
    Vector &operator=(const Vector &&other) {
        size_t size = this->size();
        for (size_t i = 0; i < size; ++i) {
            (*this)[i] = other[i];
        }
        precision = other.getPrecision();
        return *this;
    }

    /**
     * This is a deep copy assignment operator.
     * Applies the copy assignment operator to T. Copies the contents of the `other` vector to this
     * vector. Assumes `other` has the same size as this vector.
     * @param other the Vector that contains the values to be copied.
     * @return A reference to `this` Vector after modification.
     */
    Vector &operator=(const Vector &other) {
        size_t size = this->size();
        for (size_t i = 0; i < size; ++i) {
            (*this)[i] = other[i];
        }
        precision = other.getPrecision();
        return *this;
    }

    /**
     * @brief Copy-and-cast assignment operator. Allows (down)casting
     * elements from a vector of one type into another.
     *
     */
    template <typename OtherT>
    Vector &operator=(const Vector<OtherT> &other) {
        size_t size = this->size();
        for (size_t i = 0; i < size; i++) {
            (*this)[i] = (T)other[i];
        }
        precision = other.getPrecision();
        return *this;
    }

    /**
     * Returns a new vector that contains all elements in the range [start, end].
     * @param start The index of the first element to be included in the output vector.
     * @param end The index of the last element to be included in the output vector.
     * @return A new vector that contains the selected elements.
     *
     * NOTE: This method works relatively to the current batch.
     */
    Vector simple_subset(const size_t &start, const size_t &size) const {
        auto res = Vector(size);

        for (size_t i = 0; i < size; ++i) {
            res[i] = (*this)[start + i];
        }
        return res;
    }

    // Rename with `and` and refactor as a common macro
    /**
     * Masks each element in `this` vector by doing a bitwise logical AND with `n`.
     * @param n The mask.
     */
    void mask(const T &n) {
        for (int i = 0; i < this->size(); ++i) {
            (*this)[i] &= n;
        }
    }

    /**
     * Sets the bits of each element in `this` vector by doing a bitwise logical OR with `n`
     * @param n The element that encodes the bits to set.
     */
    void set_bits(const T &n) {
        for (int i = 0; i < this->size(); ++i) {
            (*this)[i] |= n;
        }
    }

    /**
     * Sets every element of this vector to zero.
     */
    void zero() {
        for (int i = 0; i < this->size(); i++) {
            (*this)[i] = 0;
        }
    }

    static const int LEVEL_MASK_SIZE = 7;

    /**
     * @brief Mask for level 2^i (64 bits). Gives LSB of the most
     * significant half of each chunk.
     */
    static const uint64_t constexpr LEVEL_MASKS[LEVEL_MASK_SIZE] = {
        0xffffffffffffffff,  //  1, N/A
        0xaaaaaaaaaaaaaaaa,  //  2, Chunk               ...10
        0x4444444444444444,  //  4, Chunk             ...0100
        0x1010101010101010,  //  8, Chunk         ...00010000
        0x0100010001000100,  // 16, Chunk ...0000000100000000
        0x0001000000010000,  // 32, etc.
        0x0000000100000000,  // 64
    };

    /** @brief Creates a new Vector whose i-th element is generated by:
     * 1. splitting the bit representation of the i-th element of `this` Vector into parts of size
     * `level_size`, and
     * 2. setting all bits of the least significant half of each part equal to the LSB of the most
     * significant part.
     *
     * @param log_level_size log2 of the maximum chunk size (indexes into `LEVEL_MASKS`)
     * @return A new vector that contains elements generated as described above.
     *
     * NOTE: This method is used in secure greater-than and works relatively to the current batch.
     *
     * Moved from private so we can test this method externally.
     */
    inline Vector bit_level_shift(const int &log_level_size) const {
        static const int MAX_BITS_NUMBER = std::numeric_limits<std::make_unsigned_t<T>>::digits;

        auto mask = LEVEL_MASKS[log_level_size];

        size_t size = this->size();
        Vector res(size);

        auto half_size = 1 << (log_level_size - 1);

        for (size_t i = 0; i < size; i++) {
            T t = (*this)[i];

            T or_mask = 0;
            T and_mask = -1;

            T zero_mask = (t | ~mask);
            T one_mask = (t & mask);

            // Build the masks for this element
            for (size_t j = 0; j <= half_size; j++) {
                and_mask &= zero_mask;
                or_mask |= one_mask;

                // NOTE: this is arithmetic shift; i.e. sign extension
                // This happens because T is signed.
                zero_mask >>= 1;
                one_mask >>= 1;
            }

            // Create the level-shifted result.
            res[i] = (t & and_mask) | or_mask;
        }

        return res;
    }

    /**
     * @return The number of elements in the vector.
     *
     * NOTE: This method works relatively to the current batch.
     */
    inline size_t size() const {
        //            printf("total_size: %d\n", this->total_size());
        return batch_end - batch_start;
    }

    // **************************************** //
    //           Arithmetic operators           //
    // **************************************** //

    /**
     * Elementwise plaintext addition.
     */
    define_binary_vector_op(+);

    /**
     * Elementwise plaintext subtraction.
     */
    define_binary_vector_op(-);

    /**
     * Elementwise plaintext multiplication.
     */
    define_binary_vector_op(*);

    /**
     * Elementwise plaintext division.
     */
    define_binary_vector_op(/);

    /**
     * Elementwise plaintext negation.
     */
    define_unary_vector_op(-);

    // **************************************** //
    //             Boolean operators            //
    // **************************************** //

    /**
     * Elementwise plaintext bitwise AND.
     */
    define_binary_vector_op(&);

    /**
     * Elementwise plaintext bitwise OR.
     */
    define_binary_vector_op(|);

    /**
     * Elementwise plaintext bitwise XOR.
     */
    define_binary_vector_op(^);

    /**
     * Elementwise plaintext boolean complement.
     */
    define_unary_vector_op(~);

    /**
     * Elementwise plaintext boolean negation.
     */
    define_unary_vector_op(!);

    // **************************************** //
    //           Comparison operators           //
    // **************************************** //

    /**
     * Elementwise plaintext equality comparison.
     */
    define_binary_vector_op(==);

    /**
     * Elementwise plaintext inequality comparison.
     */
    define_binary_vector_op(!=);

    /**
     * Elementwise plaintext greater-than comparison.
     */
    define_binary_vector_op(>);

    /**
     * Elementwise plaintext greater-or-equal comparison.
     */
    define_binary_vector_op(>=);

    /**
     * Elementwise plaintext less-than comparison.
     */
    define_binary_vector_op(<);

    /**
     * Elementwise plaintext less-or-equal comparison.
     */
    define_binary_vector_op(<=);

    define_binary_vector_element_op(+);
    define_binary_vector_element_op(-);
    define_binary_vector_element_op(*);
    define_binary_vector_element_op(/);
    define_binary_vector_element_op(%);

    define_binary_vector_element_op(&);
    define_binary_vector_element_op(|);
    define_binary_vector_element_op(^);

    define_binary_vector_element_op(>>);
    define_binary_vector_element_op(<<);

    define_binary_vector_element_op(>);
    define_binary_vector_element_op(<);
    define_binary_vector_element_op(==);
    define_binary_vector_element_op(!=);

    define_binary_vector_assignment_op(+=);
    define_binary_vector_assignment_op(-=);
    define_binary_vector_assignment_op(*=);
    define_binary_vector_assignment_op(&=);
    define_binary_vector_assignment_op(|=);
    define_binary_vector_assignment_op(^=);

    // TODO: remove that; equivalent to `< 0`
    /**
     * Elementwise plaintext less-than-zero comparison.
     */
    inline Vector ltz() const {
        size_t size = this->size();
        Vector res(size);
        for (isize_tnt i = 0; i < size; i++) res[i] = ((*this)[i] < (T)0);
        return res;
    }

    /**
     * Elementwise plaintext LSB extension: set all bits equal to the LSB.
     * Note: this is only makes sense for bit shares.
     */
    inline Vector extend_lsb() const {
        size_t size = this->size();
        Vector res(size);
        for (size_t i = 0; i < size; i++) res[i] = -((*this)[i] & 1);  // Relies on two's complement
        return res;
    }

    inline Vector extract_valid(Vector valid) {
        assert(this->size() == valid.size());
        std::vector<T> r;
        for (size_t i = 0; i < valid.size(); i++) {
            if (valid[i] != 0) {
                r.push_back((*this)[i]);
            }
        }
        return r;
    }

    /**
     * Returns a mutable reference to the element at the given `index`.
     * @param index The index of the target element.
     * @return A mutable reference to the element at the given `index`.
     *
     * NOTE: This method works relatively to the current batch.
     */
    inline T &operator[](const int &index) { return (*data.get())[batch_start + index]; }

    /**
     * Returns an immutable reference of the element at the given `index`.
     * @param index The index of the target element.
     * @return Returns a read-only reference of the element at the given `index`.
     *
     * NOTE: This method works relatively to the current batch.
     */
    inline const T &operator[](const int &index) const {
        return (*data.get())[batch_start + index];
    }

    //        /**
    //         * Unpacks bits in the elements of `this` vector to create a new vector of size `n`
    //         whose i-th element equals
    //         * the (i/n)-th bit of the (i%n)-th element of `this` vector.
    //         * @param n The number of bits to 'unpack'.
    //         * @return A new Vector that contains `n` single-bit elements constructed as described
    //         above.
    //         */
    //        inline Vector bits2elements(int n) const {
    //            static const int MAX_BITS_NUMBER = std::pow(2,
    //            std::ceil(std::log2(std::numeric_limits<T>::digits))); Vector res(n); int limit =
    //            ceil(n/this->size()); for (int i=0; i<limit; i++) {
    //                for (int j=0; j<MAX_BITS_NUMBER; j++)
    //                    res[i] = getBit(*this[i], j);
    //            }
    //            return res;
    //        }

    // TODO: replace with aggregation functions
    //  - Functions that can sum together multiple elements `sum`
    //  - Functions that can do together multiple logical operations `and`, `or`, `xor`.
    /**
     * Checks if the two input vectors (`this` and `other`) contain the same elements.
     * @param other The vector to compare `this` with.
     * @return True if `this` vector contains the same elements with `other`, False otherwise.
     */
    bool same_as(const Vector<T> &other) const {
        if (this->size() != other.size()) {
#ifdef DEBUG_VECTOR_SAME_AS
            std::cout << "[same_as]: size mismatch: this size " << this->size()
                      << " != " << other.size() << "\n";
#endif
            return false;
        }
        for (int i = 0; i < other.size(); i++)
            if ((*this)[i] != other[i]) {
#ifdef DEBUG_VECTOR_SAME_AS
                std::cout << "[same_as]: mismatch @ " << i << ": this " << (long)(*this)[i]
                          << " != " << (long)other[i] << "\n";
#endif
                return false;
            }
        return true;
    }

    /**
     * @brief Checks if the vector `prefix` is a prefix of this vector.
     *
     * @param prefix
     * @return true if the argument is a prefix
     * @return false otherwise
     */
    bool starts_with(const Vector<T> &prefix) {
        if (prefix.total_size() > total_size()) {
            return false;
        }

        for (int i = 0; i < prefix.size(); i++) {
            if ((*this)[i] != prefix[i]) {
                return false;
            }
        }
        return true;
    }

    // Friend classes
    template <typename Share, int ReplicationNumber>
    friend class EVector;
    friend class service::RunTime;

    template <typename InputType, typename ReturnType, typename ObjectType>
    friend class orq::service::Task_1;

    template <typename InputType, typename ReturnType, typename ObjectType>
    friend class orq::service::Task_2;

    template <typename InputType, typename ReturnType, typename... U>
    friend class orq::service::Task_ARGS_RTR_1;

    template <typename InputType, typename ReturnType, typename... U>
    friend class orq::service::Task_ARGS_RTR_2;

    template <typename InputType>
    friend class orq::service::Task_ARGS_VOID_1;

    template <typename InputType>
    friend class orq::service::Task_ARGS_VOID_2;
};

/**
 * Same as BSharedVector::compare_rows() but works with plaintext data. Used for testing.
 *
 * Compares two `MxN` arrays row-wise by applying `M` greater-than comparisons on `N` keys.
 *
 * @tparam Share Share data type.
 * @param x_vec The left column-first array with `M` rows and `N` columns.
 * @param y_vec The right column-first array with `M` rows and `N` columns.
 * @param inverse A vector of `N` boolean values that denotes the order of comparison per key (if
 * `inverse[i]=True`, then rows from `x_vec` and `y_vec` are swapped in the comparison on the i-th
 * column.
 * @return A new vector that contains the result bits of the `M` greater-than comparisons.
 *
 * NOTE: The i-th row, let l, from the left array is greater than the i-th row, let r, from the
 * right array if l's first key is greater than r's first key, or the first keys are the same and
 * l's second key is greater than r's second key, or the first two keys are the same and so forth,
 * for all keys.
 */
// TODO (john): Move this to utils
template <typename Share>
static Vector<Share> compare_rows(const std::vector<Vector<Share> *> &x_vec,
                                  const std::vector<Vector<Share> *> &y_vec,
                                  const std::vector<bool> &inverse) {
    assert((x_vec.size() > 0) && (x_vec.size() == y_vec.size()) &&
           (inverse.size() == x_vec.size()));
    const int cols_num = x_vec.size();  // Number of keys
    // Compare elements on first key
    Vector<Share> *t = inverse[0] ? y_vec[0] : x_vec[0];
    Vector<Share> *o = inverse[0] ? x_vec[0] : y_vec[0];
    Vector<Share> eq = (*t == *o);
    Vector<Share> gt = (*t > *o);

    // Compare elements on remaining keys
    for (int i = 1; i < cols_num; ++i) {
        t = inverse[i] ? y_vec[i] : x_vec[i];
        o = inverse[i] ? x_vec[i] : y_vec[i];
        Vector<Share> new_eq = (*t == *o);
        Vector<Share> new_gt = (*t > *o);

        // Compose 'gt' and `eq` bits
        gt ^= (new_gt & eq);
        eq &= new_eq;
    }
    return gt;
}

/**
 *
 * Same as BSharedVector::swap() but works with plaintext data. Used for testing.
 *
 * Swaps rows of two `MxN` arrays in place using the provided `bits`.
 *
 * @tparam Share Share data type.
 * @param x_vec The left column-first array with `M` rows and `N` columns.
 * @param y_vec The right column-first array with `M` rows and `N` columns.
 * @param bits The vector that contains the 'M' bits to use for swapping (if bits[i]=True, the i-th
 * rows will be swapped).
 */
// TODO (john): Move this to utils
template <typename Share>
static void swap(std::vector<Vector<Share> *> &x_vec, std::vector<Vector<Share> *> &y_vec,
                 const Vector<Share> &bits) {
    // Make sure the input arrays have the same dimensions
    assert((x_vec.size() > 0) && (x_vec.size() == y_vec.size()));
    const int cols_num = x_vec.size();  // Number of columns
    for (int i = 0; i < cols_num; ++i) {
        auto xi_size = x_vec[i]->size();
        assert((xi_size == y_vec[i]->size()) && (bits.size() == xi_size));
    }
    auto b = bits.extend_lsb();
    // Swap elements
    for (int i = 0; i < cols_num; ++i) {
        auto xi = &x_vec[i];
        auto yi = &y_vec[i];
        auto tmp = (b & *yi) ^ (~b & *xi);
        *yi = (b & *xi) ^ (~b & *yi);
        *xi = tmp;
    }
}

/**
 *
 * Same as BSharedVector::swap() but works with plaintext data. Used for testing.
 *
 * Swaps rows of two `MxN` arrays in place using the provided `bits`.
 *
 * @tparam Share Share data type.
 * @param x_vec The left column-first array with `M` rows and `N` columns.
 * @param y_vec The right column-first array with `M` rows and `N` columns.
 * @param bits The vector that contains the 'M' bits to use for swapping (if bits[i]=True, the i-th
 * rows will be swapped).
 */
// TODO (john): Move this to utils
template <typename Share>
static void swap(Vector<Share> &x_vec, Vector<Share> &y_vec, const Vector<Share> &bits) {
    // Make sure the input arrays have the same dimensions
    assert((x_vec.size() > 0) && (x_vec.size() == y_vec.size()) && (bits.size() == x_vec.size()));
    auto b = bits.extend_lsb();
    // Swap elements
    auto tmp = (b & y_vec) ^ (~b & x_vec);
    y_vec = (b & x_vec) ^ (~b & y_vec);
    x_vec = tmp;
}

}  // namespace orq
