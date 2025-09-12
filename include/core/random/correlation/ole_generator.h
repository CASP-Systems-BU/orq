#pragma once

#include "core/communication/communicator.h"
#include "correlation_generator.h"

namespace orq::random {

/**
 * @brief Base class for Oblivious Linear Evaluation generators.
 *
 * @tparam T The data type for the correlation elements
 * @tparam E The encoding type (BShared or AShared)
 */
template <typename T, orq::Encoding E>
class OLEGenerator : public CorrelationGenerator {
   public:
    using ole_t = std::tuple<Vector<T>, Vector<T>>;
    std::optional<Communicator *> comm;

    /**
     * Constructor for the OLE generator.
     * @param rank The rank of this party.
     * @param _comm Optional communicator for verification.
     */
    OLEGenerator(int rank, std::optional<Communicator *> _comm)
        : CorrelationGenerator(rank), comm(_comm) {}

    /**
     * Generate OLE correlations.
     * @param n The number of OLEs to generate.
     * @return A tuple of two vectors representing the OLE correlation.
     */
    virtual ole_t getNext(size_t n) = 0;

    /**
     * Verify that the OLE correlation is correct.
     * @param ole The OLE correlation to verify.
     */
    void assertCorrelated(ole_t &ole) {
        if (!comm.has_value()) {
            if (getRank() == 0) {
                std::cout << "Skipping OLE check: communicator not defined\n";
            }
            return;
        }

        auto n = get<0>(ole).size();

        Vector<T> A(n), B(n), C(n), D(n);

        // P0 has {A, C}
        // P1 has {B, D}

        if (getRank() == 0) {
            std::tie(A, C) = ole;

            // Receive B and D from P1
            comm.value()->receiveShares(B, 1, B.size());
            comm.value()->receiveShares(D, 1, D.size());

            // Check A + B = C * D
            if constexpr (E == orq::Encoding::BShared) {
                assert((A ^ B).same_as(C & D));
            } else {
                // TODO: if any product is zero, we leak. Need to update PRG
                // auto prod = C * D;
                // for (int i = 0; i < n; i++) {
                //     assert(prod[i] != 0);
                // }

                assert((A + B).same_as(C * D));
            }
        } else {
            // P1 sends B and D to P0.
            std::tie(B, D) = ole;

            comm.value()->sendShares(B, -1, B.size());
            comm.value()->sendShares(D, -1, D.size());
            return;
        }
    }
};

// Template specialization
template <typename T>
struct CorrelationEnumType<T, Correlation::rOT> {
    using type = OLEGenerator<T, Encoding::BShared>;
};

template <typename T>
struct CorrelationEnumType<T, Correlation::OLE> {
    using type = OLEGenerator<T, Encoding::AShared>;
};
}  // namespace orq::random
