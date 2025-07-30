#ifndef SECRECY_DUMMY_AUTH_RANDOM_GENERATOR_H
#define SECRECY_DUMMY_AUTH_RANDOM_GENERATOR_H

#include <stdlib.h>

#include <numeric>
#include <variant>

#include "../../benchmark/stopwatch.h"
#include "../../debug/debug.h"
#include "../containers/e_vector.h"
#include "correlation_generator.h"
using namespace secrecy::benchmarking;

namespace secrecy::random {

namespace testing {
    template <typename T>
    secrecy::Vector<T> OpenAdditiveShares(const secrecy::Vector<T>& shares,
                                          const secrecy::PartyID& pID, const int& pNum,
                                          Communicator* communicator) {
        // Party Information
        auto othersCount = pNum - 1;

        // For 1PC, no other parties, so just return shares
        if (othersCount <= 0) {
            return shares;
        }

        auto size = shares.size();

        std::vector<Vector<T>> sharesLocal;
        std::vector<Vector<T>> sharesRemote;
        std::vector<secrecy::PartyID> toPartyIDs;
        std::vector<secrecy::PartyID> fromPartyIDs;

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

    template <typename T>
    T OpenAdditiveShare(const T& share, const secrecy::PartyID& pID, const int& pNum,
                        Communicator* communicator) {
        Vector<T> shares(1);
        shares[0] = share;
        auto sharesOpened = OpenAdditiveShares(shares, pID, pNum, communicator);
        return sharesOpened[0];
    }
}  // namespace testing

template <typename T>
class AuthRandomGeneratorBase : public CorrelationGenerator {
    using tensor_t = EVector<T, 2>;

   public:
    AuthRandomGeneratorBase(const PartyID& rank) : CorrelationGenerator(rank) {}
    virtual ~AuthRandomGeneratorBase() = default;

    virtual tensor_t getNext(const size_t& n) = 0;
    virtual void assertCorrelated(const tensor_t& bt) = 0;
};

// This class generates random authenticated numbers
// using a dummy generator. It is used for testing purposes only.
// It takes a party key share `[key]_i` and generates ([x]_i, [x*key]_i)
// It works for all number of parties.
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
        std::vector<secrecy::PartyID> toPartyIDs;
        std::vector<secrecy::PartyID> fromPartyIDs;

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

    std::shared_ptr<secrecy::random::ZeroSharingGenerator> zeroSharingGenerator_;
    std::shared_ptr<CommonPRG> localPRG_;
    Communicator* comm_;
};

template <typename T>
class ZeroAuthRandomGenerator : public AuthRandomGeneratorBase<T> {
    using tensor_t = EVector<T, 2>;

   public:
    ZeroAuthRandomGenerator(const PartyID& rank) : AuthRandomGeneratorBase<T>(rank) {}

    tensor_t getNext(const size_t& n) override {
        // secret shares and their MACs
        Vector<T> a(n), am(n);
        return std::vector({a, am});
    }

    void assertCorrelated(const tensor_t& a) override {
        assert(a(0).size() == a(1).size());
        for(int i = 0; i < a(0).size(); ++i) {
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
}  // namespace secrecy::random

#endif  // SECRECY_DUMMY_AUTH_RANDOM_GENERATOR_H
