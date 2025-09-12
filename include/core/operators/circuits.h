/**
 * @file circuits.h
 * @brief Implementation of various boolean circuits
 *
 * (moved out of BSharedVector.h)
 *
 */
#pragma once

#include "core/containers/b_shared_vector.h"

namespace orq::operators {
/**
 * @brief Alias for a unique pointer to a BSharedVector.
 *
 * @tparam S
 * @tparam E
 */
template <typename S, typename E>
using unique_B = std::unique_ptr<BSharedVector<S, E>>;

/**
 * Computes vectorized boolean addition using a ripple carry adder. We use a slightly
 * non-traditional circuit to achieve exactly \f$\ell - 1\f$ rounds of computation (where \f$\ell\f$
 * is the bitwidth of the shares), and bit packing, which achieves asymptotically optimal bandwidth
 * as the input vectors grow. Thus, in many cases, RCA is actually superior to PPA, since we are
 * usually not round-constrained.
 *
 * @param a
 * @param b
 * @param carry_in the carry-in bit. used for subtraction. default false.
 * @return A unique pointer to a new shared vector that contains boolean shares of the elementwise
 * additions
 */
template <typename Share, typename EVector>
static unique_B<Share, EVector> ripple_carry_adder(const BSharedVector<Share, EVector> &a,
                                                   const BSharedVector<Share, EVector> &b,
                                                   const bool carry_in) {
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
 * This runs a slightly optimized version of RCA, since we only need to compute
 * carries (no sum bits). In most cases it is a bit faster, but is not round
 * optimal.
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
 * @tparam Share Share data type
 * @tparam EVector Share container type
 * @param x A pair containing the non-shifted generate and propagate vectors of x
 * @param y A pair containing the shifted generate and propagate vectors of y
 * @return A new pair containing the output generate and propagate vectors, respectively.
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
 * Computes vectorized boolean addition using a parallel prefix adder (specifically, a Kogge-Stone
 * circuit).
 *
 * A prior version used the Sklansky circuit, which has half the bandwidth, but the bandwidth
 * savings don't matter here: we always compute over the full-width types. Additionally, the greater
 * fan-out of Sklansky had higher local compute overheads due to data movement.
 *
 * @param current the first BSharedVector
 * @param other the second BSharedVector
 * @param carry_in the carry bit (default false)
 * @return A unique pointer to a new shared vector that contains boolean
 * shares of the elementwise additions
 */
template <typename S, typename E>
unique_B<S, E> parallel_prefix_adder(const BSharedVector<S, E> &current,
                                     const BSharedVector<S, E> &other, const bool carry_in) {
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

    for (int i = 0; i < LOG_MAX_BITS_NUMBER; ++i) {
        g_shift.bit_left_shift(g, 1 << i);
        p_shift.bit_left_shift(p, 1 << i);

        std::tie(g, p) = prefix_sum<S, E>({g, p}, {g_shift, p_shift});
    }
    // reuse temporary: p = g << 1
    p.bit_left_shift(g, 1);
    return propagate ^ p;
}
}  // namespace orq::operators

// Out-of-line definitions for BSharedVector member circuits.

/**
 * @brief Internal greater-than and equals comparison. Equality is required for greater than,
 * and sorting requires both results, so fuse them here. Primarily calls down to the `bit_same`
 * subroutine, with some additional handling for the sign of the inputs. The two inputs are used
 * as temporary storage. Results returned by reference to shared vectors passed as arguments.
 *
 * @param other
 * @param eq_bits output containing the equality result
 * @param gt_bits output containing the greater-than result.
 */
template <typename T, typename E>
void orq::BSharedVector<T, E>::_compare(const orq::BSharedVector<T, E> &other,
                                        orq::BSharedVector<T, E> &eq_bits,
                                        orq::BSharedVector<T, E> &gt_bits) const {
    const size_t size = this->size();
    assert(size == other.size());

    // Number of bits in the share representation
    const int MAX_BITS_NUMBER = std::numeric_limits<std::make_unsigned_t<T>>::digits;

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
    if (std::is_signed<T>::value) {
        BSharedVector<T, E> s1(compressed_size), s2(compressed_size), r(compressed_size);

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

// Helper struct for division.
template <typename T>
struct DoubleWidth;

// We haven't yet added support for 16-bit protocols, so just bump 8 bits up
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
 * @brief Binary division using non-restoring algorithm.
 *
 * See: https://en.wikipedia.org/wiki/Division_algorithm for more.
 *
 * For now, only works for <= 64 bit ints, since we need twice as many
 * bits to implement the algorithm.
 *
 * Complexity is \f$\ell\times\f$ the complexity of boolean addition.
 *
 * @param other
 * @return std::unique_ptr<BSharedVector>
 */
template <typename T, typename E>
inline std::unique_ptr<orq::BSharedVector<T, E>> orq::BSharedVector<T, E>::operator/(
    const orq::BSharedVector<T, E> &other) const {
    /* The data type of intermediate values. Eventually, this could be
     * dynamically generated (i.e., for int8_t BSharedVector, use
     * int16_t intermediates).
     */
    using T2 = DoubleWidth<T>::type;

    /* The EVector type (this should mirror the current EVector type,
     * just with a larger base datatype T.)
     */
    using E2 = orq::EVector<T2, E::replicationNumber>;

    auto size = this->size();

#ifdef INSTRUMENT_TABLES
    single_cout("[PRIV_DIV] n=" << size);
#endif

    BSharedVector<T2, E2> r(size);
    r = *this;

    // Must do this in 3 steps to force larger types
    BSharedVector<T2, E2> d(size);
    d = other;
    d <<= BSharedVector<T, E>::MAX_BITS_NUMBER;

    BSharedVector<T2, E2> q(size);

    BSharedVector<T2, E2> c(size);

    BSharedVector<T2, E2> neg_d = -d;

    for (int i = BSharedVector<T, E>::MAX_BITS_NUMBER - 1; i >= 0; i--) {
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
    auto res = std::make_unique<BSharedVector<T, E>>(size);
    *res = q;

    return res;
}
