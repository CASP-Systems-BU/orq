#ifndef SECRECY_B_SHARED_VECTOR_H
#define SECRECY_B_SHARED_VECTOR_H

#include <bit>
#include <cmath>
#include <limits>

#include "../../debug/debug.h"
#include "../operators/common.h"
#include "shared_vector.h"

#ifdef USE_PARALLEL_PREFIX_ADDER
#define ADDER parallel_prefix_adder
#else
#define ADDER ripple_carry_adder
#endif

namespace secrecy {
// Forward class declarations
template <typename EVector>
class ElementwisePermutation;

namespace operators {
    // Friend function
    template <typename T, typename V>
    void compare(const BSharedVector<T, V> &x_vec, const BSharedVector<T, V> &y_vec,
                 BSharedVector<T, V> &eq, BSharedVector<T, V> &gt);

    template <typename T, typename V>
    void swap(BSharedVector<T, V> &x_vec, BSharedVector<T, V> &y_vec, BSharedVector<T, V> &bits);
}  // namespace operators

template <typename S, typename E>
using unique_B = std::unique_ptr<BSharedVector<S, E>>;

/**
 * Computes vectorized boolean addition using a ripple carry adder.
 * @param current  the first BSharedVector
 * @param other    the second BSharedVector
 * @param carry_in the carry-in bit. used for subtraction
 * @return A unique pointer to a new shared vector that contains boolean
 * shares of the elementwise additions
 *
 * NOTE: can add bit compression and be modified to work with negatives
 * and/or overflows
 */
template <typename Share, typename EVector>
static unique_B<Share, EVector> ripple_carry_adder(const BSharedVector<Share, EVector> &a,
                                                   const BSharedVector<Share, EVector> &b,
                                                   const bool carry_in = false) {
    static const int MAX_BITS_NUMBER = std::numeric_limits<std::make_unsigned_t<Share>>::digits;
    size_t compressed_size = a.size() / MAX_BITS_NUMBER + ((a.size() % MAX_BITS_NUMBER) > 0);

    // Compressed vectors
    BSharedVector<Share, EVector> a_xor_b_i(compressed_size), a_i(compressed_size),
        carry_i(compressed_size);
    auto sum = std::make_unique<BSharedVector<Share, EVector>>(a.size());

    auto a_xor_b = a ^ b;

    if (carry_in) {
        carry_i.inplace_invert();
    }

    for (int i = 0; i < MAX_BITS_NUMBER; ++i) {
        a_xor_b_i.pack_from(a_xor_b, i);
        a_i.pack_from(a, i);

        sum->unpack_from(a_xor_b_i ^ carry_i, i);

        // don't compute carry on the last round
        if (i == MAX_BITS_NUMBER - 1) {
            break;
        }

        // This is not the canonical full-adder: you can do it with one AND
        // + 1 OR inside the loop, along with an extra AND up front (a & b).
        // However, while XORs are expensive in hardware, they are cheap in
        // MPC, so we should do it this way, instead (only 1 AND)

        // Effectively computing the recurrence
        //   c_i = ((c_{i-1} ^ a_i) & axb_i) ^ a_i
        carry_i ^= a_i;
        carry_i &= a_xor_b_i;
        carry_i ^= a_i;
    }

    return sum;
}

/**
 * @brief Implements a comparison circuit using ripple_carry_adder; i.e.,
 * via boolean subtraction. `a < b` implies `a - b < 0`, and we can check
 * `ltz()` for free with boolean shares. So, subtract the two inputs and
 * return (a secret sharing of) the sign bit.
 *
 * This runs a slightly optimized version of RCA, since only need to compute
 * carries (no sum bits).
 *
 * @param a
 * @param b
 * @return unique pointer to a BSharedVector
 */
template <typename S, typename E>
static unique_B<S, E> rca_compare(const BSharedVector<S, E> &a, const BSharedVector<S, E> &b) {
    // compute a + (~b) + 1
    auto nb = ~b;

    static const int MAX_BITS_NUMBER = std::numeric_limits<std::make_unsigned_t<S>>::digits;
    size_t compressed_size = a.size() / MAX_BITS_NUMBER + ((a.size() % MAX_BITS_NUMBER) > 0);

    // Compressed vectors
    BSharedVector<S, E> a_xor_b_i(compressed_size), a_i(compressed_size), carry_i(compressed_size);
    auto sum = std::make_unique<BSharedVector<S, E>>(a.size());

    auto a_xor_b = a ^ nb;

    // "+ 1" for subtraction
    carry_i.inplace_invert();

    int i = 0;
    for (; i < MAX_BITS_NUMBER; ++i) {
        a_xor_b_i.pack_from(a_xor_b, i);

        // don't care about intermediate sum bits - no unpack!

        if (i == MAX_BITS_NUMBER - 1) {
            break;
        }

        a_i.pack_from(a, i);

        // Only compute carry bits
        carry_i ^= a_i;
        carry_i &= a_xor_b_i;
        carry_i ^= a_i;
    }

    carry_i ^= a_xor_b_i;

    // i == MBN - 1
    // now, actually compute "sum" - just the carry bit
    // unfortunate need to unpack into full width datatype; TODO: 1-bit
    // types
    sum->unpack_from(carry_i, i);
    return sum->ltz();
}

/**
 * Computes a prefix graph component for the parallel prefix adder.
 *
 * @tparam Share - Share data type
 * @tparam EVector - Share container type
 * @param x - A pair containing the non-shifted generate and propagate
 * vectors of x
 * @param y - A pair containing the shifted generate and propagate vectors
 * of y
 * @return A new pair containing the output generate and propagate vectors
 *
 * NOTE: the first element in a pair is the generate vector and the second
 * element is the propagate vector
 */
template <typename S, typename E>
std::pair<BSharedVector<S, E>, BSharedVector<S, E>> prefix_sum(
    const std::pair<BSharedVector<S, E>, BSharedVector<S, E>> &x,
    const std::pair<BSharedVector<S, E>, BSharedVector<S, E>> &y) {
    auto [g_x, p_x] = x;
    auto [g_y, p_y] = y;

    p_y &= p_x;  // propagate
    g_y &= p_x;

    // g_x & (g_y & p_x)
    BSharedVector<S, E> gpg = g_x & g_y;

    // This expression is equivalent to
    // g' := gx ^ gx.px ^ gx.gy.px
    // p' := py.px
    return {g_x ^ g_y ^ gpg, p_y};
}

/**
 * Computes vectorized boolean addition using a parallel prefix adder.
 * @param current - the first BSharedVector
 * @param other   - the second BSharedVector
 * @return A unique pointer to a new shared vector that contains boolean
 * shares of the elementwise additions
 */
template <typename S, typename E>
unique_B<S, E> parallel_prefix_adder(const BSharedVector<S, E> &current,
                                     const BSharedVector<S, E> &other,
                                     const bool carry_in = false) {
    static const uint MAX_BITS_NUMBER = std::numeric_limits<std::make_unsigned_t<S>>::digits;
    static const uint LOG_MAX_BITS_NUMBER = std::bit_width(MAX_BITS_NUMBER - 1);

    BSharedVector<S, E> p = current ^ other, g = current & other, propagate(current.size());
    
    if (carry_in) {
        // Copy the LSB of p into a new variable.
        propagate = p;
        propagate.mask(1);

        // if carry, flip LSB of p
        // ... p := (c ^ o ^ 1)
        p = p ^ 1;

        // Compute "dot product" between the 3 operands.
        // g := (c & 1) ^ (o & 1) ^ (c & o)
        // but this is just an XOR over the LSBs.
        g ^= propagate;
    }
    
    propagate = p;

    BSharedVector<S, E> g_shift(p.size()), p_shift(p.size());

    for (int i = 1; i <= LOG_MAX_BITS_NUMBER; ++i) {
        g_shift.reverse_bit_level_shift_from(g, i);
        p_shift.reverse_bit_level_shift_from(p, i);

        std::tie(g, p) = prefix_sum<S, E>({g, p}, {g_shift, p_shift});
    }
    // reuse temporary: p = g << 1
    p.bit_left_shift(g, 1);
    return propagate ^ p;
}

template <typename T>
struct DoubleWidth;

// We haven't yet added suport for 16-bit protocols, so just bump 8 bits up
// to 32.
template <>
struct DoubleWidth<int8_t> {
    using type = int32_t;
};
template <>
struct DoubleWidth<int16_t> {
    using type = int32_t;
};
template <>
struct DoubleWidth<int32_t> {
    using type = int64_t;
};
template <>
struct DoubleWidth<int64_t> {
    using type = __int128_t;
};

/**
 * A SharedVector that contains boolean shares and supports secure boolean operations.
 * @tparam Share - Share data type.
 * @tparam EVector - Share container type.
 */
template <typename Share, typename EVector>
class BSharedVector : public SharedVector<Share, EVector> {
    static const uint MAX_BITS_NUMBER = std::numeric_limits<std::make_unsigned_t<Share>>::digits;
    static const uint LOG_MAX_BITS_NUMBER = std::bit_width(MAX_BITS_NUMBER - 1);

