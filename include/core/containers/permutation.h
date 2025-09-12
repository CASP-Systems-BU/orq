#ifndef PERMUTATION_H
#define PERMUTATION_H

#include <algorithm>
#include <numeric>

#include "core/random/permutations/dm_dummy.h"
#include "core/random/permutations/permutation_manager.h"
#include "shared_vector.h"

namespace orq {
// type definitions
using Group = std::set<int>;
using LocalPermutation = std::vector<int>;

// forward class declarations
template <typename EVector>
class ElementwisePermutation;

// declare the necessary operators from shuffle.h
namespace operators {
    template <typename Share, typename EVector>
    void oblivious_apply_sharded_perm(
        SharedVector<Share, EVector> &x,
        std::shared_ptr<orq::random::ShardedPermutation> &permutation);

    template <typename Share, typename EVector>
    void oblivious_apply_inverse_sharded_perm(
        SharedVector<Share, EVector> &x,
        std::shared_ptr<orq::random::ShardedPermutation> &permutation);

    template <typename Share, typename EVector, typename EVectorPerm>
    void oblivious_apply_elementwise_perm(SharedVector<Share, EVector> &x,
                                          ElementwisePermutation<EVectorPerm> &perm);
}  // namespace operators

template <typename EVector>
class ElementwisePermutation {
    // type definitions
    using SharedPerm = SharedVector<int, orq::EVector<int, EVector::replicationNumber>>;
    using ASharedPerm = ASharedVector<int, orq::EVector<int, EVector::replicationNumber>>;
    using BSharedPerm = BSharedVector<int, orq::EVector<int, EVector::replicationNumber>>;

   public:
    // the underlying SharedVector for the permutation
    SharedPerm sharedVector;

    /**
     * Default constructor which creates an identity permutation of a given size.
     * @param size The size of the permutation.
     *
     * By default, this constructs an arithmetic sharing of the identity permutation.
     * This constructor simply calls the next constructor with the default AShared sharing type.
     */
    ElementwisePermutation(size_t size) : ElementwisePermutation(size, Encoding::AShared) {}

    /**
     * Constructor which creates an identity permutation of a given size with a given encoding.
     * @param size The size of the permutation.
     * @param encoding The encoding type of the underlying SharedVector (arithmetic or binary).
     */
    ElementwisePermutation(size_t size, Encoding encoding) : sharedVector(size, encoding) {
        Vector<int> identity(size);
        std::iota(identity.begin(), identity.end(), 0);

        // public sharing does the same thing for AShared and BShared
        sharedVector.vector =
            orq::service::runTime->public_share<EVector::replicationNumber>(identity);
    }

    /**
     * Copy constructor which takes another ElementwisePermutation as input.
     * @param permutation The permutation to copy.
     */
    ElementwisePermutation(ElementwisePermutation &permutation)
        : sharedVector(*permutation.sharedVector.deepcopy()) {}

    /**
     * Constructor which takes a SharedVector as input and assigns it to the underlying
     * SharedVector.
     * @param v The SharedVector to copy.
     *
     * The second template argument is named argEVector to distinguish it from the EVector that this
     * class is templated by. This function copies the underlying data to a 32 bit SharedVector,
     * which requires that the input vector has a bitwidth <= 32.
     */
    template <typename Share, typename argEVector>
    ElementwisePermutation(const SharedVector<Share, argEVector> &v)
        : sharedVector(v.vector, v.encoding) {
        // ensure we're not discarding data when converting to 32 bits
        const int bit_length = std::numeric_limits<std::make_unsigned_t<Share>>::digits;
        static_assert(bit_length <= 32);
    }

    /**
     * Copy assignment operator.
     * @param other The permutation to copy.
     */
    ElementwisePermutation &operator=(const ElementwisePermutation &other) {
        // not a self-assignment
        if (this != &other) {
            this->sharedVector.vector = other.sharedVector.vector;
            this->sharedVector.encoding = other.sharedVector.encoding;
        }

        return *this;
    }

    /**
     * Assignment operator from a SharedVector
     * @param other The SharedVector to copy.
     *
     * The second template argument is named argEVector to distinguish it from the EVector that this
     * class is templated by. This function copies the underlying data to a 32 bit SharedVector,
     * which requires that the input vector has a bitwidth <= 32.
     */
    template <typename Share, typename argEVector>
    ElementwisePermutation &operator=(const SharedVector<Share, argEVector> &other) {
        sharedVector.vector = other.vector;
        sharedVector.encoding = other.encoding;
        return *this;
    }

    /**
     * Gets the size of the underlying SharedVector.
     */
    size_t size() { return sharedVector.size(); }

