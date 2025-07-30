#pragma once

#include "communicator.h"

namespace secrecy {

template <typename InnerFactory>
class CommunicatorFactory {
   public:
    virtual ~CommunicatorFactory() = default;

    std::unique_ptr<Communicator> create() {
        // Delegate implementation to the derived class
        return static_cast<InnerFactory &>(*this).create();
    }

    void start() { return static_cast<InnerFactory &>(*this).start(); }

    int getPartyId() const { return static_cast<InnerFactory &>(*this).getPartyId(); }

    int getNumParties() const { return static_cast<InnerFactory &>(*this).getNumParties(); }

    void blockingReady() { static_cast<InnerFactory &>(*this).blockingReady(); }
};

}  // namespace secrecy
