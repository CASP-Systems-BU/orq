#ifndef SECRECY_VECTOR_H
#define SECRECY_VECTOR_H

#include <iostream>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <vector>

#include "../../debug/debug.h"
#include "mapped_iterator.h"

#define define_binary_vector_op(_op_) \
    inline Vector operator _op_(const Vector &y) const { return *this; }

#define define_unary_vector_op(_op_) \
    inline Vector operator _op_() const { return *this; }

#define define_binary_vector_element_op(_op_)                   \
    template <typename OtherType>                               \
    inline Vector operator _op_(const OtherType &other) const { \
        return *this;                                           \
    }

#define define_binary_vector_assignment_op(_op_)          \
    template <typename OtherType>                         \
    inline Vector operator _op_(const OtherType &other) { \
        return *this;                                     \
    }

namespace secrecy {

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
 * Secrecy's wrapper of std::vector<T> that provides vectorized plaintext operations.
 * @tparam T - The type of elements in the Vector (e.g., int, long, long long, etc.)
 */
template <typename T>
class Vector {
    using Unsigned_type = typename std::make_unsigned<T>::type;
    const int MAX_BITS_NUMBER = std::numeric_limits<Unsigned_type>::digits;
    VectorSizeType length = 0;

    T element;

   public:
    // allows the use of secrecy::Vector<T>::value_type
    using value_type = T;

    /**
     * Empty constructor
     */
    Vector(std::shared_ptr<std::vector<T>> _data,
           std::shared_ptr<std::vector<VectorSizeType>> _mapping = nullptr)
        : length(_data.size()) {}

    /**
     * Move constructor
     * @param other
     */
    Vector(std::vector<T> &&_other) : length(_other.size()) {}

    /**
     * Copy constructor from vector
     * @param other
     */
    Vector(std::vector<T> &_other) : length(_other.size()) {}

    /**
     * Creates a Vector of `size` values initialize to `init_val` (0 by default).
     * @param size - The size of the new Vector.
     */
    Vector(VectorSizeType _size, T _init_val = 0) : length(_size) {}

    /**
     * Constructs a new Vector from a list of `T` elements.
     * @param elements - The list of elements of the new Vector.
     */
    Vector(std::initializer_list<T> &&elements) : length(elements.size()) {}

    /**
     * Copy constructor from range
     * @param first - The input_range whose elements will be copied to the new Vector.
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

    /**
     * @brief Arbitrary-operation prefix "sum". Operation should be
     * associative.
     *
     */
    inline void prefix_sum(const T &(*op)(const T &, const T &)) {}

    Vector chunkedSum(const VectorSizeType aggSize = 0) const { return *this; }

    // TODO: check for a (different by one index) bug for `end`.
    /**
     * Returns a new vector containing elements in the range [start, end] that are `step` positions
     * apart.
     * @param start - The index of the first element to be included in the output vector.
     * @param step - The distance between two consecutive elements.
     * @param end - The maximum possible index of the last element to be included in the output
     * vector.
     * @return A new vector that contains the selected elements.
     *
     * NOTE: This method works relatively to the current batch.
     */
    Vector simple_subset(const VectorSizeType &start, const VectorSizeType &step,
                         const VectorSizeType &end) const {
        VectorSizeType res_size = end - start + 1;
        return Vector(res_size);
    }

    void reset_batch() {}

    // TODO: check the last index usage for consistency.
    /**
     * Sets start and end index of the current batch.
     * If the start index is negative, the start index is set to zero. If the end index is greater
     * than the Vector's size, the end index is set the max possible index.
     * @param _start_ind - The index of the first element in the current batch.
     * @param _end_ind - The index of the last element in the current batch.
     */
    void set_batch(const VectorSizeType &_start_ind, const VectorSizeType &_end_ind) {}

    inline VectorSizeType total_size() const { return length; }

    using IteratorType = MappedIterator<T, typename std::vector<T>::iterator,
                                        typename std::vector<VectorSizeType>::iterator>;

    // TODO these need to work, somehow
    IteratorType begin() const { return {}; }

    IteratorType end() const { return {}; }

    std::vector<T> as_std_vector() const { return std::vector<T>(length); }

    std::vector<T> _get_internal_data() const { return std::vector<T>(length); }

    std::span<T> span() { return {}; }

    std::span<T> batch_span() { return {}; }

    std::span<const T> batch_span() const { return {}; }

    bool has_mapping() const { return false; }

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

    Vector included_reference(const Vector flag) const {
        // We won't need the entire thing, so resize after: but don't know
        // a priori how large the output is.
        VectorSizeType upper_bound_size = std::min(this->total_size(), flag.total_size());
        return Vector<T>(upper_bound_size);
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

    /**
     * Applies a new indexing mapping such that after accessing the last element, we access the
     * first element again and keep accessing the elements in cycles.
     * @param _subset_cycles the number of cycles the new indexing mapping will contain.
     * @return `Vector` that points to the same memory location as the original vector but with
     * different index mapping.
     */
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
    void alternating_bit_decompress(const Vector &other, const VectorSizeType &start,
                                    const VectorSizeType &step, const VectorSizeType &included_size,
                                    const VectorSizeType &excluded_size,
                                    const int &direction) const {}

    /**
     * @brief Materialize a vector which might have an access pattern
     * applied. If there is no mapping, just return the vector. Otherwise,
     * copy the mapped vector into a new vector (which will have no map).
     *
     * Useful for communication primitives, which require unmapped vectors.
     *
     * @return Vector<T>
     */
    Vector<T> materialize() const { return *this; }

    void materialize_inplace() {}

    Vector mapping_reference(std::vector<VectorSizeType> map) const { return Vector(map.size()); }

    template <typename S>
    Vector mapping_reference(std::vector<S> map) const {
        return Vector(map.size());
    }

    template <typename S>
    Vector mapping_reference(Vector<S> map) const {
        return Vector(map.size());
    }

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

    // This only differs from resize in that it takes `n` elements from the end
    // instead of the start.
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

    bool same_as(const Vector<T> &other, bool print_warn = true) const { return true; }

    bool starts_with(const Vector<T> &prefix) { return true; }

    // Friend classes
    template <typename Share, int ReplicationNumber>
    friend class EVector;
    friend class service::RunTime;

    template <typename InputType, typename ReturnType, typename ObjectType>
    friend class secrecy::service::Task_1;

    template <typename InputType, typename ReturnType, typename ObjectType>
    friend class secrecy::service::Task_2;

    template <typename InputType, typename ReturnType, typename... U>
    friend class secrecy::service::Task_ARGS_RTR_1;

    template <typename InputType, typename ReturnType, typename... U>
    friend class secrecy::service::Task_ARGS_RTR_2;

    template <typename InputType>
    friend class secrecy::service::Task_ARGS_VOID_1;

    template <typename InputType>
    friend class secrecy::service::Task_ARGS_VOID_2;
};

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

};  // namespace secrecy

static_assert(std::ranges::random_access_range<secrecy::Vector<int>>);
static_assert(std::ranges::common_range<secrecy::Vector<int>>);
static_assert(std::ranges::sized_range<secrecy::Vector<int>>);

#endif  // SECRECY_VECTOR_H
