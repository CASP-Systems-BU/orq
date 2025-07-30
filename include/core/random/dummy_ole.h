#ifndef SECRECY_DUMMY_OLE_H
#define SECRECY_DUMMY_OLE_H

#include "ole_generator.h"

namespace secrecy::random {

bool dummy_showed_warning = false;

/**
 * @brief Insecure dummy OLE generator for testing. Uses a common-seed PRG
 * to choose a random OLE correlation, `A + B = C * D`, where party 0
 * gets {A, C} and party 1 gets {B, D}.
 *
 * @tparam T underlying datatype
 */
template <typename T, secrecy::Encoding E>
class DummyOLE : public OLEGenerator<T, E> {
    using OLEBase = OLEGenerator<T, E>;

    std::shared_ptr<CommonPRG> all_prg;

   public:
    DummyOLE(int rank, std::shared_ptr<CommonPRGManager> common, Communicator* communicator)
        : OLEBase(rank, communicator) {
        all_prg = common->get({0, 1});

        if (rank == 0 && !dummy_showed_warning) {
            std::cout << "NOTE: Using Dummy OLE.\n";
            dummy_showed_warning = true;
        }

        if (all_prg == NULL) {
            std::cerr << "CommonPRG for all {0,1} was NULL\n";
            exit(-1);
        }
    }

    OLEBase::ole_t getNext(size_t n) {
        // seed the OLE with random vectors
        // auto n = corr.size();
        Vector<T> A(n), B(n), C(n), D(n);

        all_prg->getNext(B);
        all_prg->getNext(C);
        all_prg->getNext(D);

        if constexpr (E == secrecy::Encoding::BShared) {
            A = B ^ C & D;
        } else {
            A = C * D - B;
        }

        if (OLEBase::getRank() == 0) {
            return {A, C};
        } else {
            return {B, D};
        }
    }
};
}  // namespace secrecy::random

#endif  // SECRECY_DUMMY_OLE_H