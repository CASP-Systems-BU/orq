#pragma once

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

#ifdef __BMI__
#include <immintrin.h>
#endif

#include <cmath>

#include "debug/orq_debug.h"
#include "mapped_iterator.h"

template <typename T>
concept arithmetic = std::integral<T> or std::floating_point<T>;

/**
 * @brief Define a binary operation between two vectors, such as `a - b`.
 *
 */
#define define_binary_vector_op(_op_)                    \
    inline Vector operator _op_(const Vector &y) const { \
        VectorSizeType size = this->size();              \
        Vector res = this->construct_like();             \
        for (VectorSizeType i = 0; i < size; ++i) {      \
            res[i] = (*this)[i] _op_ y[i];               \
        }                                                \
        res.setPrecision(this->getPrecision());          \
        return res;                                      \
    }

/**
 * @brief Define a unary operation with one vector, such as `~a`.
 *
 */
#define define_unary_vector_op(_op_)                \
    inline Vector operator _op_() const {           \
        VectorSizeType size = this->size();         \
        Vector res = this->construct_like();        \
        for (VectorSizeType i = 0; i < size; ++i) { \
            res[i] = _op_(*this)[i];                \
        }                                           \
        res.setPrecision(this->getPrecision());     \
        return res;                                 \
    }

/**
 * @brief Define a binary operation with a vector and an element, such as `a * 2`.
 *
 */
#define define_binary_vector_element_op(_op_)          \
    template <typename S>                              \
        requires arithmetic<S>                         \
    inline Vector operator _op_(const S other) const { \
        VectorSizeType size = this->size();            \
        Vector res = this->construct_like();           \
        for (VectorSizeType i = 0; i < size; ++i) {    \
            res[i] = (*this)[i] _op_ other;            \
        }                                              \
        res.setPrecision(this->getPrecision());        \
        return res;                                    \
    }

/**
 * @brief Define a Vector assignment operator with a Vector, such as `a &= b`.
 *
 */
#define define_binary_vector_assignment_op(_op_)       \
    inline Vector operator _op_(const Vector &other) { \
        VectorSizeType size = this->size();            \
        for (VectorSizeType i = 0; i < size; i++) {    \
            (*this)[i] _op_ other[i];                  \
        }                                              \
        return *this;                                  \
    }

/**
 * @brief Define a Vector assignment operator with an element, such as `a += 1`.
 *
 */