    using unique_B = std::unique_ptr<BSharedVector>;

   public:
    using SharedVector_t = SharedVector<Share, EVector>;

    void pack_from(const BSharedVector &source, const int &position) {
        secrecy::service::runTime->modify_parallel(this->vector, &EVector::pack_from, source.vector,
                                                   position);
    }

    void unpack_from(const BSharedVector &source, const int &position) {
        secrecy::service::runTime->modify_parallel(this->vector, &EVector::unpack_from,
                                                   source.vector, position);
    }

    /**
     * Used by bit_same() to shift elements (see Vector::bit_level_shift() for more details).
     * @param level_size - The shift level size in number of bits.
     * @return A new vector with all elements of `this` vector shifted according to the
     * `level_size`.
     */
    void bit_level_shift_from(const BSharedVector &in, const int &level_size) {
        secrecy::service::runTime->execute_parallel(in.vector, this->vector,
                                                    &EVector::bit_level_shift, level_size);
    }

    void reverse_bit_level_shift_from(const BSharedVector &in, const int &level_size) {
        secrecy::service::runTime->execute_parallel(in.vector, this->vector,
                                                    &EVector::reverse_bit_level_shift, level_size);
    }

    /**
     * Creates a new shared vector that contains all elements of `this` vector right-shifted by
     * `shift_size`.
     * @param shift_size - The number of bits to right-shift each element of `this` vector.
     * @return A new shared vector that contains the right-shifted elements.
     */
    void bit_arithmetic_right_shift(const BSharedVector &in, const int &shift_size) {
        secrecy::service::runTime->execute_parallel(
            in.vector, this->vector, &EVector::bit_arithmetic_right_shift, shift_size);
    }