    /**
     * Gets the underlying SharedVector as an ASharedVector.
     */
    ASharedPerm getASharedPerm() {
        ASharedPerm v(size());
        v.vector = sharedVector.vector;
        return v;
    }

    /**
     * Gets the underlying SharedVector as an BSharedVector.
     */
    BSharedPerm getBSharedPerm() {
        BSharedPerm v(size());
        v.vector = sharedVector.vector;
        return v;
    }

    /**
     * Gets the encoding type of the underlying SharedVector.
     */
    orq::Encoding getEncoding() { return sharedVector.encoding; }

    /**
     * Open the underlying SharedVector.
     */
    Vector<int> open() {
        Vector<int> opened = sharedVector.open();
        return opened;
    }

    /**
     * Shuffle the underlying SharedVector
     * @return The shuffled permutation.
     */
    ElementwisePermutation shuffle() {
        sharedVector.shuffle();
        return *this;
    }

    /**
     * Reverse the direction of the permutation.
     * @return A permutation such that when applied to a vector, the permuted vector will be the
     * reverse of the vector permuted under the original permutation.
     *
     * Each element (with some value x) will now become size - 1 - x.
     * Multiply by -1 then add (size - 1).
     */
    ElementwisePermutation reverse() {
        assert(sharedVector.encoding == Encoding::AShared);

        Vector<int> negative_one_plaintext = {-1};
        Vector<int> additive_constant_plaintext = {static_cast<int>(size()) - 1};

        ASharedPerm negative_one =
            orq::service::runTime->public_share<EVector::replicationNumber>(negative_one_plaintext)
                .repeated_subset_reference(size());
        ASharedPerm additive_constant =
            orq::service::runTime
                ->public_share<EVector::replicationNumber>(additive_constant_plaintext)
                .repeated_subset_reference(size());

        ASharedPerm v = getASharedPerm();
        v *= negative_one;
        v += additive_constant;

        sharedVector.vector = v.vector;

        return *this;
    }

    /**
     * Find the inverse of the permutation (different than applying an inverse
     * permutation).
     * @return The inverse of the permutation.
     */
    ElementwisePermutation invert() {
        // initialize the inverse to the identity permutation
        ElementwisePermutation<EVector> inverse(size(), this->getEncoding());

        orq::operators::oblivious_apply_elementwise_perm(inverse.sharedVector, *this);
        sharedVector.vector = inverse.sharedVector.vector;
        return *this;
    }

    /**
     * Convert a binary shared ElementwisePermutation to an arithmetic shared
     * ElementwisePermutation.
     * @return An ElementwisePermutation with an arithmetic sharing of the input permutation.
     *
     * Apply a random permutation, open the result, secret share it arithmetically,
     *      then unapply the random permutation.
     */
    ElementwisePermutation b2a() {
        assert(sharedVector.encoding == Encoding::BShared);

        // do a direct conversion for 2PC rather than the permutation-specific protocol below
        if (runTime->getNumParties() == 2) {
            ASharedPerm a = getBSharedPerm().b2a();

            // assign the arithmetic shared result to this permutation
            sharedVector.vector = a.vector;
            sharedVector.encoding = Encoding::AShared;

            return *this;
        }

        // make a deepcopy
        ElementwisePermutation<EVector> permutation(*this);

        // generate a random sharded permutation
        auto pi = random::PermutationManager::get()->getNext<int>(size(), Encoding::BShared);

        // shuffle the permutation according to pi
        orq::operators::oblivious_apply_sharded_perm(permutation.sharedVector, pi);

        // open pi(perm)
        Vector<int> pi_perm = permutation.open();

        ASharedPerm ret = orq::service::runTime->public_share<EVector::replicationNumber>(pi_perm);

        // unapply the random permutation
        orq::operators::oblivious_apply_inverse_sharded_perm(ret, pi);

        // assign the arithmetic shared result to this permutation
        sharedVector.vector = ret.vector;
        sharedVector.encoding = Encoding::AShared;

        return *this;
    }

    /**
     * Convert a permutation with negative location values to one with positive location
     * values.
     * @return An ElementwisePermutation with the correct range of values.
     *
     * The input is has values from -1 to -n. We want to negate them (so 1 to n) and then
     *      shift down by 1 (0 to n-1).
     */
    ElementwisePermutation negate() {
        Vector<int> negative_one_plaintext = {-1};

        ASharedPerm negative_one =
            orq::service::runTime->public_share<EVector::replicationNumber>(negative_one_plaintext)
                .repeated_subset_reference(size());

        ASharedPerm v(sharedVector.vector);
        v *= negative_one;
        v += negative_one;

        return *this;
    }
};

}  // namespace orq

#endif  // PERMUTATION_H
