#pragma once

/**
 * @file seeded_prg.h
 * @brief Implements PRG via CommonPRG with random seed.
 * @date 2024-10-29
 *
 *
 */

#include <sodium.h>

#include "../../debug/debug.h"
#include "common_prg.h"
#include "prg_algorithm.h"
#include "random_generator.h"

#define SEED_NUM_BYTES 4

namespace secrecy::random {
class PseudoRandomGenerator : public RandomGenerator {
    std::unique_ptr<CommonPRG> cprg;

   public:
    PseudoRandomGenerator() : RandomGenerator() {
        std::vector<unsigned char> seed_vec(crypto_aead_aes256gcm_KEYBYTES);

        AESPRGAlgorithm::aesKeyGen(seed_vec);

        // only use the first four bytes
        seed_vec.erase(seed_vec.begin() + SEED_NUM_BYTES, seed_vec.end());

        std::cout << "Random seed: " << debug::container2str(seed_vec) << "\n";

        std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm = std::make_unique<secrecy::random::AESPRGAlgorithm>(seed_vec);
        cprg = std::make_unique<CommonPRG>(std::move(prg_algorithm), 0);
    }

    PseudoRandomGenerator(std::vector<unsigned char> _seed) : RandomGenerator() {
        std::cout << "Fixed seed: " << debug::container2str(_seed) << "\n";
        std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm = std::make_unique<secrecy::random::AESPRGAlgorithm>(seed_vec);
        cprg = std::make_unique<CommonPRG>(std::move(prg_algorithm), 0);
    }

    template <typename T>
    void getNext(T &nums) {
        cprg->getNext(nums);
    }

    template <typename T>
    void getNext(Vector<T> &nums) {
        cprg->getNext(nums);
    }
};
}  // namespace secrecy::random