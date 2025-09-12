/**
 * @file zero_ole.h
 * @brief Like Dummy OLE, but actually outputs all zero.
 *
 */
#pragma once

#include "ole_generator.h"

namespace orq::random {

bool zero_showed_warning = false;

/**
 * @brief OLE generator that outputs all zeros.
 *
 * Used for testing and benchmarking where actual security is not required.
 *
 * @tparam T The data type for the OLE elements
 * @tparam E The encoding type
 */
template <typename T, orq::Encoding E>
class ZeroOLE : public OLEGenerator<T, E> {
   public:
    /**
     * Constructor for the zero OLE generator.
     * @param rank The rank of this party.
     * @param communicator The communicator for this party.
     */
    ZeroOLE(int rank, Communicator *communicator) : OLEGenerator<T, E>(rank, communicator) {
        if (rank == 0 && !zero_showed_warning) {
            std::cout << "NOTE: Using Zero OLE.\n";
            zero_showed_warning = true;
        }
    }

    /**
     * Generate zero OLE correlations.
     * @param n The number of OLE pairs to generate.
     * @return A tuple of two zero vectors.
     */
    OLEGenerator<T, E>::ole_t getNext(size_t n) { return {Vector<T>(n), Vector<T>(n)}; }
};
}  // namespace orq::random