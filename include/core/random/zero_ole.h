/**
 * @file zero_ole.h
 * @brief Like Dummy OLE, but actually outputs all zero.
 *
 */
#pragma once

#include "ole_generator.h"

namespace secrecy::random {

bool zero_showed_warning = false;

template <typename T, secrecy::Encoding E>
class ZeroOLE : public OLEGenerator<T, E> {
   public:
    ZeroOLE(int rank, Communicator *communicator) : OLEGenerator<T, E>(rank, communicator) {
        if (rank == 0 && !zero_showed_warning) {
            std::cout << "NOTE: Using Zero OLE.\n";
            zero_showed_warning = true;
        }
    }

    OLEGenerator<T, E>::ole_t getNext(size_t n) { return {Vector<T>(n), Vector<T>(n)}; }
};
}  // namespace secrecy::random