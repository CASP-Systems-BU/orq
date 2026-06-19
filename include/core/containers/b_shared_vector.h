#pragma once

#include <bit>
#include <cmath>
#include <limits>

#include "core/operators/aggregation.h"
// #include "core/operators/common.h"
#include "debug/orq_debug.h"
#include "shared_vector.h"

namespace orq {
// Forward class declarations
template <typename EVector>
class ElementwisePermutation;

namespace operators {
    // Friend function
    template <typename T, typename V>
    void compare(const BSharedVector<T, V>& x_vec, const BSharedVector<T, V>& y_vec,
                 BSharedVector<T, V>& eq, BSharedVector<T, V>& gt);

    template <typename T, typename V>
    void swap(BSharedVector<T, V>& x_vec, BSharedVector<T, V>& y_vec, BSharedVector<T, V>& bits);

    template <typename T, typename E>
    static std::unique_ptr<BSharedVector<T, E>> ripple_carry_adder(const BSharedVector<T, E>&,
                                                                   const BSharedVector<T, E>&,
                                                                   bool = false);

    template <typename T, typename E>
    static std::unique_ptr<BSharedVector<T, E>> parallel_prefix_adder(const BSharedVector<T, E>&,
                                                                      const BSharedVector<T, E>&,
                                                                      bool = false);
}  // namespace operators

/**
 * @brief Alias for a unique pointer to a BSharedVector.
 *
 * @tparam S
 * @tparam E
 */
template <typename S, typename E>
using unique_B = std::unique_ptr<BSharedVector<S, E>>;

/**
 * A SharedVector that contains boolean shares and supports secure boolean operations.
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 */
template <typename Share, typename EVector>
class BSharedVector : public SharedVector<Share, EVector> {
    static const uint MAX_BITS_NUMBER = std::numeric_limits<std::make_unsigned_t<Share>>::digits;
    static const uint LOG_MAX_BITS_NUMBER = std::bit_width(MAX_BITS_NUMBER - 1);

    using unique_B = std::unique_ptr<BSharedVector>;

    double model_rca(double latency, double bandwidth, size_t n) const {
        // each logical round is split into batches
        auto rounds = (MAX_BITS_NUMBER - 1);
        rounds *= service::runTime->numBatches(n);

        return rounds * (latency + n / bandwidth);
    }

    double model_ppa(double latency, double bandwidth, size_t n) const {
        auto elm_per_sec = bandwidth / MAX_BITS_NUMBER;
        // 1 + 3 lg(n)
        auto rounds = 1 + 3 * std::bit_width(MAX_BITS_NUMBER - 1);
        rounds *= service::runTime->numBatches(n);

        // No scaling here, because no bit packing
        return rounds * (latency + n / elm_per_sec);
    }

    unique_B binary_adder(const BSharedVector<Share, EVector>& a,
                          const BSharedVector<Share, EVector>& b, bool carry_in) const {
#ifndef FORCE_PARALLEL_PREFIX_ADDER
        const double latency_sec = 1e-3 * service::runTime->comm0()->getLatency();
        const double bandwidth_bps = 1e9 * service::runTime->comm0()->getBandwidth();

        size_t n = a.size();

        auto t_rca = model_rca(latency_sec, bandwidth_bps, n);
        auto t_ppa = model_ppa(latency_sec, bandwidth_bps, n);

        // Output the model parameters
        // single_cout(t_rca << " RCA vs. " << t_ppa << " PPA");

        if (t_rca <= t_ppa) {
            return operators::ripple_carry_adder(a, b, carry_in);
        } else
#endif
        {
            return operators::parallel_prefix_adder(a, b, carry_in);
        }
    }

   public:
    using SharedVector_t = SharedVector<Share, EVector>;

    /**
     * @brief Bit packing. Operates in place by packing the bit at index `position`
     * from BSharedVector `source` into `this` BSharedVector.
     *
     * @param source
     * @param position
     */
    void pack_from(const BSharedVector& source, const int position) {
        service::runTime->modify_parallel(this->vector, &EVector::pack_from, source.vector,
                                          position);
    }