#define define_binary_vector_element_assignment_op(_op_) \
    template <typename S>                                \
        requires arithmetic<S>                           \
    Vector operator _op_(const S other) {                \
        VectorSizeType size = this->size();              \
        for (VectorSizeType i = 0; i < size; i++) {      \
            (*this)[i] _op_ other;                       \
        }                                                \
        return *this;                                    \
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
 * Extracts the bit at `bitIndex` from the given element. Use a dedicated hardware instruction if
 * available (x86 SSE only).
 *
 * @param share The vector element whose bit we want to extract.
 * @param bitIndex The zero-based index (0 is the LSB).
 * @return The extracted bit as a single-bit T element.
 */
template <typename T>
static inline T getBit(const T &share, const int &bitIndex) {
    // Preprocessor tricks to make compiler happy
#ifdef __BMI__
    const bool _use_bmi = true;
#else
#define _bextr_u64(x, y, z)
    const bool _use_bmi = false;
#endif

    // No 128-bit version available.
    if constexpr (_use_bmi && std::numeric_limits<T>::digits <= 64) {
        return _bextr_u64(share, bitIndex, 1);
    } else {
        using Unsigned_type = typename std::make_unsigned<T>::type;
        return ((Unsigned_type)share >> bitIndex) & (Unsigned_type)1;
    }
}

/**
 * Sets the bit at `bitIndex` in element `share` equal to the LSB of element `bit`.
 * @param share The element whose bit we want to update.
 * @param bit The element whose LSB must be copied into `share`.
 * @param bitIndex The zero-based index (0 is the LSB) of the bit to be updated in element
 * `share`.
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
    clrBit(share, bitIndex);
    share |= (value << bitIndex);
}

/**
 * @brief Conditionally set bits in `share`, masked by `mask`, based on the boolean `value` flag.
 *
 * Optimized version from https://graphics.stanford.edu/~seander/bithacks.html
 *
 * It's not immediately clear to me that this is faster, but perhaps better on certain processors,
 * or with certain optimizations enabled.
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
    /**
     * @brief An alias for unsigned `T`.
     *
     */
    using Unsigned_type = typename std::make_unsigned<T>::type;

    /**
     * @brief Number of bits to represent `T`.
     *
     */
    const int MAX_BITS_NUMBER = std::numeric_limits<Unsigned_type>::digits;

    /**
     * The start index of the batch that is currently being processed
     */
    VectorSizeType batch_start = 0;

    /**
     * The end index of the batch that is currently being processed
     */
    VectorSizeType batch_end = 0;

    /**
     * A (shared) pointer to the actual vector contents. NOTE: Shallow copying of this object
     * creates two instances that share the same data.
     */
    std::shared_ptr<std::vector<T>> data;

    /**
     * A (shared) pointer to the vector storing the index mapping for this vector. Can be null, in
     * which case the mapping is defaulted to the identity mapping
     */
    std::shared_ptr<std::vector<VectorSizeType>> mapping;

    // Fixed-point precision
    size_t precision = 0;

   public:
    // Accessible via orq::Vector<T>::value_type
    using value_type = T;

    /**
     * Construct a vector pointing to specific data and mapping
     * Mostly used internally
     *
     * Default mapping is the identity (null pointer)
     */
    Vector(std::shared_ptr<std::vector<T>> _data,
           std::shared_ptr<std::vector<VectorSizeType>> _mapping = nullptr)
        : data(_data),
          mapping(_mapping),
          batch_start(0),
          batch_end(_mapping ? _mapping.get()->size() : _data.get()->size()) {}

    /**
     * Move constructor
     * @param _other The std::vector<T> whose elements will be moved to the new Vector.
     */
    Vector(std::vector<T> &&_other) : Vector(std::make_shared<std::vector<T>>(std::move(_other))) {}

    /**
     * Copy constructor from vector
     * @param _other The std::vector<T> whose elements will be copied to the new Vector.
     */
    Vector(std::vector<T> &_other) : Vector(std::make_shared<std::vector<T>>(_other)) {}

    /**
     * Creates a Vector of `size` values initialize to `init_val` (0 by default).
     * @param _size The size of the new Vector.
     * @param _init_val default-initialized value
     */
    Vector(VectorSizeType _size, T _init_val = 0)
        : Vector(std::make_shared<std::vector<T>>(_size, _init_val)) {}

    /**
     * Constructs a new Vector from a list of `T` elements.
     * @param elements The list of elements of the new Vector.
     */
    Vector(std::initializer_list<T> &&elements)
        : Vector(std::make_shared<std::vector<T>>(elements)) {}

    /**
     * Copy constructor from range
     * @param _other The input_range whose elements will be copied to the new Vector.
     */
    template <std::ranges::input_range IR>
    Vector(IR _other) : Vector(std::make_shared<std::vector<T>>(_other.begin(), _other.end())) {}

    /**
     * This is a shallow copy constructor.
     * @param other The vector that contains the std::vector<T> pointer to be copied.
     *
     * WARNING: The new vector will point to the same memory location used by `other`. To copy the
     * data into a separate memory location, create a new vector first then use assignment operator.
     */
    Vector(const Vector &other)
        : data(other.data),
          mapping(other.mapping),
          batch_start(other.batch_start),
          batch_end(other.batch_end),
          precision(other.precision) {}

    /**
     * This is a shallow copy constructor.
     * @param other The vector that contains the std::vector<T> pointer to be copied.
     *
     * WARNING: The new vector will point to the same memory location used by `other`. To copy the
     * data into a separate memory location, create a new vector first then use assignment operator.
     */
    Vector(Vector &other)
        : data(other.data),
          mapping(other.mapping),
          batch_start(other.batch_start),
          batch_end(other.batch_end),
          precision(other.precision) {}

    /**
     * Creates a new Vector with the same structure as this Vector,
     * but with newly allocated empty vectors of the same size.
     * @return A new Vector with the same structure but empty contents.
     */
    Vector construct_like() const {
        Vector result(this->size());
        return result;
    }

    /**
     * @brief Constructor that converts a vector of floating-point numbers to a vector of integers.
     *
     * @tparam FP - The floating-point type to convert from.
     * @param _other - The vector of floating-point numbers to convert.
     * @param fixed_point_precision - The number of fractional bits to use for the fixed-point
     * conversion. Default is 16.
     */
    template <std::floating_point FP>
        requires std::integral<T>
    Vector(const std::vector<FP> &_other, int fixed_point_precision = 16)
        : Vector(static_cast<VectorSizeType>(_other.size())) {
        precision = fixed_point_precision;
        const long double scale = std::ldexp(1.0L, fixed_point_precision);  // 2^{precision}
        for (VectorSizeType i = 0; i < _other.size(); ++i) {
            (*this)[i] = static_cast<T>(std::llround(_other[i] * scale));
        }
    }

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
        VectorSizeType size = this->size();
        Vector res = this->construct_like();
        for (VectorSizeType i = 0; i < size; ++i) {
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
        VectorSizeType size = this->size();
        Vector res = this->construct_like();
        for (VectorSizeType i = 0; i < size; ++i) {
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
        VectorSizeType size = this->size();
        Vector res = this->construct_like();
        for (VectorSizeType i = 0; i < size; ++i) {
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
        VectorSizeType size = this->size();
        Vector res = this->construct_like();

        for (VectorSizeType i = 0; i < size; ++i) {
            res[i] = std::popcount(static_cast<Unsigned_type>((*this)[i])) & 1;
        }

        return res;
    }

    /**
     * @brief Compute a prefix sum of this vector. Operates in place; for
     * immutable operation, first copy into a new vector.
     *
     */
    inline void prefix_sum() { std::inclusive_scan(begin(), end(), begin()); }

    /**
     * @brief Arbitrary-operation prefix "sum". Operation should be
     * associative.
     *
     */
    inline void prefix_sum(const T &(*op)(const T &, const T &)) {
        std::inclusive_scan(begin(), end(), begin(), op);
    }

    /**
     * @brief Sums each consecutive `aggSize` vector elements and returns a new vector
     * containing the aggregated sums.
     * @param aggSize The number of elements to aggregate in each sum.
     * @return A new Vector that contains the aggregated sums.
     */
    Vector chunkedSum(const VectorSizeType aggSize = 0) const {
        // If aggSize is 0, we sum all elements
        const VectorSizeType aggSize_ = (aggSize == 0) ? this->size() : aggSize;

        const VectorSizeType s = this->size();
        const VectorSizeType newSize = div_ceil(s, aggSize_);

        Vector res(newSize, 0);
        VectorSizeType j = 0;
        for (VectorSizeType i = 0; i < newSize; ++i) {
            T sum = 0;
            const VectorSizeType end = std::min(j + aggSize_, s);
            for (; j < end; ++j) {
                sum += (*this)[j];
            }
            res[i] = sum;
        }

        return res;
    }

    /**
     * Computes the dot product of this vector with another vector, aggregating results in chunks of
     * `aggSize`.
     * Each `aggSize` consecutive elements contribute to an exactly on dot product element in the
     * result. The size of the resulting vector is determined by the `aggSize` parameter.
     * @param other The other vector to compute the dot product with.
     * @param aggSize The number of elements to do dotproduct on for each result element.
     * @return A new Vector containing the aggregated dot product results.
     *
     */
    Vector dot_product(const Vector &other, const VectorSizeType aggSize = 0) const {
        // If aggSize is 0, we compute the dot product of all elements
        const VectorSizeType aggSize_ = (aggSize == 0) ? this->size() : aggSize;

        assert(this->size() == other.size());
        const VectorSizeType s = this->size();
        const VectorSizeType newSize = div_ceil(s, aggSize_);

        Vector res(newSize);
        VectorSizeType j = 0;
        for (VectorSizeType i = 0; i < newSize; ++i) {
            T sum = 0;
            const VectorSizeType end = std::min(j + aggSize_, s);
            for (; j < end; ++j) {
                sum += (*this)[j] * other[j];
            }
            res[i] = sum;
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
    Vector simple_subset(const VectorSizeType &start, const VectorSizeType &step,
                         const VectorSizeType &end) const {
        VectorSizeType res_size = end - start + 1;

        Vector res = this->construct_like();

        for (VectorSizeType i = 0; i < res_size; i += step) {
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
    void set_batch(const VectorSizeType &_start_ind, const VectorSizeType &_end_ind) {
        batch_start = (_start_ind >= 0) ? _start_ind : 0;
        batch_end = (_end_ind <= this->total_size()) ? _end_ind : this->total_size();
    }

    /**
     * @return The total number of elements in the vector.
     */
    inline VectorSizeType total_size() const {
        if (has_mapping()) {
            return this->mapping->size();
        } else {
            return this->data->size();
        }
    }

    using IteratorType = MappedIterator<T, typename std::vector<T>::iterator,
                                        typename std::vector<VectorSizeType>::iterator>;

    /**
     * @return An iterator pointing to the first element.
     *
     * NOTE: This method is used by the communicator.
     */
    inline IteratorType begin() const {
        if (has_mapping()) {
            return IteratorType(data->begin(), mapping->begin());
        } else {
            return IteratorType(data->begin());
        }
    }

    /**
     * @return An iterator pointing to the last element.
     *
     * NOTE: This method is used by the communicator.
     */
    inline IteratorType end() const {
        if (has_mapping()) {
            return IteratorType(data->begin(), mapping->end());
        } else {
            return IteratorType(data->end());
        }
    }

    /**
     * @brief Return a new C++ vector with the same data. This is not a reference to the underlying
     * storage.
     *
     * @return std::vector<T>
     */
    std::vector<T> as_std_vector() const { return std::vector(this->begin(), this->end()); }

    /**
     * @brief Return the underlying storage of this Vector.
     *
     * @return std::vector<T>
     */
    std::vector<T> _get_internal_data() const { return *data; }

    /**
     * @brief Return a span with a view of the underlying data
     *
     * @return std::span<T>
     */
    std::span<T> span() { return std::span<T>(*data); }

    /**
     * @brief Return an unmapped span to the current batch. Useful for protocols that need direct
     * access to the underlying storage.
     *
     * @return std::span<T>
     */
    std::span<T> batch_span() {
        assert(!has_mapping());
        return std::span<T>(data->begin() + batch_start, size());
    }

    /**
     * @brief Const version of the above.
     *
     * @return std::span<const T>
     */
    std::span<const T> batch_span() const {
        assert(!has_mapping());
        return std::span<const T>(data->begin() + batch_start, size());
    }

    /**
     * @brief Checks whether a mapping exists inside this Vector. Note that this function only
     * checks for existence; thus, a vector with the trivial mapping would return `true` here.
     *
     * @return true if the Vector has a mapping
     * @return false if it does not
     */
    bool has_mapping() const { return mapping != nullptr; }

    /**
     * @brief Remaps the vector to reference a subset of the original vector. Returned Vector points
     * to the same underlying storage.
     *
     * Note: end index is inclusive. To access a single element, use `simple_subset_reference(i, 1,
     * i)`.
     *
     * @param _start_index
     * @param _step
     * @param _end_index **inclusive** end index
     * @return Vector
     */
    Vector simple_subset_reference(const VectorSizeType _start_index, const VectorSizeType _step,
                                   const VectorSizeType _end_index) const {
        if (_step == 1) {
            return slice(_start_index, _end_index + 1);
        }

        VectorSizeType size = std::min(this->total_size(), (_end_index - _start_index) / _step + 1);
        auto new_mapping = std::make_shared<std::vector<VectorSizeType>>(size);
        for (VectorSizeType i = 0, j = _start_index; i < size; ++i, j += _step) {
            // TODO: try moving the if outside the for loop
            (*new_mapping)[i] = mapping ? (*mapping)[j] : j;
        }

        return Vector<T>(data, new_mapping);
    }

    /**
     * @brief Simple subset reference with implicit end index (at the end of the Vector).
     *
     * @param _start_index
     * @param _step
     * @return Vector
     */
    Vector simple_subset_reference(const VectorSizeType _start_index,
                                   const VectorSizeType _step) const {
        return simple_subset_reference(_start_index, _step,
                                       std::max<VectorSizeType>(0, this->total_size() - 1));
    }

    /**
     * @brief Simple subset reference with implicit step size (1) and end.
     *
     * @param _start_index
     * @return Vector
     */
    Vector simple_subset_reference(const VectorSizeType _start_index) const {
        return simple_subset_reference(_start_index, 1,
                                       std::max<VectorSizeType>(0, this->total_size() - 1));
    }

    /**
     * @brief Take a slice of a vector. This is the same as simple_subset_reference, but the end
     * index is EXCLUSIVE. It also only supports a step size of 1.
     *
     * The resulting slice will have size `end - start`. `slice` expresses natural ranges, e.g.,
     * `slice(x, x + s)` represents the slice starting at `x` having size `s`.
     *
     * - `slice(x)` is equivalent to `simple_subset_reference(x)`
     * - `slice(x, y)` is equivalent to `simple_subset_reference(x, 1, y - 1)`
     *
     * @param start
     * @param end **exclusive** end index
     * @return Vector
     */
    Vector slice(const size_t start, const size_t end) const {
        size_t n = std::min(end - start, size());
        auto new_mapping = std::make_shared<std::vector<VectorSizeType>>(n);
        if (has_mapping()) {
            std::copy(mapping->begin() + start, mapping->begin() + end, new_mapping->begin());
        } else {
            std::iota(new_mapping->begin(), new_mapping->end(), start);
        }
        return Vector<T>(data, new_mapping);
    }

    /**
     * @brief Take a slice with an implicit end index (at the end of the Vector).
     *
     * @param start
     * @return Vector
     */
    Vector slice(const size_t start) const { return slice(start, size()); }

    /**
     * @brief Return a view of this vector with only the nonzero positions of `flag` included. For
     * example, if the base vector is `[ 1 2 3 4 5 6 ]` and the flag vector is `[ 0 0 1 1 0 1 ]`
     * then this access pattern returns `[ 3 4 6 ]`.
     *
     * If flag is shorter than this vector, assume all remaining flag values are zero.
     *
     * NOTE: should we ever need to parallelize this operation, it can be
     * implemented by
     * - (parallel) prefix sum over `flag` vector
     * - multiply prefix sum with original `flag` vector, placing -1 at all locations where flag=0
     *   and the prefix sum value where flag=1
     * - (parallel) copy from non-negative indices
     *
     * I think this actually needs to be an `exclusive_sum`, not `inclusive` but this can also be
     * implemented by seeding the prefix sum with -1.
     *
     * @param flag
     * @return Vector
     */
    Vector included_reference(const Vector flag) const {
        // We won't need the entire thing, so resize after: but don't know
        // a priori how large the output is.
        VectorSizeType upper_bound_size = std::min(this->total_size(), flag.total_size());
        auto new_mapping = std::make_shared<std::vector<VectorSizeType>>(upper_bound_size);

        VectorSizeType mi = 0;
        for (VectorSizeType fi = 0; fi < upper_bound_size; fi++) {
            if (flag[fi] != 0) {
                (*new_mapping)[mi++] = mapping ? (*mapping)[fi] : fi;
            }
        }

        // We only used `mi` indices of the mapping; resize it.
        new_mapping->resize(mi);

        return Vector<T>(data, new_mapping);
    }

    /**
     * Applies an alternating pattern to include and exclude elements. Keep alternating elements
     * as included in the pattern or excluded from the pattern using the `_subset_included_size`
     * and the `_subset_excluded_size` for each included or excluded chunk.
     *
     * @param _subset_included_size the size of a number of elements that we choosing from.
     * @param _subset_excluded_size the size of a number of elements that we are totally not
     * choosing.
     * @return `Vector` that points to the same memory location as the original vector but with
     * different index mapping.
     */
    Vector alternating_subset_reference(const VectorSizeType _subset_included_size,
                                        VectorSizeType _subset_excluded_size) const {
        if (_subset_excluded_size == -1) {
            _subset_excluded_size = this->total_size() - _subset_included_size;
        }

        VectorSizeType size =
            this->total_size() / (_subset_included_size + _subset_excluded_size) *
                (_subset_included_size) +
            std::min(_subset_included_size,
                     this->total_size() % (_subset_included_size + _subset_excluded_size));
        auto new_mapping = std::make_shared<std::vector<VectorSizeType>>(size);
        auto chunk_size = _subset_included_size + _subset_excluded_size;
        VectorSizeType i = 0;
        VectorSizeType j = 0;
        while (i < size) {
            for (VectorSizeType k = 0; i < size && k < _subset_included_size; ++k) {
                (*new_mapping)[i++] = mapping ? (*mapping)[j + k] : j + k;
            }
            j += chunk_size;
        }
        return Vector<T>(data, new_mapping);
    }

    /**
     * @brief The same as above, but counting from the end of the Vector.
     *
     * @param _subset_included_size
     * @param _subset_excluded_size
     * @return Vector
     */
    Vector reversed_alternating_subset_reference(const VectorSizeType _subset_included_size,
                                                 VectorSizeType _subset_excluded_size) const {
        if (_subset_excluded_size == -1) {
            _subset_excluded_size = this->total_size() - _subset_included_size;
        }

        auto chunk_size = _subset_included_size + _subset_excluded_size;
        auto full_chunks = this->total_size() / chunk_size;
        auto last_chunk_size =
            std::min(_subset_included_size,
                     this->total_size() % (_subset_included_size + _subset_excluded_size));
        VectorSizeType size = full_chunks * _subset_included_size + last_chunk_size;
        auto new_mapping = std::make_shared<std::vector<VectorSizeType>>(size);
        VectorSizeType i = 0;
        VectorSizeType chunk_end = _subset_included_size - 1;
        for (VectorSizeType chunk = 0; chunk < full_chunks; ++chunk) {
            for (VectorSizeType j = 0; j < _subset_included_size; ++j) {
                (*new_mapping)[i++] = mapping ? (*mapping)[chunk_end - j] : chunk_end - j;
            }
            chunk_end += chunk_size;
        }
        chunk_end = this->total_size() - 1;
        for (VectorSizeType j = 0; j < last_chunk_size; ++j) {
            (*new_mapping)[i++] = mapping ? (*mapping)[chunk_end - j] : chunk_end - j;
        }
        return Vector<T>(data, new_mapping);
    }

    /**
     * Applies a new indexing mapping to the current vector so that each element is repeated a
     * number of times consecutively.
     * @param _subset_repetition the number of times each element is repeated.
     * @return `Vector` that points to the same memory location as the original vector but with
     * different index mapping.
     */
    Vector repeated_subset_reference(const VectorSizeType _subset_repetition) const {
        VectorSizeType size = this->total_size() * _subset_repetition;
        auto new_mapping = std::make_shared<std::vector<VectorSizeType>>(size);
        auto orig_size = this->total_size();
        auto i = 0;
        for (auto j = 0; j < orig_size; ++j) {
            for (auto k = 0; k < _subset_repetition; ++k) {
                (*new_mapping)[i++] = mapping ? (*mapping)[j] : j;
            }
        }

        return Vector<T>(data, new_mapping);
    }

    /**
     * Applies a new indexing mapping such that after accessing the last element, we access the
     * first element again and keep accessing the elements in cycles.
     * @param _subset_cycles the number of cycles the new indexing mapping will contain.
     * @return `Vector` that points to the same memory location as the original vector but with
     * different index mapping.
     */
    Vector cyclic_subset_reference(const VectorSizeType _subset_cycles) const {
        VectorSizeType size = this->total_size() * _subset_cycles;
        auto new_mapping = std::make_shared<std::vector<VectorSizeType>>(size);
        auto orig_size = this->total_size();
        auto i = 0;
        for (auto j = 0; j < _subset_cycles; ++j) {
            for (auto k = 0; k < orig_size; ++k) {
                (*new_mapping)[i++] = mapping ? (*mapping)[k] : k;
            }
        }

        return Vector<T>(data, new_mapping);
    }

    /**
     * Applies a new mapping indexing that controls the order in which the elements accessed.
     * @param _subset_direction set to (1) to keep current order or (-1) to reverse the order.
     * @return `Vector` that points to the same memory location as the original vector but with
     * different index mapping.
     */
    Vector directed_subset_reference(const int _subset_direction) const {
        if (_subset_direction == -1) {
            size_t size = this->total_size();
            auto new_mapping = std::make_shared<std::vector<VectorSizeType>>(size);
            for (size_t i = 0, j = size - 1; i < size; ++i, --j) {
                (*new_mapping)[i] = mapping ? (*mapping)[j] : j;
            }
            return Vector<T>(data, new_mapping);
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
    Vector simple_bit_compress(const int &start, const int &step, const int &end,
                               const int &repetition) const {
        const int _step = step;
        const int _repetition = repetition;

        const int bits_per_element = std::abs(((end - start + 1) / step) * repetition);
        const VectorSizeType total_bits = bits_per_element * this->size();
        const VectorSizeType total_new_elements = div_ceil(total_bits, MAX_BITS_NUMBER);

        Vector res(total_new_elements);

        // TODO: factor out division here, keep two counters?
        for (VectorSizeType i = 0, j = 0; j < total_bits; i++, j += MAX_BITS_NUMBER) {
            auto r = &res[i];
            for (VectorSizeType k = j, p = 0; p < MAX_BITS_NUMBER && k < total_bits; k++, p++) {
                setBitValue(*r,
                            getBit((*this)[k / bits_per_element],
                                   start + (k % bits_per_element) / _repetition * _step),
                            p);
            }
        }

        return res;
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
    void simple_bit_decompress(const Vector &other, const int &start, const int &step,
                               const int &end, const int &repetition) {
        const int bits_per_element = std::abs(((end - start + 1) / step) * repetition);
        const VectorSizeType total_bits = bits_per_element * this->size();
        const VectorSizeType total_new_elements = div_ceil(total_bits, MAX_BITS_NUMBER);

        for (VectorSizeType i = 0, j = 0; j < total_bits; i++, j += MAX_BITS_NUMBER) {
            auto r = other[i];
            for (VectorSizeType k = j, p = 0; p < MAX_BITS_NUMBER && k < total_bits; k++, p++) {
                setBitValue((*this)[k / bits_per_element], getBit(r, p),
                            start + (k % bits_per_element) / repetition * step);
            }
        }
    }

    /**
     * Extracts the bit at a given position from each element of the source Vector and stores it in
     * this Vector
     * @param source The Vector to take bits from
     * @param position The bit position to take
     */
    void pack_from(const Vector &source, const int &position) {
        const VectorSizeType base_index = batch_start * MAX_BITS_NUMBER;
        const VectorSizeType total_bits =
            std::min(this->size() * MAX_BITS_NUMBER, source.size() - base_index);

        for (VectorSizeType i = 0, j = 0; j < total_bits; i++, j += MAX_BITS_NUMBER) {
            T r = 0;
            // auto r = &res[i];
            for (VectorSizeType k = j, p = 0; p < MAX_BITS_NUMBER && k < total_bits; k++, p++) {
                // // This works but is a bit slower:
                // r |= _pdep_u64((*this)[j + k] >> position, 1 << k);

                // Extract the bit at `position` and shift it into `r`
                r |= getBit(source[k + base_index], position) << p;
            }
            (*this)[i] = r;
        }
    }

    /**
     * The inverse of pack_from, takes a packed Vector and puts its bits at a position in this
     * Vector. WARNING: Requires that `batch_start` be a multiple of the bit-length of a share ffor
     * proper alignment.
     * @param source The packed Vector to take bits from
     * @param position The bit position to put the bits in
     */
    void unpack_from(const Vector &source, const T &position) {
        const VectorSizeType total_bits = this->size();
        // Restrict batch size to a multiple of MAX_BITS_NUMBER
        // because we assume we're starting at boundary of a packed element
        assert(batch_start % MAX_BITS_NUMBER == 0);
        const VectorSizeType base_index = batch_start / MAX_BITS_NUMBER;

        for (VectorSizeType i = 0, j = 0; j < total_bits; i++, j += MAX_BITS_NUMBER) {
            auto r = source[i + base_index];
            for (VectorSizeType k = j, p = 0; p < MAX_BITS_NUMBER && k < total_bits; k++, p++) {
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
     * least significant first. `-1` means most significant first. (default: `1`, LSB first.)
     * @return a new `Vector` that has only the chosen bits in its elements (less size than input).
     */
    Vector alternating_bit_compress(const VectorSizeType &start, const VectorSizeType &step,
                                    const VectorSizeType &included_size,
                                    const VectorSizeType &excluded_size,
                                    const int &direction = 1) const {
        const VectorSizeType bits_per_chunk = included_size / step;
        const VectorSizeType bits_per_element =
            (MAX_BITS_NUMBER - start) / (included_size + excluded_size) * bits_per_chunk;
        const VectorSizeType last_chunk_bits_per_element =
            std::min(included_size, ((MAX_BITS_NUMBER - start) % (included_size + excluded_size))) /
            step;
        const VectorSizeType total_bits_per_element =
            bits_per_element + last_chunk_bits_per_element;

        const int direction_offset = (direction == -1) ? included_size - 1 : 0;

        const VectorSizeType total_bits = total_bits_per_element * this->size();
        const VectorSizeType total_new_elements = div_ceil(total_bits, MAX_BITS_NUMBER);

        Vector res(total_new_elements);

        for (VectorSizeType i = 0; i < total_bits; ++i) {
            const VectorSizeType element_index = i / total_bits_per_element;
            const VectorSizeType element_chunk_index =
                (i % total_bits_per_element) / bits_per_chunk;
            const VectorSizeType element_bit_index =
                start + direction_offset + element_chunk_index * (included_size + excluded_size) +
                ((i % total_bits_per_element) % bits_per_chunk) * step * direction;
            setBit(res[i / MAX_BITS_NUMBER], getBit((*this)[element_index], element_bit_index),
                   i % MAX_BITS_NUMBER);
        }

        return res;
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
     * least significant first. `-1` means most significant first. (default: `1`)
     */
    void alternating_bit_decompress(const Vector &other, const VectorSizeType &start,
                                    const VectorSizeType &step, const VectorSizeType &included_size,
                                    const VectorSizeType &excluded_size,
                                    const int &direction = 1) const {
        const VectorSizeType bits_per_chunk = included_size / step;
        const VectorSizeType bits_per_element =
            (MAX_BITS_NUMBER - start) / (included_size + excluded_size) * bits_per_chunk;
        const VectorSizeType last_chunk_bits_per_element =
            std::min(included_size, ((MAX_BITS_NUMBER - start) % (included_size + excluded_size))) /
            step;
        const VectorSizeType total_bits_per_element =
            bits_per_element + last_chunk_bits_per_element;

        const int direction_offset = (direction == -1) ? included_size - 1 : 0;

        const VectorSizeType total_bits = total_bits_per_element * this->size();
        const VectorSizeType total_new_elements = div_ceil(total_bits, MAX_BITS_NUMBER);

        for (VectorSizeType i = 0; i < total_bits; ++i) {
            const VectorSizeType element_index = i / total_bits_per_element;
            const VectorSizeType element_chunk_index =
                (i % total_bits_per_element) / bits_per_chunk;
            const VectorSizeType element_bit_index =
                start + direction_offset + element_chunk_index * (included_size + excluded_size) +
                ((i % total_bits_per_element) % bits_per_chunk) * step * direction;
            setBit((*this)[element_index], getBit(other[i / MAX_BITS_NUMBER], i % MAX_BITS_NUMBER),
                   element_bit_index);
        }
    }

    /**
     * @brief Materialize a vector which might have an access pattern applied. If there is no
     * mapping, just return the vector. Otherwise, copy the mapped vector into a new vector (which
     * will have no map).
     *
     * Useful for communication primitives, which require unmapped vectors.
     *
     * @return Vector<T>
     */
    Vector<T> materialize() const {
        if (has_mapping()) {
            Vector<T> res = this->construct_like();
            res = *this;
            return res;
        } else {
            return *this;
        }
    }

    /**
     * @brief Materialize a Vector in place: construct a new Vector, copy the data over from this
     * Vector into the new one, and reset the mapping. Externally, this function has no side
     * effects, but may be necessary before communicating Vectors.
     *
     * If this Vector does not have a mapping, it is already materialized, so we do nothing.
     *
     */
    void materialize_inplace() {
        if (has_mapping()) {
            Vector<T> res = this->construct_like();
            res = *this;
            data = std::move(res.data);
            // Goodbye mapping
            mapping.reset();
            reset_batch();
        }
    }

    /**
     * @brief Create a mapping reference, where the std::vector argument `map` will become the new
     * map. Not allowed if a mapping is already applied.
     *
     * @param map std::vector specifying the new map
     * @return Vector
     */
    Vector mapping_reference(std::vector<VectorSizeType> map) const {
        assert(!has_mapping());
        auto new_mapping = std::make_shared<std::vector<VectorSizeType>>(std::move(map));
        return Vector<T>(data, new_mapping);
    }

    /**
     * @brief Create a mapping reference with a `std::vector<S>` map, for arbitrary type `S`.
     *
     * @tparam S
     * @param map
     * @return Vector
     */
    template <typename S>
    Vector mapping_reference(std::vector<S> map) const {
        assert(!has_mapping());
        auto new_mapping = std::make_shared<std::vector<VectorSizeType>>(map.size());
        std::copy(map.begin(), map.end(), new_mapping->begin());
        return Vector<T>(data, new_mapping);
    }

    /**
     * @brief Create a mapping reference, where the Vector argument `map` will become the new map.
     * Not allowed if a mapping is already applied.
     *
     * @param map ORQ Vector specifying the new map
     * @return Vector
     */
    template <typename S>
    Vector mapping_reference(Vector<S> map) const {
        assert(!has_mapping());
        auto new_mapping = std::make_shared<std::vector<VectorSizeType>>(map.size());
        std::copy(map.begin(), map.end(), new_mapping->begin());
        return Vector<T>(data, new_mapping);
    }

    /**
     * @brief Compose mappings. Apply the new mapping on top of an existing mapping, should one
     * exist.
     *
     * Specifically, assume `x` is the underlying storage. Then let `x1` be the vector with mapping
     * `m1` applied; `x1[i] = x[m1[i]]`.
     *
     * Applying map `m2` with this function gives `x2`, such that `x2[i] = x[m1[m2[i]]]`.
     *
     * The new mapping specified must be the same size as, or smaller than, the old one (this
     * function cannot expand a vector).
     *
     * @tparam S
     * @param new_mapping
     */
    template <typename S = VectorSizeType>
    void apply_mapping(std::vector<S> new_mapping) {
        auto size = new_mapping.size();

        // Make sure we're not expanding by accident
        assert(size <= this->size());

        // If no mapping yet, just use what was passed if same type, or seed with iota
        if (!has_mapping()) {
            if constexpr (std::is_same_v<S, VectorSizeType>) {
                mapping = std::make_shared<std::vector<VectorSizeType>>(std::move(new_mapping));
            } else {
                // different type
                mapping = std::make_shared<std::vector<VectorSizeType>>(size);
                std::copy(new_mapping.begin(), new_mapping.end(), mapping->begin());
            }
            return;
        }

        // Already a mapping, so we need to compose. Need a temporary so we don't clobber existing
        // This is basically permutation composition.
        auto temp = std::make_shared<std::vector<VectorSizeType>>(size);

        for (int i = 0; i < size; i++) {
            (*temp)[i] = (*mapping)[new_mapping[i]];
        }
        mapping = std::move(temp);
    }

    /**
     * @brief Use iterators to reverse this Vector in place.
     *
     */
    void reverse() { std::reverse(begin(), end()); }

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
        VectorSizeType size = this->size();
        assert(size == other.size());
        for (VectorSizeType i = 0; i < size; ++i) {
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
        VectorSizeType size = this->size();
        assert(size == other.size());
        for (VectorSizeType i = 0; i < size; ++i) {
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
        VectorSizeType size = this->size();
        assert(size == other.size());
        for (VectorSizeType i = 0; i < size; i++) {
            (*this)[i] = (T)other[i];
        }
        precision = other.getPrecision();
        return *this;
    }

    /**
     * Returns a new vector that contains all elements in the range [start, start + size].
     * @param start The index of the first element to be included in the output vector.
     * @param size The number of elements to include
     * @return A new vector that contains the selected elements.
     *
     * NOTE: This method works relatively to the current batch.
     */
    Vector simple_subset(const VectorSizeType &start, const VectorSizeType &size) const {
        Vector res = this->construct_like();

        for (VectorSizeType i = 0; i < size; ++i) {
            res[i] = (*this)[start + i];
        }
        return res;
    }

    /**
     * Masks each element in `this` vector by doing a bitwise logical AND with `n`.
     * TODO: factor out into common macro for other operations using
     * define_binary_vector_element_assignment_op
     * @param n The mask.
     */
    void mask(const T &n) {
        for (VectorSizeType i = 0; i < this->size(); ++i) {
            (*this)[i] &= n;
        }
    }

    /**
     * Sets the bits of each element in `this` vector by doing a bitwise logical OR with `n`
     * @param n The element that encodes the bits to set.
     */
    void set_bits(const T &n) {
        for (VectorSizeType i = 0; i < this->size(); ++i) {
            (*this)[i] |= n;
        }
    }

    /**
     * Sets every element of this vector to zero. Don't modify the mapping.
     * (If a mapping is applied, only mapped values will be zero'd.)
     *
     * NOTE: this method works relative to the current batch.
     */
    void zero() { std::fill(begin() + batch_start, begin() + batch_end, 0); }

    /**
     * @return The number of elements in the vector.
     *
     * NOTE: This method works relatively to the current batch.
     */
    inline VectorSizeType size() const { return batch_end - batch_start; }

    /**
     * @brief Resize this vector.
     * - If a mapping is applied, resize both the mapping and the underlying data. Point new indices
     * to new storage.
     * - If no mapping is applied, just resize the underlying data.
     *
     * This method resets the current batch.
     *
     * @param n
     */
    void resize(size_t n) {
        if (has_mapping()) {
            size_t old_size = total_size();
            mapping->resize(n);
            int64_t n_new_elm = n - old_size;

            if (n_new_elm > 0) {
                // grew - need to add this many new elements to data
                size_t data_old_size = data->size();
                data->resize(data_old_size + n_new_elm);

                // new indices for mapping point to the newly added elements.
                std::iota(mapping->begin() + old_size, mapping->end(), data_old_size);
            }
        } else {
            // no mapping, just change data
            data->resize(n);
        }

        reset_batch();
        assert(total_size() == n);
    }

    /**
     * @brief Resize this vector to its last \f$n\f$ elements. If there is a mapping, erase all but
     * the last \f$n\f$ elements of the mapping. Otherwise, erase the underlying data. For the
     * _head_ of a Vector, just use `resize`.
     *
     * @param n
     */
    void tail(size_t n) {
        auto n_remove = total_size() - n;

        if (has_mapping()) {
            mapping->erase(mapping->begin(), mapping->begin() + n_remove);
        } else {
            // no mapping. can actually erase data
            data->erase(data->begin(), data->begin() + n_remove);
        }

        reset_batch();
        assert(total_size() == n);
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
    define_binary_vector_assignment_op(%=);
    define_binary_vector_assignment_op(/=);
    define_binary_vector_assignment_op(&=);
    define_binary_vector_assignment_op(|=);
    define_binary_vector_assignment_op(^=);

    define_binary_vector_element_assignment_op(+=);
    define_binary_vector_element_assignment_op(-=);
    define_binary_vector_element_assignment_op(*=);
    define_binary_vector_element_assignment_op(%=);
    define_binary_vector_element_assignment_op(/=);
    define_binary_vector_element_assignment_op(&=);
    define_binary_vector_element_assignment_op(|=);
    define_binary_vector_element_assignment_op(^=);

    /**
     * Elementwise plaintext less-than-zero comparison.
     */
    inline Vector ltz() const {
        VectorSizeType size = this->size();
        Vector res = this->construct_like();
        for (VectorSizeType i = 0; i < size; i++) {
            res[i] = ((*this)[i] < (T)0);
        }
        return res;
    }

    /**
     * Elementwise plaintext LSB extension: set all bits equal to the LSB.
     * Note: this is only makes sense for bit shares.
     */
    inline Vector extend_lsb() const {
        VectorSizeType size = this->size();
        Vector res = this->construct_like();
        for (VectorSizeType i = 0; i < size; i++)
            res[i] = -((*this)[i] & 1);  // Relies on two's complement
        return res;
    }

    /**
     * @brief Extract the valid elements from a Vector, as denoted by the passed flag vector.
     * Basically a selection function. Used in table operations to filter out invalid rows.
     *
     * @param valid
     * @return Vector
     */
    inline Vector extract_valid(Vector valid) {
        assert(this->size() == valid.size());
        std::vector<T> r;
        for (VectorSizeType i = 0; i < valid.size(); i++) {
            if (valid[i] != 0) {
                r.push_back((*this)[i]);
            }
        }
        return r;
    }

    /**
     * @brief Perform division-with remainder in a single loop, to optimize cache performance.
     *
     * @param d
     * @return std::pair<Vector, Vector> a pair of `(quotient, remainder)`
     */
    std::pair<Vector, Vector> divrem(const T d) {
        auto n = this->size();
        Vector q = this->construct_like();
        Vector r = this->construct_like();
        for (size_t i = 0; i < n; i++) {
            q[i] = (*this)[i] / d;
            r[i] = (*this)[i] % d;
        }
        return {q, r};
    }

    /**
     * Returns a mutable reference to the element at the given `index`.
     * NOTE: This method works relatively to the current batch.
     *
     * @param index The index of the target element.
     * @return A mutable reference to the element at the given `index`.
     */
    inline T &operator[](const VectorSizeType &index) {
        if (has_mapping()) {
            return (*data)[(*mapping)[batch_start + index]];
        } else {
            return (*data)[batch_start + index];
        }
    }

    /**
     * Returns an immutable reference of the element at the given `index`.
     *
     * NOTE: This method works relatively to the current batch.
     * @param index The index of the target element.
     * @return Returns a read-only reference of the element at the given `index`.
     */
    inline const T &operator[](const VectorSizeType &index) const {
        if (has_mapping()) {
            return (*data)[(*mapping)[batch_start + index]];
        } else {
            return (*data)[batch_start + index];
        }
    }

    /**
     * @brief Checks if the two input vectors (`this` and `other`) contain the same elements.
     *
     * @param other
     * @param print_warn Print a warning if the Vectors do not match. Useful for test scripts but
     * should be disabled in protocols or production code. This setting can also be controlled at
     * compile time by the directive `DEBUG_VECTOR_SAME_AS`. (Default: `true`)
     * @return true the Vectors are the same
     * @return false they are not
     */
    bool same_as(const Vector<T> &other, bool print_warn = true) const {
        if (this->size() != other.size()) {
#ifdef DEBUG_VECTOR_SAME_AS
            if (print_warn) {
                std::cout << "[same_as]: size mismatch: this size " << this->size()
                          << " != " << other.size() << "\n";
            }
#endif
            return false;
        }

        for (VectorSizeType i = 0; i < other.size(); i++) {
            if ((*this)[i] != other[i]) {
#ifdef DEBUG_VECTOR_SAME_AS
                if (print_warn) {
                    if constexpr (std::is_same<T, int8_t>::value) {
                        std::cout << "[same_as]: mismatch @ " << i << ": this "
                                  << (int32_t)(*this)[i] << " != " << (int32_t)other[i] << "\n";
                    } else {
                        std::cout << "[same_as]: mismatch @ " << i << ": this " << (*this)[i]
                                  << " != " << other[i] << "\n";
                    }
                }
#endif
                return false;
            }
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

        for (VectorSizeType i = 0; i < prefix.size(); i++) {
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
    const auto cols_num = x_vec.size();  // Number of keys
    // Compare elements on first key
    Vector<Share> *t = inverse[0] ? y_vec[0] : x_vec[0];
    Vector<Share> *o = inverse[0] ? x_vec[0] : y_vec[0];
    Vector<Share> eq = (*t == *o);
    Vector<Share> gt = (*t > *o);

    // Compare elements on remaining keys
    for (auto i = 1; i < cols_num; ++i) {
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
 * @param bits The vector that contains the 'M' bits to use for swapping (if bits[i]=True, the
 * i-th rows will be swapped).
 */
// TODO (john): Move this to utils
template <typename Share>
static void swap(std::vector<Vector<Share> *> &x_vec, std::vector<Vector<Share> *> &y_vec,
                 const Vector<Share> &bits) {
    // Make sure the input arrays have the same dimensions
    assert((x_vec.size() > 0) && (x_vec.size() == y_vec.size()));
    const auto cols_num = x_vec.size();  // Number of columns
    for (int i = 0; i < cols_num; ++i) {
        auto xi_size = x_vec[i]->size();
        assert((xi_size == y_vec[i]->size()) && (bits.size() == xi_size));
    }
    auto b = bits.extend_lsb();
    // Swap elements
    for (auto i = 0; i < cols_num; ++i) {
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
 * @param bits The vector that contains the 'M' bits to use for swapping (if bits[i]=True, the
 * i-th rows will be swapped).
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

static_assert(std::ranges::random_access_range<orq::Vector<int>>);
static_assert(std::ranges::common_range<orq::Vector<int>>);
static_assert(std::ranges::sized_range<orq::Vector<int>>);
