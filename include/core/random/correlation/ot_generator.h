#pragma once

#include "core/communication/communicator.h"
#include "correlation_generator.h"
#include "ole_generator.h"

namespace orq::random {

/**
 * @brief OT Generator subclasses OLE Generator just by providing a different correlation. Other
 * abstract functionality is the same.
 *
 * @tparam T
 */
template <typename T>
class OTGenerator : public OLEGenerator<T> {
    using OLEGenerator<T>::OLEGenerator;

   public:
    virtual bool isCorrelated(Vector<T>& A, Vector<T>& B, Vector<T>& C,
                              Vector<T>& D) const override {
        return (A ^ B).same_as(C & D);
    }

    /**
     * @brief Base class random correlation generator. By default this does nothing since
     * OLEGenerator<T> does not implement getNext(n), but if an OTGenerator is constructed from an
     * OLEGenerator implementing that function, it will inherit.
     *
     */
    using OLEGenerator<T>::getNext;

    virtual Vector<T> getNext(Vector<T>& m, std::optional<Vector<T>> a) override {
        // TODO: chosen message OT not supported yet.
        throw std::logic_error("not implemented");
    }
};
}  // namespace orq::random