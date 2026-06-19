#pragma once

#ifdef MPC_PROTOCOL_BEAVER_TWO
#include "beaver_triple_generator.h"
#include "libsecjoin.h"
#include "ole_generator.h"
#include "ot_generator.h"
#endif

namespace orq::random {

// clang-format off

/**
 * @brief A tuple of shared pointers to correlations. Access via type-indexing.
 * 
 * @tparam T 
 */
template <typename T>
using TypedCorrelations_t = std::tuple<
#ifdef MPC_PROTOCOL_BEAVER_TWO
    std::shared_ptr<OTGenerator<T>>,
    std::shared_ptr<OLEGenerator<T>>,
    std::shared_ptr<BeaverTripleGenerator<T, orq::Encoding::AShared>>,
    std::shared_ptr<BeaverTripleGenerator<T, orq::Encoding::BShared>>,
    std::shared_ptr<OPRF>
#else
    // This is necessary to make the compiler happy; CorrRegistry_t needs unique types
    // (We only use correlated randomness for 2PC)
    T
#endif
>;
// TODO: add ShardedPermutationGenerator here

// clang-format on

/**
 * @brief A tuple of all types used by the system. Access the correlation registry by type first,
 * then look up the specific correlation (from the resulting TypedCorrelations_t).
 *
 */
using CorrRegistry_t = std::tuple<TypedCorrelations_t<int8_t>, TypedCorrelations_t<int16_t>,
                                  TypedCorrelations_t<int32_t>, TypedCorrelations_t<int64_t>,
                                  TypedCorrelations_t<__int128_t>>;

}  // namespace orq::random