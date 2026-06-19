#pragma once

#include "core/communication/communicator.h"
#include "ole_generator.h"

namespace orq::random {

bool dummy_showed_warning = false;

/**
 * @brief Insecure dummy OLE generator base class for testing. Uses a common-seed PRG
 * to choose a random OLE correlation, `Y = A * X - B`, where party 0 gets {A, B} and party 1 gets
 * {X, Y}.
 *
 * @tparam T underlying datatype
 */
template <typename T, typename I = T>
class DummyBase {
   private:
    std::shared_ptr<CommonPRG> all_prg;

    Communicator* _comm;
    int _rank;

    /**
     * @brief Virtual method to get the resulting correlated value given three others
     *
     * @param B additive shared
     * @param C multiplicative share
     * @param D multiplicative share
     * @return Vector<I>
     */
    virtual Vector<I> getCorrelated(Vector<T>& B, Vector<T>& C, Vector<T>& D) = 0;

   public:
    /**
     * Constructor for the dummy OLE generator.
     * @param rank The rank of this party.
     * @param common The CommonPRGManager for shared randomness.
     * @param communicator The communicator for this party.
     */
    DummyBase(int rank, std::shared_ptr<CommonPRGManager> common, Communicator* communicator)
        : _comm(communicator), _rank(rank) {
        all_prg = common->get({0, 1});

        if (_rank == 0 && !dummy_showed_warning) {
            std::cout << "[ORQ] NOTE: Using Dummy OLE.\n";
            dummy_showed_warning = true;
        }

        if (all_prg == NULL) {
            std::cerr << "CommonPRG for all {0,1} was NULL\n";
            exit(-1);
        }
    }

    /**
     * Generate dummy OLE correlations.
     * @param n The number of OLE pairs to generate.
     * @return A tuple of two vectors representing the OLE correlation.
     */
    OLEGenerator<T, I>::ole_t getNext(const size_t n) {
        // seed the OLE with random vectors
        // auto n = corr.size();
        Vector<T> B(n), C(n), D(n);

        all_prg->getNext(B);
        all_prg->getNext(C);
        all_prg->getNext(D);

        auto A = getCorrelated(B, C, D);

        if (_rank == 0) {
            return {C, A};
        } else {
            return {D, B};
        }
    }

    /**
     * @brief Generate chosen-message OLE correlations. P0 sends its shares to P1, who computes the
     * correlation.
     *
     * @param mul_share
     * @param add_share
     * @return Vector<I>
     */
    Vector<I> getNext(Vector<T>& mul_share, std::optional<Vector<T>> add_share) {
        // std::cout << "DummyBase getNext(m, a)\n";
        auto n = mul_share.size();
        if (_rank == 0) {
            _comm->sendShares(mul_share, 1);
            _comm->sendShares(*add_share, 1);
            return *add_share;
        } else {
            Vector<T> mul0(n), t(n);
            _comm->receiveShares(mul0, -1);
            _comm->receiveShares(t, -1);

            return getCorrelated(t, mul0, mul_share);
        }
    }
};

template <typename T>
class DummyOT : public OTGenerator<T>, DummyBase<T> {
    using Base = DummyBase<T>;

   public:
    using OTGenerator<T>::assertCorrelated;

    DummyOT(int rank, std::shared_ptr<CommonPRGManager> common, Communicator* communicator)
        : OTGenerator<T>(rank, common, communicator), DummyBase<T>(rank, common, communicator) {}

    virtual Vector<T> getNext(Vector<T>& m, std::optional<Vector<T>> a) override {
        return Base::getNext(m, a);
    }

    /**
     * @brief Specialization for Dummy OT: compute over binary field.
     *
     * @param xor_
     * @param and0
     * @param and1
     * @return Vector<T>
     */
    virtual Vector<T> getCorrelated(Vector<T>& xor_, Vector<T>& and0, Vector<T>& and1) override {
        return (and0 & and1) ^ xor_;
    }
};

/**
 * @brief Specialization for arbitrary OLE. Works for any type T over which *, - are defined.
 *
 * @tparam T
 * @tparam I
 */
template <typename T, typename I = T>
class DummyOLE : public OLEGenerator<T, I>, DummyBase<T, I> {
    using Base = DummyBase<T, I>;

   public:
    using OLEGenerator<T, I>::assertCorrelated;
    using Base::getNext;

    DummyOLE(int rank, std::shared_ptr<CommonPRGManager> common, Communicator* communicator)
        : OLEGenerator<T, I>(rank, common, communicator), Base(rank, common, communicator) {}

    virtual Vector<I> getNext(Vector<T>& m, std::optional<Vector<T>> a) override {
        return Base::getNext(m, a);
    }

    Vector<I> getCorrelated(Vector<T>& add, Vector<T>& mul0, Vector<T>& mul1) override {
        return static_cast<Vector<I>>(mul0) * mul1 - add;
    }
};

}  // namespace orq::random
