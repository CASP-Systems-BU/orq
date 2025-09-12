#pragma once

/**
 * @file seeded_prg.h
 * @brief Implements PRG via CommonPRG with random seed.
 * @date 2024-10-29
 *
 *
 */

#include <sodium.h>

#include "common_prg.h"
#include "debug/orq_debug.h"
#include "prg_algorithm.h"
#include "random_generator.h"

#define SEED_NUM_BYTES 4

namespace orq::random {
/**
 * @brief PRG implementation using CommonPRG with seed.
 *
 * Wraps CommonPRG with automatic seed generation or allows fixed seeding.
 */
class PseudoRandomGenerator : public RandomGenerator {
    std::unique_ptr<CommonPRG> cprg;

   public:
    /**
     * Constructor with automatic random seed generation.
     */
    PseudoRandomGenerator() : RandomGenerator() {
        std::vector<unsigned char> seed_vec(crypto_aead_aes256gcm_KEYBYTES);

        AESPRGAlgorithm::aesKeyGen(seed_vec);

        // only use the first four bytes
        seed_vec.erase(seed_vec.begin() + SEED_NUM_BYTES, seed_vec.end());

        std::cout << "Random seed: " << debug::container2str(seed_vec) << "\n";

        std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm =
            std::make_unique<orq::random::AESPRGAlgorithm>(seed_vec);
        cprg = std::make_unique<CommonPRG>(std::move(prg_algorithm), 0);
    }

    /**
     * Constructor with fixed seed.
     * @param _seed The fixed seed to use.
     */
    PseudoRandomGenerator(std::vector<unsigned char> _seed) : RandomGenerator() {
        std::cout << "Fixed seed: " << debug::container2str(_seed) << "\n";
        std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm =
            std::make_unique<orq::random::AESPRGAlgorithm>(_seed);
        cprg = std::make_unique<CommonPRG>(std::move(prg_algorithm), 0);
    }

    /**
     * Generate a random value.
     * @tparam T The data type to generate.
     * @param nums The variable to fill with a random value.
     */
    template <typename T>
    void getNext(T &nums) {
        cprg->getNext(nums);
    }

    /**
     * Generate random values for a vector.
     * @tparam T The data type for vector elements.
     * @param nums The vector to fill with random values.
     */
    template <typename T>
    void getNext(Vector<T> &nums) {
        cprg->getNext(nums);
    }
};
}  // namespace orq::random