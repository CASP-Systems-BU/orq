#pragma once

#include <concepts>
#include <type_traits>

template <typename U,
          bool UseUnsigned = (std::integral<U> && !std::is_same_v<std::remove_cv_t<U>, bool>)>
struct UnsignedTypeSelector {
    using type = U;
};

template <typename U>
struct UnsignedTypeSelector<U, true> {
    using type = std::make_unsigned_t<U>;
};