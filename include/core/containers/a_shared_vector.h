#pragma once

#include "core/math/util.h"
#include "shared_vector.h"

namespace orq {

template <typename Share, typename EVector>
class BSharedVector;

/**
 * A SharedVector that contains arithmetic shares and supports secure arithmetic operations.
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 */
template <typename Share, typename EVector>
class ASharedVector : public SharedVector<Share, EVector> {
   public:
    /**
     * Creates an ASharedVector of size `_size` and initializes it with zeros.
     * @param _size The size of the ASharedVector.
     */
    explicit ASharedVector(const size_t _size)
        : SharedVector<Share, EVector>(_size, Encoding::AShared) {}

    /**
     * Creates an ASharedVector of size `_size` and initializes it with secret shares in the given
     * file.
     * @param _size The size of the ASharedVector.
     * @param _input_file The file that contains the secret shares.
     */
    explicit ASharedVector(const size_t _size, const std::string& _input_file)
        : SharedVector<Share, EVector>(_size, _input_file, Encoding::AShared) {}

    /**
     * This is a shallow copy constructor from EVector contents.
     * @param _shares The EVector whose contents will be pointed by the ASharedVector.
     */
    explicit ASharedVector(EVector& _shares)
        : SharedVector<Share, EVector>(_shares, Encoding::AShared) {}

    ASharedVector(EVector&& _shares)
        : SharedVector<Share, EVector>(std::move(_shares), Encoding::AShared) {}

    ASharedVector(ASharedVector&& other) noexcept
        : SharedVector<Share, EVector>(std::move(other.vector), other.encoding) {}

    ASharedVector(const ASharedVector& other)
        : SharedVector<Share, EVector>(other.vector, other.encoding) {}

    explicit ASharedVector(SharedVector<Share, EVector>& _shares)
        : SharedVector<Share, EVector>(_shares) {
        assert(_shares.encoding == Encoding::AShared);
    }

    ASharedVector(std::unique_ptr<ASharedVector>&& base) : ASharedVector(std::move(*base)) {}

    ASharedVector(std::unique_ptr<ASharedVector>& base) : ASharedVector(*base) {}

    /**
     * @brief Construct a new ASharedVector object like another ASharedVector but with a different
     * size.
     *
     * @param size The size of the new ASharedVector.
     * @return ASharedVector The newly constructed ASharedVector.
     */
    ASharedVector construct_like(std::optional<size_t> size = {}) const {
        auto new_size = size.value_or(this->size());
        return ASharedVector(this->vector.construct_like(new_size));
    }

    /**
     * @brief Use the underlying SharedVector's implementation of `operator=`.
     *
     */
    using SharedVector<Share, EVector>::operator=;

    ASharedVector& operator=(const ASharedVector& other) {
        SharedVector<Share, EVector>::operator=(other);
        return *this;
    }

    ASharedVector& operator=(ASharedVector&& other) {
        SharedVector<Share, EVector>::operator=(std::move(other));
        return *this;
    }

    static constexpr bool inplace_add = true;
    static constexpr bool inplace_sub = true;
    static constexpr bool inplace_mul = true;
    static constexpr bool inplace_and = false;
    static constexpr bool inplace_or = false;
    static constexpr bool inplace_xor = false;

    // Destructor
    virtual ~ASharedVector() override {}

    /**
     * @brief Reuse underlying SharedVector implementation for access patterns.
     *
     */
    svector_reference(ASharedVector, simple_subset_reference);
    svector_reference(ASharedVector, alternating_subset_reference);
    svector_reference(ASharedVector, reversed_alternating_subset_reference);
    svector_reference(ASharedVector, repeated_subset_reference);
    svector_reference(ASharedVector, cyclic_subset_reference);
    svector_reference(ASharedVector, directed_subset_reference);
    svector_reference(ASharedVector, included_reference);
    svector_reference(ASharedVector, mapping_reference);
    svector_reference(ASharedVector, slice);

    /**
     * @brief Type alias for an equivalent BSharedVector.
     *
     */
    using B = BSharedVector<Share, EVector>;

    /**
     * @brief Convert from ASharedVector to BSharedVector. Each party redistributes boolean shares
     * of their additive shares, then uses a boolean addition circuit to "add" those shares back
     * together. In the end we are left with an XOR-sharing of the same value.
     *
     * Other protocols, such as using preprocessed shared bits, are possible.
     *
     */
    std::unique_ptr<B> a2b() const {
        if constexpr (CONFIDENTIAL_1PC) {
            // No point doing conversion!
            return std::make_unique<B>(this->asEVector());
        }

        std::pair<B, B> v = service::runTime->redistribute_shares_b(this->vector);
        // This `+` here is actually a call to our binary adder circuit.
        auto res = v.first + v.second;
        res->setPrecision(this->getPrecision());
        return res;
    }

    // **************************************** //
    //           Arithmetic operators           //
    // **************************************** //

    /**
     * Elementwise secure arithmetic addition. Returns a unique ptr.
     */
    binary_op(+, ASharedVector, add_a, this, other);

    /**
     * Elementwise secure arithmetic subtraction. Returns a unique ptr.
     */
    binary_op(-, ASharedVector, sub_a, this, other);

