#pragma once

#include "protocol.h"

namespace secrecy {

template <typename InnerFactory>
class ProtocolFactory {
   public:
    virtual ~ProtocolFactory() = default;

    template <typename T>
    std::unique_ptr<ProtocolBase> create(Communicator *_communicator,
                                         random::RandomnessManager *_randomnessManager) {
        // Delegate implementation to the derived class
        return static_cast<InnerFactory &>(*this).template create<T>(_communicator,
                                                                     _randomnessManager);
    }
};

template <template <typename, typename, typename, typename> class Protocol,
          template <typename> class S, template <typename> class V, template <typename> class E>
class DefaultProtocolFactory : public ProtocolFactory<DefaultProtocolFactory<Protocol, S, V, E>> {
    template <typename T>
    using ProtocolInstance = Protocol<T, S<T>, V<T>, E<T>>;

   public:
    DefaultProtocolFactory(const int &partyID, const int &partiesNumber)
        : partyID_(partyID), partiesNumber_(partiesNumber) {}

    template <typename T>
    std::unique_ptr<ProtocolBase> create(Communicator *communicator,
                                         random::RandomnessManager *randomnessManager) {
        // Create and return a new instance of the protocol
        return std::make_unique<ProtocolInstance<T>>(partyID_, communicator, randomnessManager);
    }

   private:
    const int partyID_;
    const int partiesNumber_;
};

}  // namespace secrecy