    /**
     * @brief Bit unpacking. Operates in place by unpacking the bit at index `position`
     * from BSharedVector `source` into `this`.
     *
     * @param source
     * @param position
     */
    void unpack_from(const BSharedVector& source, const int position) {
        service::runTime->modify_parallel(this->vector, &EVector::unpack_from, source.vector,
                                          position);
    }

    /**
     * Arithmetic right shift. Operates in place by putting the output into `this`. Respects the
     * sign bit.
     *
     * @param in input
     * @param shift_size The number of bits to right-shift each element of `this` vector.
     */
    void bit_arithmetic_right_shift(const BSharedVector& in, const int shift_size) {
        service::runTime->execute_parallel(in.vector, this->vector,
                                           &EVector::bit_arithmetic_right_shift, shift_size);
    }

    /**
     * Logical right shift. Operates in place. Does not respect the sign bit.
     *
     * @param in input
     * @param shift_size The number of bits to right-shift each element of `this` vector.
     */
    void bit_logical_right_shift(const BSharedVector& in, const int shift_size) {
        service::runTime->execute_parallel(in.vector, this->vector,
                                           &EVector::bit_logical_right_shift, shift_size);
    }

    /**
     * @brief Bit left shift. Operates in place. Does not respect the sign bit.
     *
     * @param in
     * @param shift_size
     */
    void bit_left_shift(const BSharedVector& in, const int shift_size) {
        service::runTime->execute_parallel(in.vector, this->vector, &EVector::bit_left_shift,
                                           shift_size);
    }

    /**
     * @brief Compute the parity of the input BSharedVector and place the result into `this`.
     *
     * @param in input shared vector
     */
    void bit_xor(const BSharedVector& in) {
        service::runTime->execute_parallel(in.vector, this->vector, &EVector::bit_xor);
    }

    /**
     * Elementwise LSB extension. This method operates in place, copying the LSB of each `in`
     * element into `this`.
     *
     * @param in
     */
    void extend_lsb(const BSharedVector& in) {
        service::runTime->execute_parallel(in.vector, this->vector, &EVector::extend_lsb);
    }

    /**
     * Returns a new shared vector `e` whose i-th element is constructed by comparing the i-th
     * elements of the two input vectors (`this` and `y`). If the i-th elements are the same up to
     * their j-th bit (from left to right), then the j-th bit of the `e` is 1, otherwise it is 0.
     *
     * In effect, this is a prefix-OR circuit. We use the Kogge-Stone toplogy.
     *
     * @param y The vector that is compared with `this` vector.
     * @param _temp An optional vector to use as temporary storage. If
     * nothing is passed, allocate internally.
     * @return A new shared vector identifying the same-bits prefix.
     *
     * NOTE: This method requires \f$\lg \ell\f$ communication rounds, where \f$\ell\f$ is
     * the size of `Share` in bits.
     */
    unique_B bit_same(const BSharedVector& y,
                      std::optional<BSharedVector> _temp = std::nullopt) const {
        // Initialize vector - initial compute
        auto sameBit = (*this) ^ y;
        sameBit->inplace_invert();

        // Reuse storage if provided. Otherwise, allocate new.
        BSharedVector temp = _temp.has_value() ? (*_temp) : BSharedVector(y.size());

        for (int level_size = 0; level_size < LOG_MAX_BITS_NUMBER; level_size++) {
            temp.bit_arithmetic_right_shift(sameBit, 1 << level_size);
            *sameBit &= temp;
        }
        return sameBit;
    }

    /**
     * @brief Return a length-one ASharedVector counting the number of nonzero elements in this
     * vector.
     *
     * @return std::unique_ptr<ASharedVector>
     */
    auto count_nonzero() const {
        // TODO: avoid this alloc
        auto zero = this->construct_like();
        auto a = (*this != zero)->b2a_bit();
        a->prefix_sum();
        a->tail(1);
        return a;
    }

