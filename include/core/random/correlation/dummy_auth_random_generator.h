#pragma once

#include <stdlib.h>

#include <numeric>
#include <variant>

#include "core/containers/e_vector.h"
#include "correlation_generator.h"
#include "debug/orq_debug.h"
#include "profiling/stopwatch.h"

using namespace orq::benchmarking;

namespace orq::random {

namespace testing {
    /**
     * Open additive shares across all parties.
     * @param shares The local shares to open.
     * @param pID The party ID.
     * @param pNum The number of parties.
     * @param communicator The communicator for exchanging shares.
     * @return The opened values.
     */
    template <typename T>
    orq::Vector<T> OpenAdditiveShares(const orq::Vector<T>& shares, const orq::PartyID pID,
                                      const int& pNum, Communicator* communicator) {
        // Party Information
        auto othersCount = pNum - 1;

        // For 1PC, no other parties, so just return shares
        if (othersCount <= 0) {
            return shares;
        }

        auto size = shares.size();

        std::vector<Vector<T>> sharesLocal;
        std::vector<Vector<T>> sharesRemote;
        std::vector<orq::PartyID> toPartyIDs;
        std::vector<orq::PartyID> fromPartyIDs;

        for (int i = 0; i < othersCount; ++i) {
            sharesLocal.push_back(shares);
            sharesRemote.push_back(Vector<T>(size));
            toPartyIDs.push_back(i + 1);
            fromPartyIDs.push_back(-i - 1);
        }

        // Exchanging the shares
        communicator->exchangeShares(sharesLocal, sharesRemote, toPartyIDs, fromPartyIDs);

        // Opening the shares
        Vector<T> opened(size);
        for (int i = 0; i < othersCount; ++i) {
            opened += sharesRemote[i];
        }

        // Adding the local shares
        opened += sharesLocal[0];

        return opened;
    }

    /**
     * Open a single additive share across all parties.
     * @param share The local share to open.
     * @param pID The party ID.
     * @param pNum The number of parties.
     * @param communicator The communicator for exchanging shares.
     * @return The opened value.
     */
    template <typename T>
    T OpenAdditiveShare(const T& share, const orq::PartyID pID, const int& pNum,
                        Communicator* communicator) {
        Vector<T> shares(1);
        shares[0] = share;
        auto sharesOpened = OpenAdditiveShares(shares, pID, pNum, communicator);
        return sharesOpened[0];
    }
}  // namespace testing

/**
 * @brief Base class for authenticated random generators.
 *
 * @tparam T The data type for the authenticated values
 */
template <typename T>
class AuthRandomGeneratorBase : public CorrelationGenerator {
    using tensor_t = EVector<T, 2>;

   public:
    /**
     * Constructor for the base class.
     * @param rank The rank of this party.
     */
    AuthRandomGeneratorBase(const PartyID& rank) : CorrelationGenerator(rank) {}

    virtual ~AuthRandomGeneratorBase() = default;

    /**
     * Generate authenticated random values.
     * @param n The number of values to generate.
     * @return A tensor containing values and their MACs.
     */
    virtual tensor_t getNext(const size_t& n) = 0;

    /**
     * Verify the authenticated values are correct.
     * @param bt The authenticated values to verify.
     */
    virtual void assertCorrelated(const tensor_t& bt) = 0;
};

/**
 * @brief Dummy generator for authenticated random numbers.
 *
 * Generates authenticated random numbers insecurely for testing purposes.
 * Takes a party key share [key]_i and generates ([x]_i, [x*key]_i).
 * Works for any number of parties.
 *
 * @tparam T The data type for the authenticated values
 */
template <typename T>
class DummyAuthRandomGenerator : public AuthRandomGeneratorBase<T> {
    // Authunticated random numbers are represented as two vectors.
    // If we have an `EVector<T, 2> x`, a Vector will hold
    // the secret share of the data inside x(0).
    // The othe vector x(1) will hold a secret share of the mac.
    // Each of the vectors is of `k+s` bits where k is the number of
    // bits for the data and s is the number of bits for the mask (i.e. security parameter).
    using tensor_t = EVector<T, 2>;

