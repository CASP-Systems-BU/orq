#pragma once

#include "debug/orq_debug.h"
#include "e_vector.h"
#include "encoded_vector.h"

namespace orq {

// Forward class declarations
template <typename Share, typename EVector>
class SharedVector;
template <typename Share, typename EVector>
class BSharedVector;
template <typename Share, typename EVector>
class ASharedVector;

namespace operators {
    template <typename Share, typename EVector>
    static void shuffle(SharedVector<Share, EVector> &x);
}

/**
 * A secret-shared vector with share and container types.
 * @tparam Share Share data type.
 * @tparam EVector Share container type.
 *
 * A SharedVector is an "encoded view" of a plaintext vector as seen from an untrusted party.
 * Different parties in a MPC protocol have different "views" of the same plaintext vector and views
 * may vary significantly across protocols. Currently, ORQ supports two techniques to construct
 * a SharedVector: *arithmetic and boolean secret sharing*. Using these techniques, a secret value
 * \f$s\f$ is encoded using \f$n > 1\f$ random "shares" such that:
 *
 * - \f$s = s_1 + s_2 + ... + s_n~\texttt{mod}~2^\ell\f$, where \f$\ell\f$ is the length of \f$s\f$
 * in bits (Arithmetic)
 *
 * - \f$s = s_1 \oplus s_2 \oplus ... \oplus s_n\f$, where \f$\oplus\f$ denotes the bitwise XOR
 * operation (Boolean)
 *
 * Let \f$v = \{4, 12, 84\}\f$ be a plaintext vector that is secret-shared by 2 parties using
 * arithmetic sharing. From the viewpoint of each party, vector \f$v\f$ will look like this:
 *
 * \f(
 * \begin{align}
 * v = \{-1, 12, 100\} \quad & \textit{(encoded view of party 1)}\\
 * v = \{5, 0, -16]\}  \quad & \textit{(encoded view of party 2)}
 * \end{align}
 * \f)
 *
 * These two vectors are in practice SharedVectors containing random numbers that add up to the
 * numbers in the original vector (which remains hidden from the parties). To reconstruct the
 * original vector in this example, the parties must "open" their SharedVector to a single entity
 * (i.e., a learner) to apply the elementwise addition. The methods to construct and open
 * SharedVectors are defined in Protocol.
 */
template <typename T, typename EVector>
class SharedVector : public EncodedVector {
   public:
    // The contents of the shared vector
    EVector vector;

    /**
     * Creates a SharedVector of size `_size` and initializes it with zeros.
     * @param _size The size of the SharedVector.
     * @param eType The encoding of the SharedVector.
     */
    explicit SharedVector(const size_t &_size, const Encoding &eType)
        : EncodedVector(eType), vector(_size) {}

    /**
     * Creates a SharedVector of size `_size` and initializes it with the contents of the file
     * `_input_file`.
     * @param _size The size of the SharedVector.
     * @param _input_file The file containing the contents of the SharedVector.
     * @param eType The encoding of the SharedVector.
     */
    explicit SharedVector(const size_t &_size, const std::string &_input_file,
                          const Encoding &eType)
        : EncodedVector(eType), vector(_size, _input_file) {}

    /**
     * Writes the secret shares of the shared vector to a file.
     * @param _output_file The file to write the secret shares to.
     */
    void outputSecretShares(const std::string &_output_file) { vector.output(_output_file); }

    /**
     * This is a shallow copy constructor from EVector.
     * @param _shares The EVector whose contents will be pointed by the SharedVector.
     * @param eType encoding
     */
    explicit SharedVector(const EVector &_shares, const Encoding &eType)
        : EncodedVector(eType), vector(_shares) {}

    /**
     * This is a move constructor from SharedVector.
     * @param secretShares The SharedVector whose contents will be moved to the new SharedVector.
     */
    SharedVector(SharedVector &&secretShares) noexcept
        : EncodedVector(secretShares.encoding), vector(secretShares.vector) {}

    SharedVector(SharedVector &secretShares)
        : EncodedVector(secretShares.encoding), vector(secretShares.vector) {}

    /**
     * Copy constructor from EncodedVector.
     * @param _shares The EncodedVector object whose contents will be copied to the new
     * SharedVector.
     */
    explicit SharedVector(EncodedVector &_shares) : EncodedVector(_shares.encoding) {
        auto secretShares_ = reinterpret_cast<SharedVector *>(&_shares);
        this->encoding = secretShares_.encoding;
        this->vector = secretShares_->vector;
    }

    // Destructor
    virtual ~SharedVector() {}