    /**
     * Creates a new shared vector that contains all elements of `this` vector right-shifted by
     * `shift_size`.
     * @param shift_size - The number of bits to right-shift each element of `this` vector.
     * @return A new shared vector that contains the right-shifted elements.
     */
    void bit_logical_right_shift(const BSharedVector &in, const int &shift_size) {
        secrecy::service::runTime->execute_parallel(in.vector, this->vector,
                                                    &EVector::bit_logical_right_shift, shift_size);
    }

    void bit_left_shift(const BSharedVector &in, const int &shift_size) {
        secrecy::service::runTime->execute_parallel(in.vector, this->vector,
                                                    &EVector::bit_left_shift, shift_size);
    }

    /**
     * Creates a new shared vector whose i-th element is a single bit generated by XORing all bits
     * of the i-th element of `this` vector, 0 <= i < size().
     * @return A new shared vector that contains single-bit elements generated as described above.
     */
    void bit_xor(const BSharedVector &in) {
        secrecy::service::runTime->execute_parallel(in.vector, this->vector, &EVector::bit_xor);
    }

    /**
     * Elementwise LSB extension.
     * This method creates a new vector whose i-th element has all its bits equal to the LSB of the
     * i-th element of `this` vector.
     * @return A unique pointer to a new shared vector that contains boolean shares of the elements
     * constructed as described above.
     */
    void extend_lsb(const BSharedVector &in) {
        secrecy::service::runTime->execute_parallel(in.vector, this->vector, &EVector::extend_lsb);
    }

