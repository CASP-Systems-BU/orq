#ifndef SECRECY_DUMMY_AUTH_TRIPLE_GENERATOR_H
#define SECRECY_DUMMY_AUTH_TRIPLE_GENERATOR_H

#include "../../benchmark/stopwatch.h"
#include "../../debug/debug.h"
#include "../containers/e_vector.h"
#include "correlation_generator.h"
#include "dummy_auth_random_generator.h"
using namespace secrecy::benchmarking;

#include <stdlib.h>

#include <variant>

namespace secrecy::random {

template <typename T>
class AuthTripleGeneratorBase : public CorrelationGenerator {
    using S = EVector<T, 2>;
    using triple_t = std::tuple<S, S, S>;
    using vec_t = Vector<T>;

   public:
    AuthTripleGeneratorBase(const PartyID& rank) : CorrelationGenerator(rank) {}
    virtual ~AuthTripleGeneratorBase() = default;

    virtual triple_t getNext(const size_t& n) = 0;
    virtual void assertCorrelated(const triple_t& bt) = 0;
};

// This class generates random authenticated beaver triples
// using a dummy generator. It is used for testing purposes only.
// It takes a party key share `[key]_i` and generates
// {([a]_i, [a*key]_i), ([b]_i, [b*key]_i), ([c]_i, [c*key]_i)}
// where c = a * b.
// It works for all parties numbers.
template <typename T>
class DummyAuthTripleGenerator : public AuthTripleGeneratorBase<T> {
    // Authunticated random numbers are represented as two vectors.
    // If we have an `EVector<T, 2> x`, a Vector will hold
    // the secret share of the data inside x(0).
    // The othe vector x(1) will hold a secret share of the mac.
    // Each of the vectors is of `k+s` bits where k is the number of
    // bits for the data and s is the number of bits for the mask (i.e. security parameter).
    using S = EVector<T, 2>;
    using triple_t = std::tuple<S, S, S>;
    using vec_t = Vector<T>;

   public:
    DummyAuthTripleGenerator(const int& partiesNum, const T& keyShare, const PartyID& rank,
                             std::shared_ptr<CommonPRG> localPRG,
                             std::shared_ptr<ZeroSharingGenerator> zeroSharingGenerator,
                             Communicator* comm)
        : AuthTripleGeneratorBase<T>(rank),
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

    triple_t getNext(const size_t& n) override {
        // Triples
        Vector<T> a(n), b(n), c(n);
        Vector<T> am(n), bm(n), cm(n);

        // Make the secret shares (zero secret shares).
        zeroSharingGenerator_->getNextArithmetic(a);
        zeroSharingGenerator_->getNextArithmetic(b);
        zeroSharingGenerator_->getNextArithmetic(c);
        zeroSharingGenerator_->getNextArithmetic(am);
        zeroSharingGenerator_->getNextArithmetic(bm);
        zeroSharingGenerator_->getNextArithmetic(cm);

        if (this->getRank() == 0) {
            // Generate Opened Triples
            Vector<T> a_opened(n), b_opened(n);
            localPRG_->getNext(a_opened);
            localPRG_->getNext(b_opened);
            auto c_opened = a_opened * b_opened;

            // Adjust secret shares
            a += a_opened;
            b += b_opened;
            c += c_opened;

            // Calculate Opened Triples MACs and adjust their secret shares.
            am += a_opened * key_;
            bm += b_opened * key_;
            cm += c_opened * key_;
        }

        return triple_t(std::vector({a, am}), std::vector({b, bm}), std::vector({c, cm}));
    }

    void assertCorrelated(const triple_t& bt) override {
        // unpacking the shares
        const auto [a, b, c] = bt;
        auto size = a.size();

        // Opening the shares
        auto a_opened =
            testing::OpenAdditiveShares(a(0), this->getRank(), this->partiesNum_, this->comm_);
        auto b_opened =
            testing::OpenAdditiveShares(b(0), this->getRank(), this->partiesNum_, this->comm_);
        auto c_opened =
            testing::OpenAdditiveShares(c(0), this->getRank(), this->partiesNum_, this->comm_);

        // Calculating the result
        auto c_calculated = a_opened * b_opened;

        // Asserting that the result is correct
        assert(c_opened.same_as(c_calculated));

        // Making sure that the results are not zeros
        // We make sure that not all results are zeros.
        bool allNotZero = false;
        for (int i = 0; i < size; ++i) {
            if (c_opened[i] != 0) {
                allNotZero = true;
                break;
            }
        }
        assert(allNotZero);

        // Opening the key
        auto keyOpened =
            testing::OpenAdditiveShare(keyShare_, this->getRank(), this->partiesNum_, this->comm_);

        // Opening the shares MACs
        auto am_opened =
            testing::OpenAdditiveShares(a(1), this->getRank(), this->partiesNum_, this->comm_);
        auto bm_opened =
            testing::OpenAdditiveShares(b(1), this->getRank(), this->partiesNum_, this->comm_);
        auto cm_opened =
            testing::OpenAdditiveShares(c(1), this->getRank(), this->partiesNum_, this->comm_);

        // Asserting Calculating the MACs
        auto am_calculated = a_opened * keyOpened;
        auto bm_calculated = b_opened * keyOpened;
        auto cm_calculated = c_opened * keyOpened;

        // Asserting that the MACs are correct
        assert(am_opened.same_as(am_calculated));
        assert(bm_opened.same_as(bm_calculated));
        assert(cm_opened.same_as(cm_calculated));
        return /* true */;
    }

   private:
    // Generator parameters
    const int partiesNum_;
    const T keyShare_;
    T key_;

    // Helpers
    std::shared_ptr<secrecy::random::ZeroSharingGenerator> zeroSharingGenerator_;
    std::shared_ptr<CommonPRG> localPRG_;
    Communicator* comm_;
};

template <typename T>
class ZeroAuthTripleGenerator : public AuthTripleGeneratorBase<T> {
    using S = EVector<T, 2>;
    using triple_t = std::tuple<S, S, S>;
    using vec_t = Vector<T>;

   public:
    ZeroAuthTripleGenerator(const PartyID& rank) : AuthTripleGeneratorBase<T>(rank) {}

    triple_t getNext(const size_t& n) override {
        Vector<T> a(n), b(n), c(n);
        Vector<T> am(n), bm(n), cm(n);
        return triple_t(std::vector({a, am}), std::vector({b, bm}), std::vector({c, cm}));
    }

    void assertCorrelated(const triple_t& bt) override {
        // unpacking the shares
        const auto [a, b, c] = bt;
        assert(a.size() == b.size() && b.size() == c.size());
        for(int i = 0; i < a.size(); ++i) {
            assert(a(0)[i] == 0);
            assert(b(0)[i] == 0);
            assert(c(0)[i] == 0);
            assert(a(1)[i] == 0);
            assert(b(1)[i] == 0);
            assert(c(1)[i] == 0);
        }
        return /* true */;
    }
};

// Template specialization
template <typename T>
struct CorrelationEnumType<T, Correlation::AuthMulTriple> {
    using type = AuthTripleGeneratorBase<T>;
};

}  // namespace secrecy::random

#endif  // SECRECY_DUMMY_AUTH_TRIPLE_GENERATOR_H
