#pragma once

#include <typeindex>

#include "correlation/ole_generator.h"
#include "correlation/zero_sharing_generator.h"
#include "prg/common_prg.h"
#include "prg/prg_algorithm.h"

#ifdef __APPLE__
// Apple clang doesn't have full 128-bit integer support. Mostly this doesn't
// cause issues, except for the fact that `typeid(__int128_t)` is not defined,
// causing a linker error. If we're on Apple, use this macro to shim in the
// typeid of this empty struct instead of __int128_t.
//
// On non-Apple we have not run into this issue and can call `typeid` as normal.
struct __int128_apple {};
#define __typeid(T) (std::is_same<T, __int128_t>::value ? typeid(__int128_apple) : typeid(T))
#else
#define __typeid(T) (typeid(T))
#endif

namespace orq::random {

/**
 * @brief Manages various sources of randomness and correlations.
 *
 * Coordinates local PRGs, common PRGs, zero sharing generators, and correlation generators.
 */
class RandomnessManager {
   public:
    // private PRG that is exclusive to the current party
    std::shared_ptr<CommonPRG> localPRG;

    // manager for CommonPRG objects that are shared with other parties
    std::shared_ptr<CommonPRGManager> commonPRGManager;

    // zero sharing generator
    std::shared_ptr<ZeroSharingGenerator> zeroSharingGenerator;

    // A collection of correlations
    using typed_correlation = std::tuple<std::type_index, Correlation>;
    std::map<typed_correlation, CorrelationGenerator *> correlationGenerators;

    /**
     *  Constructor for the randomness manager.
     * @param _localPRG The local PRG for this party.
     * @param _commonPRGManager The manager for common PRGs.
     * @param _zeroSharingGenerator The zero sharing generator.
     * @param _corrGen Map of correlation generators.
     */
    RandomnessManager(std::shared_ptr<CommonPRG> _localPRG,
                      std::shared_ptr<CommonPRGManager> _commonPRGManager,
                      std::shared_ptr<ZeroSharingGenerator> _zeroSharingGenerator,
                      std::map<typed_correlation, CorrelationGenerator *> _corrGen = {})
        : localPRG(_localPRG),
          commonPRGManager(_commonPRGManager),
          zeroSharingGenerator(_zeroSharingGenerator),
          correlationGenerators(_corrGen) {}

    /**
     * Fills a vector with local randomness.
     * @tparam T The data type for the random values.
     * @param nums The vector to fill.
     */
    template <typename T>
    void generate_local(Vector<T> &nums) {
        localPRG->getNext(nums);
    };

    /**
     * Fills a vector with randomness common among a group.
     * @tparam T The data type for the random values.
     * @param nums The vector to fill.
     * @param group The group that shares the CommonPRG seed.
     */
    template <typename T>
    void generate_common(Vector<T> &nums, std::set<int> group) {
        commonPRGManager->get(group)->getNext(nums);
    };

    /**
     * Calls the arithmetic Beaver triple generator's reserve() function.
     * @tparam T The data type for the triple elements.
     * @param n The number of triples to generate.
     */
    template <typename T>
    void reserve_mul_triples(size_t n) {
        getCorrelation<T, Correlation::BeaverMulTriple>()->reserve(n);
    }

    /**
     * Calls the binary Beaver triple generator's reserve() function.
     * @tparam T The data type for the triple elements.
     * @param n The number of triples to generate.
     */
    template <typename T>
    void reserve_and_triples(size_t n) {
        getCorrelation<T, Correlation::BeaverAndTriple>()->reserve(n);
    }

    /**
     * Get a correlation generator for the specified type and correlation.
     * @tparam T The data type for the correlation elements.
     * @tparam C The correlation type.
     * @return A pointer to the correlation generator.
     */
    template <typename T, Correlation C>
    typename CorrelationEnumType<T, C>::type *getCorrelation() {
        using ret_t = random::CorrelationEnumType<T, C>::type;

        return reinterpret_cast<ret_t *>(correlationGenerators[{__typeid(T), C}]);
    }
};
}  // namespace orq::random
