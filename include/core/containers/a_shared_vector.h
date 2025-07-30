#ifndef SECRECY_A_SHARED_VECTOR_H
#define SECRECY_A_SHARED_VECTOR_H

#include "shared_vector.h"

namespace secrecy {

/**
 * A SharedVector that contains arithmetic shares and supports secure arithmetic operations.
 * @tparam Share - Share data type.
 * @tparam EVector - Share container type.
 */
template <typename Share, typename EVector>
class ASharedVector : public SharedVector<Share, EVector> {
   public:
    /**
     * Creates an ASharedVector of size `_size` and initializes it with zeros.
     * @param _size - The size of the ASharedVector.
     */
    explicit ASharedVector(const size_t& _size)
        : SharedVector<Share, EVector>(_size, Encoding::AShared) {}

    /**
     * Creates an ASharedVector of size `_size` and initializes it with secret shares in the given
     * file.
     * @param _size - The size of the ASharedVector.
     * @param _input_file - The file that contains the secret shares.
     */
    explicit ASharedVector(const size_t& _size, const std::string& _input_file)
        : SharedVector<Share, EVector>(_size, _input_file, Encoding::AShared) {}

    /**
     * This is a shallow copy constructor from EVector contents.
     * @param _shares - The EVector whose contents will be pointed by the ASharedVector.
     */
    explicit ASharedVector(EVector& _shares)
        : SharedVector<Share, EVector>(_shares, Encoding::AShared) {}

    /**
     * This is a move constructor from EVector contents.
     * @param _shares - The EVector whose contents will be moved to the new ASharedVector.
     */
    ASharedVector(EVector&& _shares) : SharedVector<Share, EVector>(_shares, Encoding::AShared) {}

    /**
     * This is a move constructor from another ASharedVector.
     * @param other - The ASharedVector whose contents will be moved to the new ASharedVector.
     */
    ASharedVector(ASharedVector&& other) noexcept
        : SharedVector<Share, EVector>(other.vector, other.encoding) {}

    /**
     * This is a copy constructor from another ASharedVector.
     * @param other - The ASharedVector whose contents will be moved to the new ASharedVector.
     */
    ASharedVector(const ASharedVector& other)
        : SharedVector<Share, EVector>(other.vector, other.encoding) {}

    /**
     * Copy constructor from SharedVector contents.
     * @param _shares - The SharedVector object whose contents will be copied to the new
     * ASharedVector.
     */
    explicit ASharedVector(SharedVector<Share, EVector>& _shares)
        : EncodedVector(_shares.encoding) {
        assert(_shares.encoding == Encoding::AShared);
        auto secretShares_ = reinterpret_cast<ASharedVector*>(&_shares);
        this->vector = secretShares_->vector;
    }

    /**
     * Move constructor that creates an ASharedVector from a unique pointer to an EncodedVector
     * object.
     * @param base - The pointer to the SharedVector object whose contents will be moved to the new
     * ASharedVector.
     */
    ASharedVector(std::unique_ptr<ASharedVector>&& base)
        : ASharedVector((ASharedVector*)base.get()) {}

    /**
     * Shallow copy constructor that creates an ASharedVector from a unique pointer to an
     * EncodedVector object.
     * @param base - The SharedVector object whose contents will be pointed by the new
     * ASharedVector.
     */
    ASharedVector(std::unique_ptr<ASharedVector>& base)
        : ASharedVector((ASharedVector*)base.get()) {}

    /**
     * Move constructor that creates an ASharedVector from a pointer to another ASharedVector
     * object.
     * @param _base - The ASharedVector that will be moved as a whole (contents + state) to the new
     * ASharedVector.
     *
     * NOTE: This constructor is implicitly called by the two constructors above.
     */
    explicit ASharedVector(ASharedVector* _base) : ASharedVector(std::move(*_base)) {}

    using SharedVector<Share, EVector>::operator=;