    /**
     * Returns a new shared vector `e` whose i-th element is constructed by
     * comparing the i-th elements of the two input vectors (`this` and `y`).
     * If the i-th elements are the same up to their j-th bit (from left to
     * right), then the j-th bit of the `e` is 1, otherwise it is 0.
     *
     * @param y The vector that is compared with `this` vector.
     * @param _temp An optional vector to use as temporary storage. If
     * nothing is passed, allocate internally.
     * @return A new shared vector identifying the same-bits prefix.
     *
     * NOTE: This method requires log(l) communication rounds, where l is
     * the size of `Share` in bits.
     */
    unique_B bit_same(const BSharedVector &y,
                      std::optional<BSharedVector> _temp = std::nullopt) const {
        // Initialize vector - initial compute
        auto sameBit = (*this) ^ y;
        sameBit->inplace_invert();

        // Reuse storage if provided
        BSharedVector temp = _temp.has_value() ? (*_temp) : BSharedVector(y.size());

        for (uint level_size = 1; level_size <= LOG_MAX_BITS_NUMBER; level_size++) {
            temp.bit_level_shift_from(sameBit, level_size);
            *sameBit &= temp;
        }
        return sameBit;
    }

    /**
     * Creates a BSharedVector of size `_size` and initializes it with zeros.
     * @param _size - The size of the BSharedVector.
     */
    explicit BSharedVector(const size_t &_size)
        : SharedVector<Share, EVector>(_size, Encoding::BShared) {}

    /**
     * Creates a BSharedVector of size `_size` and initializes it with secret shares in the given
     * file.
     * @param _size - The size of the BSharedVector.
     * @param _input_file - The file that contains the secret shares.
     */
    explicit BSharedVector(const size_t &_size, const std::string &_input_file)
        : SharedVector<Share, EVector>(_size, _input_file, Encoding::BShared) {}

    /**
     * This is a shallow copy constructor from EVector.
     * @param _shares - The EVector whose contents will be pointed by the BSharedVector.
     */
    explicit BSharedVector(EVector &_shares)
        : SharedVector<Share, EVector>(_shares, Encoding::BShared) {}

    /**
     * This is a move constructor from EVector.
     * @param _shares - The EVector whose contents will be moved to the new BSharedVector.
     */
    BSharedVector(EVector &&_shares) : SharedVector<Share, EVector>(_shares, Encoding::BShared) {}

    /**
     * This is a move constructor from another BSharedVector.
     * @param other - The BSharedVector whose contents will be moved to the new BSharedVector.
     */
    BSharedVector(BSharedVector &&other)
        : SharedVector<Share, EVector>(other.vector, Encoding::BShared) {}

    /**
     * This is a copy constructor from another BSharedVector.
     * @param other - The BSharedVector whose contents will be copied to the new BSharedVector.
     */
    BSharedVector(const BSharedVector &other)
        : SharedVector<Share, EVector>(other.vector, Encoding::BShared) {}

    /**
     * Copy constructor from a SharedVector.
     * @param _shares - The SharedVector object whose contents will be copied to the new
     * BSharedVector.
     */
    explicit BSharedVector(SharedVector<Share, EVector> &_shares)
        : EncodedVector(_shares.encoding) {
        assert(_shares.encoding == Encoding::BShared);
        auto secretShares_ = reinterpret_cast<BSharedVector *>(&_shares);
        this->vector = secretShares_->vector;
    }

    /**
     * Move constructor that creates a BSharedVector from a unique pointer to a SharedVector object.
     * @param base - The pointer to the SharedVector object whose contents will be moved to the new
     * BSharedVector.
     */
    BSharedVector(std::unique_ptr<BSharedVector> &&base)
        : BSharedVector((BSharedVector *)base.get()) {}

    /**
     * Shallow copy constructor that creates a BSharedVector from a unique pointer to a SharedVector
     * object.
     * @param base - The SharedVector object whose contents will be pointed by the new
     * BSharedVector.
     */
    BSharedVector(std::unique_ptr<BSharedVector> &base)
        : BSharedVector((BSharedVector *)base.get()) {}

