#pragma once

#include <iostream>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <vector>

#include "debug/orq_debug.h"
#include "mapped_iterator.h"

/**
 * @brief Defines a dummy binary operator
 *
 */
#define define_binary_vector_op(_op_) \
    inline Vector operator _op_(const Vector &y) const { return *this; }

/**
 * @brief Defines a dummy unary operator
 *
 */
#define define_unary_vector_op(_op_) \
    inline Vector operator _op_() const { return *this; }

/**
 * @brief Defines a dummy binary operator between a Vector and some other type.
 *
 */
#define define_binary_vector_element_op(_op_)                   \
    template <typename OtherType>                               \
    inline Vector operator _op_(const OtherType &other) const { \
        return *this;                                           \
    }

/**
 * @brief Defines a dummy binary assignment operator.
 *
 */
#define define_binary_vector_assignment_op(_op_)          \
    template <typename OtherType>                         \
    inline Vector operator _op_(const OtherType &other) { \
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
 * A mock vector implementation which stores no data and returns nothing (or garbage) for all
 * operations. Most functions do nothing at all, or just compute the appropriate length that their
 * correct equivalent would output. This is useful for testing and profiling, since it is
 * extremely fast and performs no data movement.
 *
 * @tparam T The nominal element type
 */
template <typename T>
class Vector {
    using Unsigned_type = typename std::make_unsigned<T>::type;
    const int MAX_BITS_NUMBER = std::numeric_limits<Unsigned_type>::digits;
    VectorSizeType length = 0;

    /**
     * @brief A single element of the correct type so that we can properly handle the overload of
     * operator[] which returns a T&. Note that this means dummy Vectors do actually store a single
     * element, which may persist across calls (but is not guaranteed to do so). Code like
     *
     * ```c++
     * a[0] = 5
     * ```
     *
     * will store `5` in the first index of `a`. Later on, accessing a different element of `a` may
     * or may not return `5`.
     *
     */
    T element;

    /**
     * @brief Fixed-point precision (number of fractional bits)
     *
     */
    size_t precision = 0;

   public:
    // allows the use of orq::Vector<T>::value_type
    using value_type = T;

    /* Precision helpers */
    inline void setPrecision(const int fixed_point_precision) { precision = fixed_point_precision; }
    inline size_t getPrecision() const { return precision; }

    /**
     * Empty constructor
     */
    Vector(std::shared_ptr<std::vector<T>> _data,
           std::shared_ptr<std::vector<VectorSizeType>> _mapping = nullptr)
        : length(_data.size()) {}

    /**
     * Move constructor
     * @param _other
     */
    Vector(std::vector<T> &&_other) : length(_other.size()) {}

    /**
     * Copy constructor from vector
     * @param _other
     */
    Vector(std::vector<T> &_other) : length(_other.size()) {}

    /**
     * Creates a Vector of `size` values initialize to `init_val` (0 by default).
     * @param _size The size of the new Vector.
     * @param _init_val
     */
    Vector(VectorSizeType _size, T _init_val = 0) : length(_size) {}

    /**
     * Constructs a new Vector from a list of `T` elements.
     * @param elements The list of elements of the new Vector.
     */
    Vector(std::initializer_list<T> &&elements) : length(elements.size()) {}

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
     * Copy constructor from range
     * @param _other The input_range whose elements will be copied to the new Vector.
     */
    template <std::ranges::input_range IR>
    Vector(IR _other) : length(_other.size()) {}

    Vector(const Vector &other) : length(other.length) {}

    Vector(Vector &other) : length(other.length) {}

    inline Vector bit_arithmetic_right_shift(const int &shift_size) const { return *this; }

    inline Vector bit_logical_right_shift(const int &shift_size) const { return *this; }

    inline Vector bit_left_shift(const int &shift_size) const { return *this; }

    inline Vector bit_xor() const { return *this; }

    inline void prefix_sum() {}

    inline void prefix_sum(const T &(*op)(const T &, const T &)) {}

    Vector chunkedSum(const VectorSizeType aggSize = 0) const { return *this; }

    Vector simple_subset(const VectorSizeType &start, const VectorSizeType &step,
                         const VectorSizeType &end) const {
        VectorSizeType res_size = end - start + 1;
        return Vector(res_size);
    }

    void reset_batch() {}

    void set_batch(const VectorSizeType &_start_ind, const VectorSizeType &_end_ind) {}

    inline VectorSizeType total_size() const { return length; }

    using IteratorType = MappedIterator<T, typename std::vector<T>::iterator,
                                        typename std::vector<VectorSizeType>::iterator>;

    // TODO these need to work, somehow
    IteratorType begin() const { return {}; }

    IteratorType end() const { return {}; }

    /**
     * @brief Return an empty C++ vector of the given length
     *
     * @return std::vector<T>
     */
    std::vector<T> as_std_vector() const { return std::vector<T>(length); }

    /**
     * @brief Return an empty C++ vector of the given length
     *
     * @return std::vector<T>
     */
    std::vector<T> _get_internal_data() const { return std::vector<T>(length); }

    /**
     * @brief Return an empty span.
     *
     * @return std::span<T>
     */
    std::span<T> span() { return {}; }

    std::span<T> batch_span() { return {}; }

    std::span<const T> batch_span() const { return {}; }

    bool has_mapping() const { return false; }

    /**
     * @brief Return a dummy vector of the correct size
     *
     * @param _start_index
     * @param _step
     * @param _end_index
     * @return Vector
     */
    Vector simple_subset_reference(const VectorSizeType _start_index, const VectorSizeType _step,
                                   const VectorSizeType _end_index) const {
        VectorSizeType size = std::min(this->total_size(), (_end_index - _start_index) / _step + 1);
        return Vector<T>(size);
    }

    Vector simple_subset_reference(const VectorSizeType _start_index,
                                   const VectorSizeType _step) const {
        return simple_subset_reference(_start_index, _step,
                                       std::max<VectorSizeType>(0, this->total_size() - 1));
    }

    Vector simple_subset_reference(const VectorSizeType _start_index) const {
        return simple_subset_reference(_start_index, 1,
                                       std::max<VectorSizeType>(0, this->total_size() - 1));
    }

    Vector slice(const size_t start, const size_t end) const {
        size_t n = std::min(end - start, size());
        return Vector<T>(n);
    }

    Vector slice(const size_t start) const { return slice(start, size()); }

    /**
     * @brief Return a bounded dummy vector. Since included_reference is data-dependent, we can't
     * provide a cardinality-accurate dummy version.
     *
     * @param flag
     * @return Vector
     */
    Vector included_reference(const Vector flag) const {
        VectorSizeType upper_bound_size = std::min(this->total_size(), flag.total_size());
        return Vector<T>(upper_bound_size);
    }

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
        return Vector(size);
    }

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

        return Vector(size);
    }

    Vector repeated_subset_reference(const VectorSizeType _subset_repetition) const {
        VectorSizeType size = this->total_size() * _subset_repetition;
        return Vector(size);
    }

    Vector cyclic_subset_reference(const VectorSizeType _subset_cycles) const {
        VectorSizeType size = this->total_size() * _subset_cycles;
        return Vector(size);
    }

    Vector directed_subset_reference(const int _subset_direction) const { return *this; }

    Vector simple_bit_compress(const int &start, const int &step, const int &end,
                               const int &repetition) const {
        const int _step = step;
        const int _repetition = repetition;

        const int bits_per_element = std::abs(((end - start + 1) / step) * repetition);
        const VectorSizeType total_bits = bits_per_element * this->size();
        const VectorSizeType total_new_elements = div_ceil(total_bits, MAX_BITS_NUMBER);

        return Vector(total_new_elements);
    }

    void pack_from(const Vector &source, const int &position) {}

    void simple_bit_decompress(const Vector &other, const int &start, const int &step,
                               const int &end, const int &repetition) {}

    void unpack_from(const Vector &source, const T &position) {}

    Vector alternating_bit_compress(const VectorSizeType &start, const VectorSizeType &step,
                                    const VectorSizeType &included_size,
                                    const VectorSizeType &excluded_size,
                                    const int &direction) const {
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

        return Vector(total_new_elements);
    }

    inline Vector alternating_bit_compress(const VectorSizeType &start, const VectorSizeType &step,
                                           const VectorSizeType &included_size,
                                           const VectorSizeType &excluded_size) const {
        return alternating_bit_compress(start, step, included_size, excluded_size, 1);
    }

    void alternating_bit_decompress(const Vector &other, const VectorSizeType &start,
                                    const VectorSizeType &step, const VectorSizeType &included_size,
                                    const VectorSizeType &excluded_size,
                                    const int &direction) const {}

    /**
     * @brief Dummy vectors have no mapping, so just return this vector.
     *
     * @return Vector<T>
     */
    Vector<T> materialize() const { return *this; }

    void materialize_inplace() {}

    /**
     * @brief Respect the size of the mapping reference.
     *
     * @param map
     * @return Vector
     */
    Vector mapping_reference(std::vector<VectorSizeType> map) const { return Vector(map.size()); }

    template <typename S>
    Vector mapping_reference(std::vector<S> map) const {
        return Vector(map.size());
    }

    template <typename S>
    Vector mapping_reference(Vector<S> map) const {
        return Vector(map.size());
    }

    /**
     * @brief To apply a mapping for a dummy vector, we only update the length.
     *
     * @tparam S
     * @param new_mapping
     */
    template <typename S = VectorSizeType>
    void apply_mapping(std::vector<S> new_mapping) {
        length = new_mapping.size();
    }

    void reverse() {}

    Vector &operator=(const Vector &&other) { return *this; }

    Vector &operator=(const Vector &other) { return *this; }

    template <typename OtherT>
    Vector &operator=(const Vector<OtherT> &other) {
        return *this;
    }

    Vector simple_subset(const VectorSizeType &start, const VectorSizeType &size) const {
        return Vector(size);
    }

    void mask(const T &n) {}

    void set_bits(const T &n) {}

    void zero() {}

    inline Vector bit_level_shift(const int &log_level_size) const { return *this; }

    inline Vector reverse_bit_level_shift(const int &log_level_size) const { return *this; }

    inline VectorSizeType size() const { return length; }

    void resize(size_t n) { length = n; }

    void tail(size_t n) { length = n; }

    // **************************************** //
    //           Arithmetic operators           //
    // **************************************** //

    define_binary_vector_op(+);
    define_binary_vector_op(-);
    define_binary_vector_op(*);
    define_binary_vector_op(/);
    define_unary_vector_op(-);

    // **************************************** //
    //             Boolean operators            //
    // **************************************** //

    define_binary_vector_op(&);
    define_binary_vector_op(|);
    define_binary_vector_op(^);
    define_unary_vector_op(~);
    define_unary_vector_op(!);

    // **************************************** //
    //           Comparison operators           //
    // **************************************** //

    define_binary_vector_op(==);
    define_binary_vector_op(!=);
    define_binary_vector_op(>);
    define_binary_vector_op(>=);
    define_binary_vector_op(<);
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

    inline Vector ltz() const { return *this; }

    inline Vector extend_lsb() const { return *this; }

    inline Vector extract_valid(Vector valid) { return valid; }

    std::pair<Vector, Vector> divrem(const T d) { return {*this, *this}; }

    inline T &operator[](const VectorSizeType &index) { return element; }

    inline const T &operator[](const VectorSizeType &index) const { return element; }

    /**
     * @brief All dummy vectors are equal.
     *
     * @param other
     * @param print_warn
     * @return true Always returns true.
     */
    bool same_as(const Vector<T> &other, bool print_warn = true) const { return true; }

    /**
     * @brief All dummy vectors are prefixes of each other. TODO: check size at least?
     *
     * @param prefix
     * @return true Always returns true.
     */
    bool starts_with(const Vector<T> &prefix) { return true; }

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
 * @brief Return an arbitrary row as a dummy Vector.
 *
 * @tparam Share
 * @param x_vec
 * @param y_vec
 * @param inverse
 * @return Vector<Share>
 */
template <typename Share>
static Vector<Share> compare_rows(const std::vector<Vector<Share> *> &x_vec,
                                  const std::vector<Vector<Share> *> &y_vec,
                                  const std::vector<bool> &inverse) {
    return *x_vec[0];
}

template <typename Share>
static void swap(std::vector<Vector<Share> *> &x_vec, std::vector<Vector<Share> *> &y_vec,
                 const Vector<Share> &bits) {}

template <typename Share>
static void swap(Vector<Share> &x_vec, Vector<Share> &y_vec, const Vector<Share> &bits) {}

};  // namespace orq

static_assert(std::ranges::random_access_range<orq::Vector<int>>);
static_assert(std::ranges::common_range<orq::Vector<int>>);
static_assert(std::ranges::sized_range<orq::Vector<int>>);