    /**
     * Returns the popcount (AKA Hamming weight) of `this` as a `std::unique_ptr` to a
     * `ASharedVector`. Uses a round-optimal concatenation approach, unpacking `this` into
     * the LSB of a new `BSharedVector` and then taking its `chunkedSum`.
     *
     * @return A pointer to a new ASharedVector with the popcount of `this`
     */
    auto popcount() const {
        auto res = std::make_unique<ASharedVector<Share, EVector>>(this->size());
        size_t size = this->size(), bits = this->MAX_BITS_NUMBER;

        BSharedVector<Share, EVector> concat(size * bits);
        concat.unpack_from(*this, 0);
        auto conv = concat.b2a_bit();

        *res = conv->chunkedSum(bits);
        return res;
    }

    /**
     * Creates a BSharedVector of size `_size` and initializes it with zeros.
     * @param _size The size of the BSharedVector.
     */
    explicit BSharedVector(const size_t _size)
        : SharedVector<Share, EVector>(_size, Encoding::BShared) {}

    /**
     * Creates a BSharedVector of size `_size` and initializes it with secret shares in the given
     * file.
     * @param _size The size of the BSharedVector.
     * @param _input_file The file that contains the secret shares.
     */
    explicit BSharedVector(const size_t _size, const std::string& _input_file)
        : SharedVector<Share, EVector>(_size, _input_file, Encoding::BShared) {}

    /**
     * This is a shallow copy constructor from EVector.
     * @param _shares The EVector whose contents will be pointed by the BSharedVector.
     */
    explicit BSharedVector(EVector& _shares)
        : SharedVector<Share, EVector>(_shares, Encoding::BShared) {}

    /**
     * This is a move constructor from EVector.
     * @param _shares The EVector whose contents will be moved to the new BSharedVector.
     */
    BSharedVector(EVector&& _shares) : SharedVector<Share, EVector>(_shares, Encoding::BShared) {}

    /**
     * This is a move constructor from another BSharedVector.
     * @param other The BSharedVector whose contents will be moved to the new BSharedVector.
     */
    BSharedVector(BSharedVector&& other)
        : SharedVector<Share, EVector>(other.vector, Encoding::BShared) {}

    /**
     * This is a copy constructor from another BSharedVector.
     * @param other The BSharedVector whose contents will be copied to the new BSharedVector.
     */
    BSharedVector(const BSharedVector& other)
        : SharedVector<Share, EVector>(other.vector, Encoding::BShared) {}

    /**
     * Copy constructor from a SharedVector.
     * @param _shares The SharedVector object whose contents will be copied to the new
     * BSharedVector.
     */
    explicit BSharedVector(SharedVector<Share, EVector>& _shares)
        : EncodedVector(_shares.encoding) {
        assert(_shares.encoding == Encoding::BShared);
        auto secretShares_ = reinterpret_cast<BSharedVector*>(&_shares);
        this->vector = secretShares_->vector;
    }

    /**
     * Move constructor that creates a BSharedVector from a unique pointer to a SharedVector object.
     * @param base The pointer to the SharedVector object whose contents will be moved to the new
     * BSharedVector.
     */
    BSharedVector(std::unique_ptr<BSharedVector>&& base)
        : BSharedVector((BSharedVector*)base.get()) {}

    /**
     * Shallow copy constructor that creates a BSharedVector from a unique pointer to a SharedVector
     * object.
     * @param base The SharedVector object whose contents will be pointed by the new
     * BSharedVector.
     */
    BSharedVector(std::unique_ptr<BSharedVector>& base)
        : BSharedVector((BSharedVector*)base.get()) {}

    /**
     * Move constructor that creates a BSharedVector from a pointer to another BSharedVector object.
     * @param _base The BSharedVector that will be moved as a whole (contents + state) to the new
     * BSharedVector.
     *
     * NOTE: This constructor is implicitly called by the two constructors above.
     */
    explicit BSharedVector(BSharedVector* _base) : BSharedVector(std::move(*_base)) {}

