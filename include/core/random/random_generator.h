#ifndef SECRECY_RANDOM_GENERATOR_H
#define SECRECY_RANDOM_GENERATOR_H

#include "../containers/e_vector.h"
#include "../../debug/debug.h"

namespace secrecy::random {

    class RandomGenerator {

        // the seed randomness
        // depending on implementation of generator, could also be a key (eg to a PRF)
        unsigned short seed;

        // NOTE: inheriting classes should implement:
        //  - Functions to add/edit different PRG queues.

        // helper function to generate a secure seed
        static unsigned short generate_seed() {
            // TODO: implement this
            return 0;
        }

    public:
        /**
         * RandomGenerator - Initializes the RandomGenerator base.
         * @param _seed (optional) - The seed randomness. If not supplied, generate it.
         */
        RandomGenerator() : RandomGenerator(generate_seed()) {}
        RandomGenerator(unsigned short _seed) : seed(_seed) {}

        virtual ~RandomGenerator() {}

        /**
         * getNext - Generate the next element of some Pseudo Random Numbers Queue.
         * @param num - The variable to fill with a random number.
         */
        template <typename T>
        void getNext(T &num) {
            
        }

        /**
         * getNext (vector version) - Generate many next elements of some Pseudo Random Numbers Queue.
         * @param nums - The vector to fill with random numbers.
         */
        template <typename T>
        void getNext(Vector<T> &nums) {
            for (size_t i = 0; i < nums.size(); ++i) {
                getNext(nums[i]);
            }
        }

    };
} // namespace secrecy::random

#endif //SECRECY_RANDOM_GENERATOR_H