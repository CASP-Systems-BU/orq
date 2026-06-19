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
 * ZeroOLE inherits from OTGenerator so that it can be used as both an OLE and OT Generator.
 *
 * @tparam T The data type for the OLE elements
 */
template <typename T>
class ZeroOLE : public OTGenerator<T> {
   public:
    /**
     * Constructor for the zero OLE generator.
     * @param rank The rank of this party.
     * @param communicator The communicator for this party.
     */
    ZeroOLE(int rank, Communicator* communicator) : OTGenerator<T>(rank, {}, communicator) {
        if (rank == 0 && !zero_showed_warning) {
            std::cout << "[ORQ] NOTE: Using Zero OLE.\n";
            zero_showed_warning = true;
        }
    }

    void randomize(Vector<T>& x) override {}

    /**
     * Generate zero OLE correlations.
     * @param n The number of OLE pairs to generate.
     * @return A tuple of two zero vectors.
     */
    OTGenerator<T>::ole_t getNext(const size_t n) override { return {Vector<T>(n), Vector<T>(n)}; }

    Vector<T> getNext(Vector<T>& A, std::optional<Vector<T>> B) override {
        return Vector<T>(A.size());
    }
};
}  // namespace orq::random