    /**
     * @brief Construct a new BSharedVector object like another BSharedVector but with a different
     * size.
     *
     * @param size The size of the new BSharedVector.
     * @return ASharedVector The newly constructed BSharedVector.
     */
    BSharedVector construct_like(std::optional<size_t> size = {}) const {
        auto new_size = size.value_or(this->size());
        return BSharedVector(this->vector.construct_like(new_size));
    }

    /**
     * @brief Use `operator=` from the underlying `SharedVector`
     *
     */
    using SharedVector<Share, EVector>::operator=;

    BSharedVector& operator=(const BSharedVector&) = default;
    BSharedVector& operator=(BSharedVector&&) = default;

    virtual ~BSharedVector() {}

    /**
     * @brief Convert the LSB of each element of this BSharedVector to an arithmetic sharing. This
     * is substantially more efficient than a full-width conversion and suffices for most
     * applications.
     *
     * @return ASharedVector
     */
    auto b2a_bit() const {
        auto res = std::make_unique<ASharedVector<Share, EVector>>(this->size());
        service::runTime->b2a_bit(this->vector, res->vector);
        res->setPrecision(this->getPrecision());
        return res;
    }

    /** @brief Full-width conversion from BSharedVector to ASharedVector. Generates an online
     * shared-bit correlation (random value shared in both domains), and then uses a boolean adder
     * to mask.
     *
     * The first loop is technically preprocessing, which would speed things up a lot.
     *
     * @return ASharedVector
     */
    std::unique_ptr<ASharedVector<Share, EVector>> b2a() const {
        using ret_t = ASharedVector<Share, EVector>;

        constexpr int R = EVector::replicationNumber;

        if constexpr (CONFIDENTIAL_1PC) {
            // No point doing conversion!
            return std::make_unique<ret_t>(this->asEVector());
        }

        auto a_re = std::make_unique<ret_t>(this->size());
        auto b_re = this->construct_like();

        auto me = service::runTime->getPartyID();

        for (auto g : service::runTime->getGroups()) {
            // Generate random vector (or maybe zero, if I'm not in the group)
            Vector<Share> myRandom(this->size());
            if (g.contains(me)) {
                service::runTime->populateCommonRandom(myRandom, g);
            }

            *a_re += service::runTime->public_share<R>(myRandom, g);
            b_re += service::runTime->public_share<R>(myRandom, g);
        }

        // Now a_re === b_re but in different sharing schemes.

        // Add random value to mask (with binary adder), then open
        auto z = (*this + b_re)->open();

        // Subtract secret-shared mask from public z
        a_re->inplace_invert();
        *a_re += service::runTime->public_share<R>(z);
        a_re->setPrecision(this->getPrecision());
        return a_re;
    }

    /**
     * This is a conversion from BSharedVector to ASharedVector.
     * It is insecure. It is only used in the generation of dummy permutations.
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
     * Elementwise secure bitwise XOR. Returns a unique_ptr.
     */
    binary_op(^, BSharedVector, xor_b, this, other);
    binary_element_op(^, xor_b, BSharedVector, Share);
    compound_assignment_op(^=, xor_b, BSharedVector);

    /**
     * Elementwise secure bitwise AND. Returns a unique_ptr.
     */
    binary_op(&, BSharedVector, and_b, this, other);
    binary_element_op(&, and_b, BSharedVector, Share);
    compound_assignment_op(&=, and_b, BSharedVector);

    /**
     * Elementwise secure boolean complement. Returns a unique_ptr.
     */
    unary_op(~, BSharedVector, not_b, this);

    /**
     * Elementwise secure boolean negation. Does not perform an equal-to-zero check: instead, just
     * considers the LSB. Returns a unique_ptr.
     */
    unary_op(!, BSharedVector, not_b_1, this);

    /**
     * Elementwise secure less-than-zero comparison. Returns a unique_ptr.
     */
    fn_no_input(ltz, BSharedVector, this);

