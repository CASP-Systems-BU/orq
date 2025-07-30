#ifndef SECRECY_CORRELATION_GENERATOR_H
#define SECRECY_CORRELATION_GENERATOR_H

#include "random_generator.h"

namespace secrecy
{
    namespace random
    {
        enum class Correlation {
            rOT,
            OLE,
            BeaverMulTriple,
            BeaverAndTriple,
            AuthMulTriple,
            AuthRandom,
            ZeroSharing,
            Common,
            ShardedPermutation
        };
        
        /**
         * @brief Base correlation generator class. This is non-functional and
         * just used for organizational purposes. All correlation generators
         * should inherit from this class, and implement (at least) the
         * `getNext()` and `assertCorrelated()` methods, with the property that
         * 
         *   CG.assertCorrelated(CG.getNext())
         * 
         * always succeeds.
         * 
         * TODO: figure out some way to make the inheritance actually enforced.
         * Currently, can't make these virtual methods, so need to cast to the
         * specific instance.
         * 
         * Seems like the way to go is std::any and type erasure...
         */
        class CorrelationGenerator : public RandomGenerator
        {
            const int rank;
        
        public:
            CorrelationGenerator(int _rank) : RandomGenerator(0), rank(_rank) {}

            // These methods ignored; just for doc purposes
            
            template <typename... Ts>
            std::tuple<Ts...> getNext(size_t n) const;

            template <typename... Ts>
            void assertCorrelated(std::tuple<Ts...> C) {}

            int getRank() const {
                return rank;
            }
        };

        // Type conversion template for `getCorrelation<T, C>()`
        // Specialized versions defined within class files
        // 
        // When adding a new correlation, add the appropriate enum-to-type
        // conversion as well.
        template<typename T, Correlation>
        struct CorrelationEnumType;
    }
}

#endif // SECRECY_CORRELATION_GENERATOR_H