    ASharedVector& operator=(const ASharedVector&) = default;
    ASharedVector& operator=(ASharedVector&&) = default;

    // Destructor
    virtual ~ASharedVector() {}

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
     * This is a conversion from ASharedVector to BSharedVector.
     */
    using B = BSharedVector<Share, EVector>;
    std::unique_ptr<B> a2b() const {
        std::pair<B, B> v = service::runTime->redistribute_shares_b(this->vector);
        // Calls binary adder
        return v.first + v.second;
    }

    // **************************************** //
    //           Arithmetic operators           //
    // **************************************** //

    /**
     * Elementwise secure arithmetic addition.
     * @param other - The second operand of addition.
     * @return A unique pointer to a new shared vector that contains arithmetic shares of
     * the elementwise additions.
     */
    binary_op(+, ASharedVector, add_a, this, other);

    /**
     * Elementwise secure arithmetic subtraction.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of subtraction.
     * @return A unique pointer to a new shared vector that contains arithmetic shares of
     * the elementwise subtractions.
     */
    binary_op(-, ASharedVector, sub_a, this, other);

    /**
     * Elementwise secure arithmetic multiplication.
     * This operator expects both input vectors (`this` and `other`) to have the same size.
     * @param other - The second operand of multiplication.
     * @return A unique pointer to a new shared vector that contains arithmetic shares of
     * the elementwise multiplications.
     */
    binary_op(*, ASharedVector, multiply_a, this, other);

    /**
     * Elementwise secure arithmetic negation.
     * This operator expects the input vector (`this`) to contain arithmetic shares.
     * @return A unique pointer to a new shared vector with all arithmetic shares of
     * `this` vector negated.
     */
    unary_op(-, ASharedVector, neg_a, this);

    binary_element_op(+, add_a, ASharedVector, Share);
    binary_element_op(-, add_a, ASharedVector, Share);
    binary_element_op(*, add_a, ASharedVector, Share);

    compound_assignment_op(+=, add_a, ASharedVector);
    compound_assignment_op(-=, sub_a, ASharedVector);
    compound_assignment_op(*=, multiply_a, ASharedVector);

    compound_assignment_element_op(+=, add_a, ASharedVector, Share);
    compound_assignment_element_op(-=, sub_a, ASharedVector, Share);
    compound_assignment_element_op(*=, multiply_a, ASharedVector, Share);

    std::unique_ptr<ASharedVector> operator/(const Share& c) const {
        // compute division and error correction shares
        std::pair<ASharedVector, ASharedVector> out =
            service::runTime->div_const_a(this->vector, c);

#ifdef USE_DIVISION_CORRECTION
        auto out_res = out.first;
        auto out_err = out.second;

        // Convert a2b, check < 0, and convert back
        auto correction = (!(out_err.a2b()->ltz()))->b2a_bit();

        return out_res + correction;
#else
        return std::make_unique<ASharedVector>(out.first);
#endif
    }

    /**
     * @brief Auto-conversion A/A division. Since private division may often
     * occur as a result of aggregation (i.e., averages), this auto-conversion
     * operator is provided. Note that this incurs to cost of two a2b calls.
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
     * @param other - The second operand of the dot product.
     * @param aggSize - The size of each dot product.
     * @return A unique pointer to a new ASharedVector that contains the result of the dot product.
     */
    std::unique_ptr<ASharedVector> dot_product(const ASharedVector& other,
                                               const int aggSize) const {
        // Number of elements in the dot product must match.
        assert(this->vector.size() == other.vector.size());

        // Compute the size of the resulting vector.
        auto newSize = div_ceil(this->vector.size(), aggSize);
        auto res = std::make_unique<ASharedVector>(newSize);

        // Compute the dot product
        service::runTime->dot_product_a(this->vector, other.vector, res->vector, aggSize);

        // Return the result as a new ASharedVector
        return res;
    }
};

}  // namespace secrecy

#endif  // SECRECY_A_SHARED_VECTOR_H