    /**
     * @brief Left shift. Returns a new BSharedVector.
     *
     * @param s
     * @return unique_B
     */
    unique_B operator<<(const int s) const {
        auto out = std::make_unique<BSharedVector>(this->size());
        out->bit_left_shift(*this, s);
        return out;
    }

    /**
     * @brief Arithmetic right shift. Returns a new BSharedVector.
     *
     * @param s
     * @return unique_B
     */
    unique_B operator>>(const int s) const {
        auto out = std::make_unique<BSharedVector>(this->size());
        out->bit_arithmetic_right_shift(*this, s);
        return out;
    }

    /**
     * @brief Left shift assignment operator.
     *
     * @param s
     */
    void operator<<=(const int s) { this->bit_left_shift(*this, s); }

    /**
     * @brief Arithmetic right shift assignment operator.
     *
     * @param s
     */
    void operator>>=(const int s) { this->bit_arithmetic_right_shift(*this, s); }

    /**
     * @brief Inherit access patterns from SharedVector.
     *
     */
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
     * Elementwise secure bitwise OR. This operator expects both input vectors (`this` and `other`)
     * to have the same size. Implement using DeMorgan's Law.
     *
     * @param other The second operand of OR.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise ORs.
     */
    std::unique_ptr<BSharedVector> operator|(const BSharedVector& other) const {
        // Logical OR is defined based on logical AND
        return ~(~(*this) & ~(other));
    }

    /**
     * @brief Secure bitwise OR assignment operator. Prevent excess allocations by operating
     * in-place on `this`.
     *
     * @param other
     * @return BSharedVector
     */
    BSharedVector operator|=(const BSharedVector& other) {
        // Break apart the operation so we can prevent copying.
        this->inplace_invert();
        *this &= ~other;
        this->inplace_invert();
        return *this;
    }

    /**
     * Masks each element in `this` vector by doing a bitwise logical AND with `n`.
     * @param n The mask.
     */
    void mask(const Share n) { service::runTime->modify_parallel(this->vector, &EVector::mask, n); }

    /**
     * Sets the bits of each element in `this` vector by doing a bitwise logical OR with `n`
     * @param n The element that encodes the bits to set.
     */
    void set_bits(const Share n) {
        service::runTime->modify_parallel(this->vector, &EVector::set_bits, n);
    }

    /**
     * @brief Invert the bits of this BSharedVector. Operates inplace.
     *
     */
    void inplace_invert() { service::runTime->not_b(this->vector, this->vector); }

    // **************************************** //
    //           Comparison operators           //
    // **************************************** //

    /**
     * Elementwise secure equality.
     * This operator expects both input vectors (`this` and `other`) to have the same size. Calls
     * down to the `bit_same` subroutine.
     *
     * @param other The second operand of equality.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise equality comparisons.
     */
    std::unique_ptr<BSharedVector> operator==(const BSharedVector& other) const {
        assert(this->size() == other.size());

        if constexpr (CONFIDENTIAL_1PC) {
            return std::make_unique<BSharedVector>(this->asEVector() == other.asEVector());
        }

        // Identify same-bits prefix
        auto same_bits = this->bit_same(other);

        // If the LSB is 1, it means that the respective elements from `this` and `other` are the
        // same
        same_bits->mask((Share)1);

        return same_bits;
    }

    /**
     * Elementwise secure inequality. This operator expects both input vectors (`this` and `other`)
     * to have the same size.
     *
     * @param other The second operand of inequality.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise inequality comparisons.
     */
    std::unique_ptr<BSharedVector> operator!=(const BSharedVector& other) const {
        return !((*this) == other);
    }

    /**
     * @brief Greater-than or equals comparison circuit Primarily calls down to the `bit_same`
     * subroutine, with some additional handling for the sign of the inputs. The two additional
     * inputs are used as temporary storage. Results returned by reference to shared vectors passed
     * as arguments.
     *
     * @param other
     * @param eq_bits output containing the equality result
     * @param gt_bits output containing the greater-than result.
     */
    void _compare(const BSharedVector& other, BSharedVector& eq_bits, BSharedVector& gt_bits) const;

