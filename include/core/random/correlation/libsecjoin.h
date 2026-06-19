/**
 * Correlations that we use with libSecureJoin.
 *
 * If the library is not included, use a mock header to define classes.
 *
 */

#pragma once

#if defined(USE_LIBOTE) && defined(USE_SECURE_JOIN)
#include "core/random/correlation/oprf.h"
#else
#include "core/random/correlation/mock.h"
#endif