    /**
     * Move constructor that creates a BSharedVector from a pointer to another BSharedVector object.
     * @param _base - The BSharedVector that will be moved as a whole (contents + state) to the new
     * BSharedVector.
     *
     * NOTE: This constructor is implicitly called by the two constructors above.
     */
    explicit BSharedVector(BSharedVector *_base) : BSharedVector(std::move(*_base)) {}

    using SharedVector<Share, EVector>::operator=;

    BSharedVector& operator=(const BSharedVector&) = default;
    BSharedVector& operator=(BSharedVector&&) = default;

    // Destructor
    virtual ~BSharedVector() {}

    /**
     * This is a conversion from single-bit BSharedVector to ASharedVector.
     */
    auto b2a_bit() const {
        auto res = std::make_unique<ASharedVector<Share, EVector>>(this->size());
        service::runTime->b2a_bit(this->vector, res->vector);
        return res;
    }

    /**
     * This is a conversion from BSharedVector to ASharedVector.
     * Converts each bit individually by shifting it to the LSB position,
     * then converts to arithmetic and sums up all bits with appropriate weights.
     *
     * L^2 total communication over L rounds
     * TODO: all the calls to b2a_bit can be done in a single round
     */
    auto b2a() const {
        // vector to store the result, initialized to 0
        auto res = std::make_unique<ASharedVector<Share, EVector>>(this->size());
        
        const int bitwidth = std::numeric_limits<std::make_unsigned_t<Share>>::digits;
        // create an integer type to hold the value 1 with the same bitwidth as the shares
        auto one = std::make_unsigned_t<Share>(1);
        
        // create a temporary vector to hold the current bit
        auto current_bit = std::make_unique<BSharedVector<Share, EVector>>(this->size());

        // iterate over each bit
        for (int i = 0; i < bitwidth; i++) {
            // shift the bit to the LSB position and store in current_bit
            current_bit->bit_logical_right_shift(*this, i);
            
            // mask to keep only the LSB
            current_bit->mask(1);
            
            // convert the bit to arithmetic
            auto bit_arith = current_bit->b2a_bit();

            // shift the arithmetic value to the correct magnitude
            if (i > 0) {
                *bit_arith *= (one << i);
            }

            // add to the result
            *res += *bit_arith;
        }

        return res;
    }

    /**
     * This is a conversion from BSharedVector to ASharedVector.
     */
    ASharedVector<Share, EVector> insecure_b2a() const {
    	auto opened = this->open();
        ASharedVector<Share, EVector> a(this->size());
        if (service::runTime->getPartyID() == 0) {
	    a.vector(0) = opened;
        }
        return a;
    }

    // **************************************** //
    //            Boolean operators             //
    // **************************************** //

    /**
     * Elementwise secure bitwise XOR.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of XOR.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise XORs.
     */
    binary_op(^, BSharedVector, xor_b, this, other);
    binary_element_op(^, xor_b, BSharedVector, Share);

    /**
     * Elementwise secure bitwise AND.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of AND.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise ANDs.
     */
    binary_op(&, BSharedVector, and_b, this, other);
    binary_element_op(&, and_b, BSharedVector, Share);

    /**
     * Elementwise secure boolean completion.
     * This operator expects the input vector (`this`) to contain boolean shares.
     * @return A unique pointer to a new shared vector with all boolean shares of
     * `this` vector complemented.
     */
    unary_op(~, BSharedVector, not_b, this);

    /**
     * Elementwise secure boolean negation.
     * This operator expects the input vector (`this`) to contain boolean shares.
     * @return A unique pointer to a new shared vector with all boolean shares of
     * `this` vector negated.
     */
    unary_op(!, BSharedVector, not_b_1, this);

    /**
     * Elementwise secure less-than-zero comparison.
     * @return A unique pointer to a new shared vector that contains boolean shares of the
     * elementwise less-than-zero comparisons.
     */
    fn_no_input(ltz, BSharedVector, this);

    compound_assignment_op(&=, and_b, BSharedVector);
    // |= defined explicitly below
    compound_assignment_op(^=, xor_b, BSharedVector);

    unique_B operator<<(const int &s) const {
        auto out = std::make_unique<BSharedVector>(this->size());
        out->bit_left_shift(*this, s);
        return out;
    }