    /**
     * Elementwise secure greater-than comparison.
     * This operator expects both input vectors (`this` and `other`) to have the same size. Call
     * down to the `_compare` subroutine.
     * @param other The second operand of greater-than.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise greater-than comparisons.
     */
    std::unique_ptr<BSharedVector> operator>(const BSharedVector& other) const {
        // ignore here
        BSharedVector _eq(this->size());
        auto gt = std::make_unique<BSharedVector>(this->size());

        _compare(other, _eq, *gt);
        return gt;
    }

    /**
     * Elementwise secure less-than comparison. Implement by calling greater-than with flipped
     * arguments.
     *
     * @param other The second operand of less-than.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise less-than comparisons.
     */
    std::unique_ptr<BSharedVector> operator<(const BSharedVector& other) const {
        return other > (*this);
    }

    /**
     * Elementwise secure greater-or-equal comparison. Implement by inverting less-than.
     *
     * @param other The second operand of greater-or-equal.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise greater-or-equal comparisons.
     */
    std::unique_ptr<BSharedVector> operator>=(const BSharedVector& other) const {
        return !((*this) < other);
    }

    /**
     * @brief Elementwise secure less-or-equal comparison. Implement by invert greater-than. This
     * operator expects both input vectors (`this` and `other`) to have the same size.
     *
     * @param other The second operand of less-or-equal.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise less-or-equal comparisons.
     */
    std::unique_ptr<BSharedVector> operator<=(const BSharedVector& other) const {
        return !((*this) > other);
    }

    /**
     * Elementwise secure boolean addition. Call the compile-time-specified addition circuit.
     * @param other The second operand of boolean addition.
     * @return A unique pointer to a new shared vector that contains boolean shares of
     * the elementwise additions.
     */
    std::unique_ptr<BSharedVector> operator+(const BSharedVector& other) const {
        return binary_adder(*this, other, false);
    }

    /**
     * @brief Elementwise secure boolean compound assignment addition.
     *
     * @param other
     * @return BSharedVector&
     */
    BSharedVector& operator+=(const BSharedVector& other) {
        *this = *binary_adder(*this, other, false);
        return *this;
    }

    /**
     * @brief Unique pointer version of the above.
     *
     * @param other
     * @return BSharedVector&
     */
    BSharedVector& operator+=(const std::unique_ptr<BSharedVector> other) {
        *this = *binary_adder(*this, *other, false);
        return *this;
    }

    /**
     * @brief Binary subtraction. Calls `RCA(this, ~other) + 1` by setting
     * the carry-in bit of the RCA.
     *
     * @param other
     * @return std::unique_ptr<BSharedVector>
     */
    std::unique_ptr<BSharedVector> operator-(const BSharedVector& other) const {
        return binary_adder(*this, *(~other), true);
    }

    /**
     * @brief Binary subtraction compound assignment.
     *
     * @param other
     * @return BSharedVector&
     */
    BSharedVector& operator-=(const BSharedVector& other) {
        *this = *binary_adder(*this, *(~other), true);
        return *this;
    }

    /**
     * @brief Unique pointer version of the above.
     *
     * @param other
     * @return BSharedVector&
     */
    BSharedVector& operator-=(const std::unique_ptr<BSharedVector> other) {
        *this = *binary_adder(*this, *(~(*other)), true);
        return *this;
    }

    /**
     * @brief Negation, implemented via binary operator subtraction: \f$-x = 0-x\f$.
     *
     * @return std::unique_ptr<BSharedVector>
     */
    std::unique_ptr<BSharedVector> operator-() const {
        auto zero = BSharedVector(this->size());
        return zero - *this;
    }

    std::unique_ptr<BSharedVector> operator/(const BSharedVector& other) const;

    // friend class
    template <typename T, typename V>
    friend class ASharedVector;
};

}  // namespace orq
