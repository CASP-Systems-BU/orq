#pragma once

#include "communicator.h"

namespace orq {

/**
 * @brief A factory or a communicator object. Parameterized by the subclass
 * type.
 *
 * @tparam InnerFactory
 */
template <typename InnerFactory>
class CommunicatorFactory {
   public:
    virtual ~CommunicatorFactory() = default;

    /**
     * @brief Create a unique pointer to a new communicator
     *
     * @return std::unique_ptr<Communicator>
     */
    std::unique_ptr<Communicator> create() {
        // Delegate implementation to the derived class
        return static_cast<InnerFactory &>(*this).create();
    }

    /**
     * @brief Start the communicator
     *
     */
    void start() { return static_cast<InnerFactory &>(*this).start(); }

    /**
     * @brief Get the Party ID of this node
     *
     * @return int
     */
    int getPartyId() const { return static_cast<InnerFactory &>(*this).getPartyId(); }

    /**
     * @brief Get the number of parties in this execution
     *
     * @return int
     */
    int getNumParties() const { return static_cast<InnerFactory &>(*this).getNumParties(); }

    /**
     * @brief If this communicator is blocking, wait until it is ready
     *
     */
    void blockingReady() { static_cast<InnerFactory &>(*this).blockingReady(); }
};

}  // namespace orq
