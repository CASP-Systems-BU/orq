#ifndef SECRECY_RANDOMNESS_MANGAGER_H
#define SECRECY_RANDOMNESS_MANGAGER_H

#include <typeindex>

#include "prg_algorithm.h"
#include "common_prg.h"
#include "zero_sharing_generator.h"
#include "ole_generator.h"

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

namespace secrecy::random {
    
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
        std::map<typed_correlation, CorrelationGenerator*> correlationGenerators;

        // empty constructor
        RandomnessManager(
            std::shared_ptr<CommonPRG> _localPRG,
            std::shared_ptr<CommonPRGManager> _commonPRGManager,
            std::shared_ptr<ZeroSharingGenerator> _zeroSharingGenerator,
            std::map<typed_correlation, CorrelationGenerator*> _corrGen = {}) : 
                localPRG(_localPRG),
                commonPRGManager(_commonPRGManager),
                zeroSharingGenerator(_zeroSharingGenerator),
                correlationGenerators(_corrGen)
            {}
        
        /**
         * generate_local - Fills a vector with local randomness.
         * @param nums - The vector to fill.
         */
        template <typename T>
        void generate_local(Vector<T> &nums) {
            localPRG->getNext(nums);
        };
        
        /**
         * generate_common - Fills a vector with randomness common among a group.
         * @param nums - The vector to fill.
         * @param group - The group that shares the CommonPRG seed.
         */
        template <typename T>
        void generate_common(Vector<T> &nums, std::set<int> group) {
            commonPRGManager->get(group)->getNext(nums);
        };

        /**
         * reserve_mul_triples - Calls the arithmetic Beaver triple generator's reserve() function.
         * @param n - The number of triples to generate.
         */
        template <typename T>
        void reserve_mul_triples(size_t n) {
            getCorrelation<T, Correlation::BeaverMulTriple>()->reserve(n);
        }

        /**
         * reserve_and_triples - Calls the binary Beaver triple generator's reserve() function.
         * @param n - The number of triples to generate.
         */
        template <typename T>
        void reserve_and_triples(size_t n) {
            getCorrelation<T, Correlation::BeaverAndTriple>()->reserve(n);
        }

        template <typename T, Correlation C>
        typename CorrelationEnumType<T, C>::type * getCorrelation() {
            using ret_t = random::CorrelationEnumType<T, C>::type;

            return reinterpret_cast<ret_t *>(correlationGenerators[{__typeid(T), C}]);
        }

    };
} // namespace secrecy::random

#endif