    unique_B operator>>(const int &s) const {
        auto out = std::make_unique<BSharedVector>(this->size());
        out->bit_arithmetic_right_shift(*this, s);
        return out;
    }

    void operator<<=(const int &s) { this->bit_left_shift(*this, s); }

    void operator>>=(const int &s) { this->bit_arithmetic_right_shift(*this, s); }

    svector_reference(BSharedVector, simple_subset_reference);
    svector_reference(BSharedVector, alternating_subset_reference);
    svector_reference(BSharedVector, reversed_alternating_subset_reference);
    svector_reference(BSharedVector, repeated_subset_reference);
    svector_reference(BSharedVector, cyclic_subset_reference);
    svector_reference(BSharedVector, directed_subset_reference);
    svector_reference(BSharedVector, included_reference);
    svector_reference(BSharedVector, mapping_reference);
    svector_reference(BSharedVector, slice);

    /**
     * Elementwise secure bitwise OR.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of OR.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise ORs.
     */
    std::unique_ptr<BSharedVector> operator|(const BSharedVector &other) const {
        // Logical OR is defined based on logical AND
        return ~(~(*this) & ~(other));
    }

    BSharedVector operator|=(const BSharedVector &other) {
        // Break apart the operation so we can prevent copying.
        this->inplace_invert();
        *this &= ~other;
        this->inplace_invert();
        return *this;
    }

    /**
     * Masks each element in `this` vector by doing a bitwise logical AND with `n`.
     * @param n - The mask.
     */
    void mask(const Share &n) {
        secrecy::service::runTime->modify_parallel(this->vector, &EVector::mask, n);
    }

    /**
     * Sets the bits of each element in `this` vector by doing a bitwise logical OR with `n`
     * @param n - The element that encodes the bits to set.
     */
    void set_bits(const Share &n) {
        secrecy::service::runTime->modify_parallel(this->vector, &EVector::set_bits, n);
    }

    /**
     * @brief Invert the bits of this BSharedVector. Operates inplace.
     *
     */
    void inplace_invert() { secrecy::service::runTime->not_b(this->vector, this->vector); }

    // **************************************** //
    //           Comparison operators           //
    // **************************************** //

    /**
     * Elementwise secure equality.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of equality.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise equality comparisons.
     */
    std::unique_ptr<BSharedVector> operator==(const BSharedVector &other) const {
        assert(this->size() == other.size());

        // Identify same-bits prefix
        auto same_bits = this->bit_same(other);

        // If the LSB is 1, it means that the respective elements from `this` and `other` are the
        // same
        same_bits->mask((Share)1);

        return same_bits;
    }

    /**
     * Elementwise secure inequality.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of inequality.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise inequality comparisons.
     */
    std::unique_ptr<BSharedVector> operator!=(const BSharedVector &other) const {
        return !((*this) == other);
    }

