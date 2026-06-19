#pragma once

namespace orq {
typedef size_t VectorSizeType;

/**
 * @brief A helper template to construct variables with double the bitwidth.
 *
 * Note: DoubleWidth<128> is intentionally undefined. Protocols which require wider bitwidths should
 * fail to compile when instantiated with 128-bit base types. Alternate approaches may be required,
 * or we could implement a 256-bit type.
 *
 * @tparam T
 */
template <typename T>
struct DoubleWidth;

template <>
struct DoubleWidth<int8_t> {
    using type = int16_t;
};
template <>
struct DoubleWidth<int16_t> {
    using type = int32_t;
};
template <>
struct DoubleWidth<int32_t> {
    using type = int64_t;
};
template <>
struct DoubleWidth<int64_t> {
    using type = __int128_t;
};

template <>
struct DoubleWidth<uint8_t> {
    using type = uint16_t;
};
template <>
struct DoubleWidth<uint16_t> {
    using type = uint32_t;
};
template <>
struct DoubleWidth<uint32_t> {
    using type = uint64_t;
};
template <>
struct DoubleWidth<uint64_t> {
    using type = __uint128_t;
};

}  // namespace orq

#include "core/math/util.h"
#include "gf2e_overloads.h"

#ifdef ZEROPC_DUMMY_VECTOR
#include "dummy_vector.h"
#else
#ifdef USE_INDEX_MAPPED_VECTOR
#include "mapping_access_vector.h"
#else
#include "class_access_vector.h"
#endif
#endif