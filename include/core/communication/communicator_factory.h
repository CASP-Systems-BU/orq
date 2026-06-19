#pragma once

#include "backend/common/setting.h"
#include "communicator.h"

namespace orq {

struct CommFactoryArgs {
    int argc;
    char **argv;
    int numThreads = 1;
    int msgTag = 1000;

    // Network Environment
    double latency = 0;
    double bandwidth = std::numeric_limits<double>::infinity();
    std::string host_prefix = "node";
    orq::service::Setting setting = orq::service::Setting::SAME;
};

/**
 * @brief A factory or a communicator object. Parameterized by the subclass
 * type.
 *
 * @tparam InnerFactory
 */
template <typename InnerFactory>
class CommunicatorFactory {
   protected:
    /**
     * @brief pairwise latency of this communicator, milliseconds. Passed into Communicator
     *
     */
    double latency;

    /**
     * @brief pairwise bandwidth of this communicator, Gbps. Passed into Communicator
     *
     */
    double bandwidth;

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
     * @brief If this communicator is blocking, wait until it is ready
     *
     */
    void blockingReady() { static_cast<InnerFactory &>(*this).blockingReady(); }

    /**
     * @brief Get the network setting (same/lan/wan) according to the runtime.
     *
     * @return The Setting enum value for this execution.
     */
    auto getSetting() const { return static_cast<const InnerFactory &>(*this).getSetting(); }
};

}  // namespace orq