    /**
     * @brief Internal greater-than and equals comparison. Equals is required
     * for greater than, and sorting requires both results, so fuse them here.
     *
     * Results returned by reference to shared vectors passed as arguments.
     *
     * @param other
     * @param eq_bits
     * @param gt_bits
     */
    void _compare(const BSharedVector &other, BSharedVector &eq_bits,
                  BSharedVector &gt_bits) const {
        const size_t size = this->size();
        assert(size == other.size());

        // Number of bits in the share representation
        const int MAX_BITS_NUMBER = std::numeric_limits<std::make_unsigned_t<Share>>::digits;

        auto compressed_size = size / MAX_BITS_NUMBER + (size % MAX_BITS_NUMBER > 0);

        // Compute same-bits prefix. Use eq_bits as temp storage, then copy
        // result in.
        eq_bits = this->bit_same(other, eq_bits);

        // If MSBs are different, `this` is greater than `other` iff the
        // MSB of `this` is set, else if MSBs are the same and the second
        // MSBs are not, then `this` is greater than `other` iff the second
        // MSB of `this` is set, else...
        //
        // The plaintext value of xEdgeBit will either be all zeros, or have
        // a one somewhere, but the secret-shared values will be random.
        // This is basically a distributed point function.
        // XOR bits to get the single-bit result
        gt_bits.bit_arithmetic_right_shift(eq_bits, 1);
        gt_bits ^= eq_bits;
        gt_bits &= *this;
        // inner expr is ((eq_bits >> 1) ^ eq_bits) & (*this))
        gt_bits.bit_xor(gt_bits);

        // If the shares are signed numbers, we need to treat the sign bits differently
        if (std::is_signed<Share>::value) {
            BSharedVector s1(compressed_size), s2(compressed_size), r(compressed_size);

            // Extract MSB (sign bit), compressed
            s1.pack_from(*this, MAX_BITS_NUMBER - 1);
            s2.pack_from(other, MAX_BITS_NUMBER - 1);

            // Extract LSB from above
            r.pack_from(gt_bits, 0);

            // Update greater bits: `this` is greater than `other` iff the
            // signs are different and `other` is negative, otherwise keep
            // the existing greater bits

            // Original expression:
            //   diffs = s1 ^ s2
            //   r = (diffs & s2) ^ (!diffs & r)
            // Equivalent expression with one fewer AND:
            //  r = s1 ^ ((s1 ^ s2) | (s2 ^ r));
            // Below: same thing, using compound assignment

            r ^= s2;
            s2 ^= s1;
            s2 |= r;
            s1 ^= s2;  // result is actually in s1, not r

            // Decompress result
            gt_bits.unpack_from(s1, 0);
        }

        eq_bits.mask(1);
        gt_bits.mask(1);
    }

    /**
     * Elementwise secure greater-than comparison.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of greater-than.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise greater-than comparisons.
     */
    std::unique_ptr<BSharedVector> operator>(const BSharedVector &other) const {
        // ignore here
        BSharedVector _eq(this->size());
        auto gt = std::make_unique<BSharedVector>(this->size());

        _compare(other, _eq, *gt);
        return gt;
    }

    /**
     * Elementwise secure less-than comparison.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of less-than.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise less-than comparisons.
     */
    std::unique_ptr<BSharedVector> operator<(const BSharedVector &other) const {
        return other > (*this);
    }

    /**
     * Elementwise secure greater-or-equal comparison.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of greater-or-equal.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise greater-or-equal comparisons.
     */
    std::unique_ptr<BSharedVector> operator>=(const BSharedVector &other) const {
        return !((*this) < other);
    }

    /**
     * Elementwise secure less-or-equal comparison.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of less-or-equal.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise less-or-equal comparisons.
     */
    std::unique_ptr<BSharedVector> operator<=(const BSharedVector &other) const {
        return !((*this) > other);
    }

    /**
     * Elementwise secure boolean addition.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of boolean addition.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise additions.
     */
    std::unique_ptr<BSharedVector> operator+(const BSharedVector &other) const {
        return ADDER(*this, other, false);
    }

    BSharedVector &operator+=(const BSharedVector &other) {
        *this = *ADDER(*this, other, false);
        return *this;
    }

    BSharedVector &operator+=(const std::unique_ptr<BSharedVector> other) {
        *this = *ADDER(*this, *other, false);
        return *this;
    }

    /**
     * @brief Binary subtraction. Calls `RCA(this, ~other) + 1` by setting
     * the carry-in bit of the RCA.
     *
     * @param other
     * @return std::unique_ptr<BSharedVector>
     */
    std::unique_ptr<BSharedVector> operator-(const BSharedVector &other) const {
        return ADDER(*this, *(~other), true);
    }

    BSharedVector &operator-=(const BSharedVector &other) {
        *this = *ADDER(*this, *(~other), true);
        return *this;
    }

    /**
     * @brief Decrement with unique ptr
     *
     * @param other
     * @return BSharedVector&
     */
    BSharedVector &operator-=(const std::unique_ptr<BSharedVector> other) {
        *this = *ADDER(*this, *(~(*other)), true);
        return *this;
    }

    /**
     * @brief Negation, implemented via binary operator subtraction
     *
     * @return std::unique_ptr<BSharedVector>
     */
    std::unique_ptr<BSharedVector> operator-() const {
        auto zero = BSharedVector(this->size());
        return zero - *this;
    }

