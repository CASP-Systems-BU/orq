#pragma once

#include "../manager.h"
#include "../prg/common_prg.h"
#include "core/communication/communicator.h"
#include "correlation_generator.h"

namespace orq::random {
/**
 * @brief Base class for Oblivious Linear Evaluation generators. By default, arithmetic
 * correlations.
 *
 * @tparam T The data type of the correlation
 * @tparam I The intermediate data type (may be larger to prevent overflow)
 */
template <typename T, typename I = T>
class OLEGenerator : public CorrelationGenerator<std::tuple<Vector<T>, Vector<I>>> {
   public:
    using value_type = T;
    /**
     * @brief OLE correlation type. By convention, the first share is multiplicative; the second
     * is the additive share.
     *
     */
    using ole_t = std::tuple<Vector<T>, Vector<I>>;
    std::optional<Communicator*> comm;
    std::shared_ptr<CommonPRG> localPRG;

    /**
     * Constructor for the OLE generator.
     * @param rank The rank of this party.
     * @param _comm Optional communicator for verification. If no communicator is provided, the base
     * class will skip verification.
     */
    OLEGenerator(int rank, std::shared_ptr<CommonPRGManager> manager,
                 std::optional<Communicator*> _comm)
        : CorrelationGenerator<ole_t>(rank), comm(_comm) {
        if (manager) {
            localPRG = manager->get({});
        }
    }

    /**
     * @brief Default implementation: full randomization
     *
     * @param x
     */
    virtual void randomize(Vector<T>& x) { localPRG->getNext(x); }

    /**
     * Generate random OLE correlations by calling the `randomize` function.
     *
     * @param n The number of OLEs to generate.
     * @return A tuple of two vectors representing the OLE correlation.
     */
    virtual ole_t getNext(const size_t n) {
        Vector<T> m(n);
        randomize(m);

        if (this->rank == 0) {
            Vector<T> a(n);
            randomize(a);
            return {m, getNext(m, a)};
        } else {
            return {m, getNext(m, {})};
        }
    }

    virtual Vector<I> getNext(Vector<T>& m, std::optional<Vector<T>> a) = 0;

    /**
     * @brief Check if the vectors have an OLE correlation.
     *
     * Subclasses can overload with pdutheir own correlation.
     *
     * @param A additive share
     * @param B additive share
     * @param C multiplicative share
     * @param D multiplicative share
     * @return true
     * @return false
     */
    virtual bool isCorrelated(Vector<T>& A, Vector<T>& B, Vector<T>& C, Vector<T>& D) const {
        return (A + B).same_as(C * D);
    }

    /**
     * Verify that the OLE correlation is correct. Aborts if not correlated.
     *
     * @param ole The OLE correlation to verify.
     */
    void assertCorrelated(const ole_t& ole) const {
        if (!comm.has_value()) {
            if (this->rank == 0) {
                std::cout << "Skipping OLE check: communicator not defined\n";
            }
            return;
        }

        auto n = get<0>(ole).size();

        Vector<T> A(n), B(n), C(n), D(n);

        // P0 has {A, C}
        // P1 has {B, D}
        if (this->rank == 0) {
            std::tie(C, A) = ole;

            // Receive B and D from P1
            comm.value()->receiveShares(B, 1);
            comm.value()->receiveShares(D, 1);

            // P0 checks A + B = C * D
            assert(isCorrelated(A, B, C, D));
        } else {
            // P1 sends B and D to P0.
            std::tie(D, B) = ole;

            comm.value()->sendShares(B, -1);
            comm.value()->sendShares(D, -1);
        }
    }
};

/**
 * @brief Minimal type-erased interface for mod-prime OLE generators.
 *
 * Any concrete `DerivedOLEmodP<T, Impl>` should implement this interface, allowing
 * GilboaCRT to operate uniformly without knowing `Impl`.
 */
template <typename T>
class OLEmodPrimeInterface {
    static_assert(std::is_unsigned_v<T>, "OLEmodPrimeInterface: T must be unsigned!");

   public:
    using wide_t = typename DoubleWidth<T>::type;

   private:
    T prime = 2;

    T maxT = std::numeric_limits<T>::max();
    wide_t maxW = std::numeric_limits<wide_t>::max();