   public:
    /**
     * Constructor for the dummy authenticated random generator.
     * @param partiesNum The number of parties.
     * @param keyShare The key share for this party.
     * @param rank The rank of this party.
     * @param localPRG The local PRG for generating randomness.
     * @param zeroSharingGenerator The zero sharing generator.
     * @param comm The communicator for this party.
     */
    DummyAuthRandomGenerator(const int& partiesNum, const T& keyShare, const PartyID& rank,
                             std::shared_ptr<CommonPRG> localPRG,
                             std::shared_ptr<ZeroSharingGenerator> zeroSharingGenerator,
                             Communicator* comm)
        : AuthRandomGeneratorBase<T>(rank),
          partiesNum_(partiesNum),
          keyShare_(keyShare),
          key_(keyShare),
          localPRG_(localPRG),
          zeroSharingGenerator_(zeroSharingGenerator),
          comm_(comm) {
        std::vector<Vector<T>> keyLocalShares;
        std::vector<Vector<T>> keyRemoteShare;
        std::vector<orq::PartyID> toPartyIDs;
        std::vector<orq::PartyID> fromPartyIDs;

        for (int i = 0; i < partiesNum_ - 1; ++i) {
            // Initialize the key
            Vector<T> localShareVec(1);
            localShareVec[0] = keyShare;
            keyLocalShares.push_back(localShareVec);
            keyRemoteShare.push_back(Vector<T>(1));
            toPartyIDs.push_back(i + 1);
            fromPartyIDs.push_back(-i - 1);
        }

        // exchange the key shares
        comm->exchangeShares(keyLocalShares, keyRemoteShare, toPartyIDs, fromPartyIDs);

        // sum the shares
        for (int i = 0; i < partiesNum_ - 1; ++i) {
            key_ += keyRemoteShare[i][0];
        }
    }

    /**
     * Generate dummy authenticated random numbers.
     * @param n The number of authenticated values to generate.
     * @return A tensor containing the values and their MACs.
     */
    tensor_t getNext(const size_t& n) override {
        // secret shares and their MACs
        Vector<T> a(n), am(n);

        // Generate Random Numbers
        zeroSharingGenerator_->getNextArithmetic(a);
        zeroSharingGenerator_->getNextArithmetic(am);

        if (this->getRank() == 0) {
            // Generate Opened Random Numbers
            Vector<T> a_opened(n);
            localPRG_->getNext(a_opened);

            // Adjust secret shares for data and MACs
            a += a_opened;
            am += a_opened * key_;
        }

        return std::vector({a, am});
    }

    /**
     * Verify that the authenticated values are correct.
     * @param a The authenticated values to verify.
     */
    void assertCorrelated(const tensor_t& a) override {
        auto size = a.size();

        // Opening the shares and MACS
        auto a_opened =
            testing::OpenAdditiveShares(a(0), this->getRank(), this->partiesNum_, this->comm_);
        auto am_opened =
            testing::OpenAdditiveShares(a(1), this->getRank(), this->partiesNum_, this->comm_);

        // Opening the key
        auto keyOpened =
            testing::OpenAdditiveShare(keyShare_, this->getRank(), this->partiesNum_, this->comm_);

        // Calculating the MACs
        auto am_calculated = a_opened * keyOpened;

        // Asserting that the MACs are correct
        assert(am_opened.same_as(am_calculated));

        // Asserting that the result is not zero
        bool allNotZero = false;
        for (int i = 0; i < size; ++i) {
            if (a_opened[i] != 0) {
                allNotZero = true;
                break;
            }
        }
        assert(allNotZero);
        assert(keyOpened != 0);

        return /* true */;
    }

   private:
    const int partiesNum_;
    const T keyShare_;
    T key_;

    std::shared_ptr<orq::random::ZeroSharingGenerator> zeroSharingGenerator_;
    std::shared_ptr<CommonPRG> localPRG_;
    Communicator* comm_;
};

/**
 * @brief Authenticated random generator that outputs zeros.
 *
 * Used for testing where authenticated zero values are needed.
 *
 * @tparam T The data type for the authenticated values
 */
template <typename T>
class ZeroAuthRandomGenerator : public AuthRandomGeneratorBase<T> {
    using tensor_t = EVector<T, 2>;

   public:
    /**
     * Constructor for the zero authenticated random generator.
     * @param rank The rank of this party.
     */
    ZeroAuthRandomGenerator(const PartyID& rank) : AuthRandomGeneratorBase<T>(rank) {}

    /**
     * Generate zero authenticated values.
     * @param n The number of zero values to generate.
     * @return A tensor containing zero values and zero MACs.
     */
    tensor_t getNext(const size_t& n) override {
        // secret shares and their MACs
        Vector<T> a(n), am(n);
        return std::vector({a, am});
    }

    /**
     * Verify that all values are zero.
     * @param a The authenticated values to verify.
     */
    void assertCorrelated(const tensor_t& a) override {
        assert(a(0).size() == a(1).size());
        for (int i = 0; i < a(0).size(); ++i) {
            assert(a(0)[i] == 0);
            assert(a(1)[i] == 0);
        }
        return /* true */;
    }
};

// Template specialization
template <typename T>
struct CorrelationEnumType<T, Correlation::AuthRandom> {
    using type = DummyAuthRandomGenerator<T>;
};
}  // namespace orq::random