    /**
     * Opens the shared vector to all computing parties.
     * @return The opened (plaintext) vector.
     */
    Vector<T> open() const {
        // Not allowed to open non-materialized vectors, since communicators
        // don't support access patterns. If no mapping, this is a nop.
        auto v = this->vector.has_mapping() ? this->vector.materialize() : this->vector;

        return [&, this] {
            if (this->encoding == Encoding::BShared) {
                return service::runTime->open_shares_b(v);
            } else {
                return service::runTime->open_shares_a(v);
            }
        }();
    }

    void print() const {
        auto v = open();
        debug::print(v, service::runTime->getPartyID());
    }

    /**
     * @return The size of the shared vector in number of elements.
     */
    VectorSizeType size() const { return vector.size(); }

    void zero() { orq::service::runTime->modify_parallel(this->vector, &EVector::zero); }

    /**
     * @brief Resize a shared vector. If the new vector is larger, the tail
     * end will be initialized to zero. If the new vector is smaller, the
     * end of the current vector will be removed (`head` semantics).
     *
     * @param n
     */
    void resize(size_t n) { vector.resize(n); }

    /**
     * @brief Resize this vector have its last `n` elements. Does not copy.
     *
     * @param n
     */
    void tail(size_t n) { vector.tail(n); }

    SharedVector slice(size_t start, size_t end) {
        return SharedVector(this->asEVector().slice(start, end), encoding);
    }

    SharedVector slice(size_t start) { return slice(start, size()); }

    /**
     * @brief Create a deep copy (allocate new space and copy all elements)
     * of a shared vector.
     *
     * @return std::unique_ptr<SharedVector>
     */
    std::unique_ptr<SharedVector> deepcopy() {
        EVector ev(size());
        // Enforce copy
        ev = this->asEVector();
        return std::make_unique<SharedVector>(ev, encoding);
    }

    /**
     * @brief Copy-assignment between shared vector. Copies shares from `other`
     * into `this`. Pass to the runtime to enable multithreaded execution.
     *
     * @param other
     * @return SharedVector&
     */
    SharedVector &operator=(const SharedVector &other) {
        assert(this->encoding == other.encoding);
        // Multithreaded equivalent to: this->vector = other.vector;
        orq::service::runTime->modify_parallel_2arg(this->vector, other.vector,
                                                    &EVector::operator=);
        return *this;
    }

    /**
     * @brief Move-assignment between shared vectors. Takes ownership of other's
     * vector. This is not passed to the runtime because no data is copied.
     *
     * @param other
     * @return SharedVector&
     */
    SharedVector &operator=(SharedVector &&other) {
        assert(this->encoding == other.encoding);
        this->vector = std::move(other.vector);
        return *this;
    }

    /**
     * @brief Cast-and-copy assignment. Create a SharedVector from another one
     * of a different underlying type. (Replication number and encoding must
     * still match.) This allows us to up/down-cast vectors.
     *
     * @tparam T2
     * @param other
     * @return SharedVector&
     */
    template <typename T2>
    SharedVector &operator=(
        const SharedVector<T2, orq::EVector<T2, EVector::replicationNumber>> &other) {
        assert(this->encoding == other.encoding);
        // Calls overloaded EVector -> Vector cast-and-copy operators
        orq::service::runTime->modify_parallel_2arg(this->vector, other.vector,
                                                    &EVector::operator=);
        return *this;
    }

    /**
     * Transforms this vector into an EVector object.
     * @return An EVector object with the same contents as `this` shared vector.
     *
     * NOTE: This method is useful for developers who need access to the underlying shares of
     * the SharedVector that are only exposed through the EVector. Use it if you are certain
     * about what you are doing.
     */
    EVector asEVector() { return EVector(this->vector); }

    /**
     * @brief Compute a prefix sum over this shared vector
     *
     * WARNING: this does not call down to the protocol primitive but
     * instead performs a local prefix sum over the actual underlying
     * vector. This will not cause any issues now, since for all current
     * protocols, add_a is trivial. But this is not necessarily true in
     * general.
     */
    void prefix_sum() { this->vector.prefix_sum(); }

    SharedVector shuffle() {
        operators::shuffle(*this);
        return *this;
    }

    void reverse() { this->vector.reverse(); }

    /**
     * Sets the fixed-point precision.
     * @param fixed_point_precision - the number of fixed-point fractional bits.
     */
    void setPrecision(const int fixed_point_precision) {
        vector.setPrecision(fixed_point_precision);
    }

    /**
     * Gets the fixed-point precision.
     */
    size_t getPrecision() const { return vector.getPrecision(); }
};
}  // namespace orq
