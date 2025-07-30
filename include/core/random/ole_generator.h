#ifndef SECRECY_OLE_GENERATOR_H
#define SECRECY_OLE_GENERATOR_H

#include "correlation_generator.h"
#include "../communication/communicator.h"

namespace secrecy::random {

    template <typename T, secrecy::Encoding E>
    class OLEGenerator : public CorrelationGenerator {
        
    public:
        using ole_t = std::tuple<Vector<T>, Vector<T>>;
        std::optional<Communicator*> comm;

        OLEGenerator(int rank, std::optional<Communicator*> _comm) : CorrelationGenerator(rank), comm(_comm) {}
        // OLEGenerator(int rank) : CorrelationGenerator(rank) {}
        
        virtual ole_t getNext(size_t n) = 0;

        void assertCorrelated(ole_t &ole) {
            if (! comm.has_value()) {
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
                if constexpr (E == secrecy::Encoding::BShared) {
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
    template<typename T>
    struct CorrelationEnumType<T, Correlation::rOT> {
        using type = OLEGenerator<T, Encoding::BShared>;
    };

    template<typename T>
    struct CorrelationEnumType<T, Correlation::OLE> {
        using type = OLEGenerator<T, Encoding::AShared>;
    };
} // namespace secrecy::random

#endif // SECRECY_OLE_GENERATOR_H