    /**
     * Elementwise secure arithmetic multiplication. Returns a unique ptr.
     */
    binary_op(*, ASharedVector, multiply_a, this, other);

    /**
     * Elementwise secure arithmetic negation. Returns a unique ptr.
     */
    unary_op(-, ASharedVector, neg_a, this);

    binary_element_op(+, add_a, ASharedVector, Share);
    binary_element_op(-, sub_a, ASharedVector, Share);
    binary_element_op(*, multiply_a, ASharedVector, Share);

    /**
     * @brief Multiply by a public floating-point constant (local, no MPC protocol).
     * Scales the float to a fixed-point integer using this vector's precision,
     * multiplies each share locally, and truncates.
     */
    template <std::floating_point FP>
    std::unique_ptr<ASharedVector> operator*(FP y) const {
        int p = this->getPrecision();
        Share scaled =
            static_cast<Share>(std::llround(static_cast<long double>(y) * std::ldexp(1.0L, p)));
        auto result_ev = this->vector * scaled;
        if (p > 0) {
            result_ev = result_ev / (Share(1) << p);
        }
        return std::make_unique<ASharedVector>(std::move(result_ev));
    }

    compound_assignment_op(+=, add_a, ASharedVector);
    compound_assignment_op(-=, sub_a, ASharedVector);
    compound_assignment_op(*=, multiply_a, ASharedVector);

    compound_assignment_ptr_op(+=, add_a, ASharedVector);
    compound_assignment_ptr_op(-=, sub_a, ASharedVector);
    compound_assignment_ptr_op(*=, multiply_a, ASharedVector);

    compound_assignment_element_op(+=, add_a, ASharedVector, Share);
    compound_assignment_element_op(-=, sub_a, ASharedVector, Share);
    compound_assignment_element_op(*=, multiply_a, ASharedVector, Share);

    /**
     * @brief Division by public constant. Call the underlying protocol's `div_const_a`
     * functionality, then perform error correction (if configured to do so using the compiler
     * directive USE_DIVISION_CORRECTION).
     *
     * @param c
     * @return std::unique_ptr<ASharedVector>
     */
    std::unique_ptr<ASharedVector> operator/(const Share& c) const {
        // compute division and error correction shares
        std::pair<ASharedVector, ASharedVector> out =
            service::runTime->div_const_a(this->vector, c);

#ifdef USE_DIVISION_CORRECTION
        auto out_res = out.first;
        auto out_err = out.second;

        // Convert a2b, check < 0, and convert back
        auto correction = (!(out_err.a2b()->ltz()))->b2a_bit();

        // These are AShares, so this addition is local.
        return out_res + correction;
#else
        // If correct is disabled, ignore the error term.
        return std::make_unique<ASharedVector>(out.first);
#endif
    }

    ASharedVector& operator/=(const Share& c) {
        *this = std::move(*(*this / c));
        return *this;
    }

    /**
     * @brief Auto-conversion private elementwise division. Since our current integer division
     * algorithm only supports BSharedVector inputs, convert `this` and `other` to binary before
     * calling `BSharedVector::operator/`. Do not convert the result back.
     *
     * @param other
     * @return std::unique_ptr<BSharedVector>
     */
    std::unique_ptr<BSharedVector<Share, EVector>> operator/(const ASharedVector& other) const {
        return this->a2b() / other.a2b();
    }

    /**
     * Computes the dot product of this vector with another vector.
     * Each `aggSize` consecutive elements contribute to an exactly on dot product element in the
     * result. The size of the resulting vector is determined by the `aggSize` parameter.
     *
     * NOTE: This function is efficient when doing dot products with small `aggSize` values,
     * For larger `aggSize` values including the entire vector in the dot product, we will need to
     * to do batching different in the runtime. Currently, whole vector dot product will be executed
     * single-threaded because each threads takes multiple.
     *
     * @param other The second operand of the dot product.
     * @param aggSize The size of each dot product.
     * @return A unique pointer to a new ASharedVector that contains the result of the dot product.
     */
    std::unique_ptr<ASharedVector> dot_product(const ASharedVector& other,
                                               const size_t aggSize) const {
        // Number of elements in the dot product must match.
        assert(this->vector.size() == other.vector.size());

        // Compute the size of the resulting vector.
        auto newSize = math::div_ceil(this->vector.size(), aggSize);
        auto res = std::make_unique<ASharedVector>(newSize);

        // Compute the dot product
        service::runTime->dot_product_a(this->vector, other.vector, res->vector, aggSize);

        // Return the result as a new ASharedVector
        return res;
    }

    /**
     * @brief Negate this vector in place.
     *
     */
    void inplace_invert() { service::runTime->neg_a(this->vector, this->vector); }

    /**
     * @brief Compute chunked sums over groups of consecutive elements.
     * Simply calls down to the underlying EVector's chunkedSum method.
     *
     * @param aggSize The number of elements to aggregate in each sum.
     * @return ASharedVector containing the chunked sums.
     */
    ASharedVector chunkedSum(const size_t aggSize = 0) const {
        return ASharedVector(this->vector.chunkedSum(aggSize));
    }
};

}  // namespace orq
