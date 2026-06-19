#pragma once
#include <cstdint>

/**
 * @file primes.h
 * @brief Defines prime number constants used throughout the codebase.
 *
 * This file provides named constants for commonly used primes, including
 * Mersenne primes which are useful for cryptographic operations.
 */

#include <NTL/ZZ.h>

#include "util.h"

namespace orq::math::primes {

/**
 * @brief All primes that fit within 8 bits (1 byte).
 *
 */
const std::vector<uint8_t> PRIMES_WITHIN_BYTE{
    2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,  53,  59,  61,
    67,  71,  73,  79,  83,  89,  97,  101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151,
    157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251};

/**
 * @brief Return the first primorial with more than `reqBits` bits; that is,
 * the smallest \f$n#\f$ such that \f$\log2(n#)>\mathtt{reqBits}-1\f$.
 *
 * Only consider primes < 256, or primorials with fewer than 335 bits.
 *
 * @param reqBits
 * @return std::pair<NTL::ZZ, int> The primorial and the number of primes, or zero and negative one
 * if no such primorial was found.
 */
std::pair<NTL::ZZ, int> primorial_gt_bits(int reqBits) {
    NTL::ZZ x(1);
    for (int i = 0; i < PRIMES_WITHIN_BYTE.size(); i++) {
        x *= PRIMES_WITHIN_BYTE[i];
        if (orq::math::log2(x) >= reqBits) {
            return {x, i};
        }
    }

    // Ran out of primes. Return 0.
    return {NTL::ZZ(), -1};
}

/**
 * @brief Mersenne primes of the form 2^p - 1.
 *
 * Mersenne primes are primes of the form M_p = 2^p - 1, where p itself must be prime.
 */

/// Mersenne prime M_61 = 2^61 - 1
constexpr uint64_t MERSENNE_61 = (1ULL << 61) - 1;

/// Mersenne prime M_127 = 2^127 - 1
constexpr __uint128_t MERSENNE_127 = ((__uint128_t)1 << 127) - 1;

}  // namespace orq::math::primes
