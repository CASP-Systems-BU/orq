#pragma once

#include <typeindex>

#include "correlation/correlation_generator.h"
#include "correlation/libsecjoin.h"
#include "correlation/zero_sharing_generator.h"
#include "permutations/sharded_permutation_generator.h"
#include "prg/common_prg.h"
#include "prg/prg_algorithm.h"

// include this last
#include "correlation/registry.h"

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
    // Sharded permutation generator (used by PermutationManager)
    std::shared_ptr<ShardedPermutationGenerator> sharded_perm_generator;

   public:
    // private PRG that is exclusive to the current party
    std::shared_ptr<CommonPRG> localPRG;

    // manager for CommonPRG objects that are shared with other parties
    std::shared_ptr<CommonPRGManager> commonPRGManager;

    // zero sharing generator
    std::shared_ptr<ZeroSharingGenerator> zeroSharingGenerator;

    std::unique_ptr<CommittedSeedsQueue<>> csq;

    // A collection of correlations
    CorrRegistry_t corr_registry;

    /**
     *  Constructor for the randomness manager.
     * @param _localPRG The local PRG for this party.
     * @param _commonPRGManager The manager for common PRGs.
     * @param _zeroSharingGenerator The zero sharing generator.
     * @param _corrGen Registry of correlation generators.
     */
    RandomnessManager(std::shared_ptr<CommonPRGManager> _commonPRGManager,
                      std::shared_ptr<ZeroSharingGenerator> _zeroSharingGenerator,
                      CorrRegistry_t&& _corrGen, std::unique_ptr<CommittedSeedsQueue<>> csq)
        : localPRG(_commonPRGManager->get({})),
          commonPRGManager(_commonPRGManager),
          zeroSharingGenerator(_zeroSharingGenerator),
          corr_registry(std::move(_corrGen)),
          csq(std::move(csq)),
          sharded_perm_generator(nullptr) {}

    /**
     * Overload that accepts a sharded permutation generator.
     */
    RandomnessManager(std::shared_ptr<CommonPRGManager> _commonPRGManager,
                      std::shared_ptr<ZeroSharingGenerator> _zeroSharingGenerator,
                      CorrRegistry_t&& _corrGen, std::unique_ptr<CommittedSeedsQueue<>> csq,
                      std::shared_ptr<ShardedPermutationGenerator> _sharded)
        : localPRG(_commonPRGManager->get({})),
          commonPRGManager(_commonPRGManager),
          zeroSharingGenerator(_zeroSharingGenerator),
          corr_registry(std::move(_corrGen)),
          csq(std::move(csq)),
          sharded_perm_generator(std::move(_sharded)) {}

    /**
     * Fills a vector with local randomness.
     * @tparam T The data type for the random values.
     * @param nums The vector to fill.
     */
    template <typename T>
    void generate_local(Vector<T>& nums) {
        localPRG->getNext(nums);
    };

    /**
     * Fills a vector with randomness common among a group.
     * @tparam T The data type for the random values.
     * @param nums The vector to fill.
     * @param group The group that shares the CommonPRG seed.
     */
    template <typename T>
    void generate_common(Vector<T>& nums, std::set<int> group) {
        commonPRGManager->get(group)->getNext(nums);
    };

    /**
     * Calls the arithmetic Beaver triple generator's reserve() function.
     * @tparam T The data type for the triple elements.
     * @param n The number of triples to generate.
     */
    template <typename T>
    void reserve_mul_triples(size_t n) {
#ifdef MPC_PROTOCOL_BEAVER_TWO
        getCorrelation<T, BeaverMulGenerator<T>>()->reserve(n);
#endif
    }

    /**
     * Calls the binary Beaver triple generator's reserve() function.
     * @tparam T The data type for the triple elements.
     * @param n The number of triples to generate.
     */
    template <typename T>
    void reserve_and_triples(size_t n) {
#ifdef MPC_PROTOCOL_BEAVER_TWO
        getCorrelation<T, BeaverAndGenerator<T>>()->reserve(n);
#endif
    }

    /**
     * Get a correlation generator for the specified type and correlation.
     * @tparam T The data type for the correlation elements.
     * @tparam C The correlation type.
     * @return A shared pointer to the correlation generator.
     */
    template <typename T, typename Corr>
    auto& getCorrelation() {
        // Specialization for perm gen, which is not currently templated on T
        if constexpr (std::is_same_v<Corr, ShardedPermutationGenerator>) {
            return sharded_perm_generator;
        } else {
            auto& typed = std::get<TypedCorrelations_t<T>>(corr_registry);
            return std::get<std::shared_ptr<Corr>>(typed);
        }
    }
};

}  // namespace orq::random