    /**
     * @brief Binary division using non-restoring algorithm
     * see: https://en.wikipedia.org/wiki/Division_algorithm for more
     *
     * For now, only works for <= 32 bit ints, since we need twice as many
     * bits to implement the algorithm.
     *
     * @param other
     * @return std::unique_ptr<BSharedVector>
     */
    std::unique_ptr<BSharedVector> operator/(const BSharedVector &other) const {
        /* The data type of intermediate values. Eventually, this could be
         * dynamically generated (i.e., for int8_t BSharedVector, use
         * int16_t intermediates).
         */
        using T = DoubleWidth<Share>::type;

        /* The EVector type (this should mirror the current EVector type,
         * just with a larger base datatype T.)
         */
        using E = secrecy::EVector<T, EVector::replicationNumber>;

        auto size = this->size();

#ifdef INSTRUMENT_TABLES
        single_cout("[PRIV_DIV] n=" << size);
#endif

        BSharedVector<T, E> r(size);
        r = *this;

        // Must do this in 3 steps to force larger types
        BSharedVector<T, E> d(size);
        d = other;
        d <<= MAX_BITS_NUMBER;

        BSharedVector<T, E> q(size);

        BSharedVector<T, E> c(size);

        BSharedVector<T, E> neg_d = -d;

        for (int i = MAX_BITS_NUMBER - 1; i >= 0; i--) {
            // c := r >= 0
            c = !r.ltz();

            /* 1 bit of the division, q(i). Instead of indexing, use
             * bitshift to get bit `c` to the `i`th location.
             *
             * If r >= 0 (c == 1), q(i) := 1
             * Otherwise (c == 0), q(i) := 0
             */
            q ^= c << i;

#ifdef DEBUG_DIVISION
            single_cout_nonl(VAR(i) << "r ");
            print(r.open());
            single_cout_nonl("q ");
            print(q.open());
            single_cout_nonl("c ");
            print(c.open());
#endif

            /* Update r. We multiply by 2 (`r << 1`) and then either add or
             * subtract d, based on the sign of r.
             *
             * If c == 1, subtract d.
             * If c == 0, add d.
             *
             * Implement the above with multiplex.
             */
            r = (r << 1) + operators::multiplex(c, d, neg_d);
        }

        /* Perform final correction. At this point, we don't actually have
         * a real binary string: `0` bits represent -1. The expression
         * `q -= ~q` corrects this.
         *
         * Then, adjust the parity of q: at this stage, q is always odd. If
         * r is negative (`r.ltz() == 1`), we subtract 1.
         *
         */
        q -= (~q) + r.ltz();

        // Reassign to the base type
        auto res = std::make_unique<BSharedVector>(size);
        *res = q;

        return res;
    }

    // Friend function
    template <typename T, typename V>
    friend void secrecy::operators::compare(const BSharedVector<T, V> &x_vec,
                                            const BSharedVector<T, V> &y_vec,
                                            BSharedVector<T, V> &eq, BSharedVector<T, V> &gt);

    template <typename T, typename V>
    friend void secrecy::operators::swap(BSharedVector<T, V> &x_vec, BSharedVector<T, V> &y_vec,
                                         BSharedVector<T, V> &bits);

    template <typename T, typename V>
    friend std::unique_ptr<BSharedVector<T, V>> ripple_carry_adder(
        const BSharedVector<T, V> &current, const BSharedVector<T, V> &other, const bool carry_in);

    template <typename T, typename V>
    friend std::unique_ptr<BSharedVector<T, V>> parallel_prefix_adder(
        const BSharedVector<T, V> &current, const BSharedVector<T, V> &other, const bool carry_in);

    template <typename T, typename V>
    friend std::pair<BSharedVector<T, V>, BSharedVector<T, V>> prefix_sum(
        const std::pair<BSharedVector<T, V>, BSharedVector<T, V>> &x,
        const std::pair<BSharedVector<T, V>, BSharedVector<T, V>> &y);

    // friend class
    template <typename T, typename V>
    friend class ASharedVector;
};

}  // namespace secrecy

#endif  // SECRECY_B_SHARED_VECTOR_H
