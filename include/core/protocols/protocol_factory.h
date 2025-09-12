#pragma once

#include "protocol.h"

namespace orq {

/**
 * @brief Base factory class for creating protocol instances using CRTP.
 *
 * @tparam InnerFactory The derived factory class.
 */
template <typename InnerFactory>
class ProtocolFactory {
   public:
    virtual ~ProtocolFactory() = default;

    /**
     * @brief Create a protocol instance for the given data type.
     *
     * @tparam T The data type for the protocol.
     * @param _communicator Pointer to the communicator.
     * @param _randomnessManager Pointer to the randomness manager.
     * @return Unique pointer to the created protocol instance.
     */
    template <typename T>
    std::unique_ptr<ProtocolBase> create(Communicator *_communicator,
                                         random::RandomnessManager *_randomnessManager) {
        // Delegate implementation to the derived class
        return static_cast<InnerFactory &>(*this).template create<T>(_communicator,
                                                                     _randomnessManager);
    }
};

/**
 * @brief Default protocol factory implementation.
 *
 * @tparam Protocol The protocol class template.
 * @tparam S Share type template.
 * @tparam V Vector type template.
 * @tparam E Encoding vector type template.
 */
template <template <typename, typename, typename, typename> class Protocol,
          template <typename> class S, template <typename> class V, template <typename> class E>
class DefaultProtocolFactory : public ProtocolFactory<DefaultProtocolFactory<Protocol, S, V, E>> {
    template <typename T>
    using ProtocolInstance = Protocol<T, S<T>, V<T>, E<T>>;

   public:
    /**
     * @brief Constructor for DefaultProtocolFactory.
     *
     * @param partyID The party identifier.
     * @param partiesNumber The total number of parties.
     */
    DefaultProtocolFactory(const int &partyID, const int &partiesNumber)
        : partyID_(partyID), partiesNumber_(partiesNumber) {}

    /**
     * @brief Create a protocol instance for the given data type.
     *
     * @tparam T The data type for the protocol.
     * @param communicator Pointer to the communicator.
     * @param randomnessManager Pointer to the randomness manager.
     * @return Unique pointer to the created protocol instance.
     */
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

}  // namespace orq