   public:
    virtual ~OLEmodPrimeInterface() = default;

    void setPrime(T newPrime) { prime = newPrime; }

    T getPrime() const { return prime; }

    int primeBits() const { return std::bit_width(prime); }

    using ole_t = std::tuple<Vector<T>, Vector<wide_t>>;

    /**
     * @brief Reduce x mod p
     *
     * @param x
     * @return Vector<T>
     */
    virtual Vector<T> reduce(Vector<wide_t> x) const {
        Vector<T> out(x.size());

        for (int i = 0; i < x.size(); i++) {
            auto y = x[i];
            if (y >= maxW - maxT) {
                // "negative" result: y wrapped around, so compute the negative value mod prime
                wide_t negValue = (maxW - y) + 1;
                out[i] = (prime - (negValue % prime)) % prime;
            } else {
                // "positive" result
                out[i] = y % prime;
            }
        }
        return out;
    }

    // Core OLE operations
    virtual void randomize(Vector<T>& x) = 0;
    virtual Vector<wide_t> getNext(Vector<T>& mul_share, std::optional<Vector<T>> add_share) = 0;
    virtual std::tuple<Vector<T>, Vector<wide_t>> getNext(const size_t n) = 0;
    virtual void assertCorrelated(const std::tuple<Vector<T>, Vector<wide_t>>&) const = 0;

    virtual const int getRank() const = 0;
    virtual Communicator* getComm() const = 0;
};

/**
 * @brief Abstract class for OLE mod prime, derived from a larger mod 2^k OLE.
 *
 * @tparam T
 */
template <typename T, typename Impl>
class DerivedOLEmodP : public Impl, public OLEmodPrimeInterface<T> {
    static_assert(std::is_base_of_v<OLEGenerator<T, typename DoubleWidth<T>::type>, Impl>,
                  "OLEmodP: Impl must derive from OLEGenerator with matching T and widened type");

   public:
    using wide_t = typename DoubleWidth<T>::type;
    using Base = Impl;
    using Base::Base;  // inherit constructors
    using typename Base::ole_t;
    using Prime = OLEmodPrimeInterface<T>;

    DerivedOLEmodP(const Base& b) : Base(b) {}

    /**
     * @brief Insecure default implementation: randomize, then reduce mod prime.
     *
     * This version introduces some bias and should be fixed with rejection sampling.
     */
    void randomize(Vector<T>& x) override {
        Base::randomize(x);
        x = Prime::reduce(x);
        std::cout << "WARNING: calling insecure randomization function.\n";
    }

    const int getRank() const override { return Base::rank; }

    Communicator* getComm() const override { return Base::comm.value_or(nullptr); }

    // Satisfy OLEmodPrimeInterface's pure virtual by delegating to underlying implementation
    void assertCorrelated(const std::tuple<Vector<T>, Vector<wide_t>>& ole) const override {
        Base::assertCorrelated(ole);
    }

    /**
     * @brief Get an OLE correlation mod prime using the underlying implementation, then reduce.
     */
    Vector<wide_t> getNext(Vector<T>& mul_share, std::optional<Vector<T>> add_share) override {
        return Prime::reduce(Base::getNext(mul_share, add_share));
    }

    ole_t getNext(size_t n) override {
        auto [x, y] = Base::getNext(n);
        return {Prime::reduce(x), Prime::reduce(y)};
    }

    bool isCorrelated(Vector<T>& A, Vector<T>& B, Vector<T>& C, Vector<T>& D) const override {
        // make sure all values are valid
        auto p = this->getPrime();

        if (!((A < p).all_true() && (B < p).all_true() && (C < p).all_true() &&
              (D < p).all_true())) {
            std::cout << "Values out of range!\n";
            return false;
        }

        // prevent overflow by casting up to the wide type
        return (((static_cast<Vector<wide_t>>(A) + B) % p)
                    .same_as((static_cast<Vector<wide_t>>(C) * D) % p));
    }
};

// Deduction guide: deduce both T and Impl from constructor argument
// This allows us to pass existing OLEGenerators into OLEmodP and infer everything else!
template <typename Impl>
DerivedOLEmodP(const Impl&) -> DerivedOLEmodP<typename Impl::value_type, Impl>;

}  // namespace orq::random
