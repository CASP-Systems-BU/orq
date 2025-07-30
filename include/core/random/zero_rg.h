#ifndef SECRECY_ZERO_RANDOM_GENERATOR_H
#define SECRECY_ZERO_RANDOM_GENERATOR_H

namespace secrecy::random {
class ZeroRandomGenerator : public CommonPRG {
   public:
    ZeroRandomGenerator(unsigned short _seed = 0, int rank = 0) : CommonPRG({}, 0) {}

    ZeroRandomGenerator(std::vector<unsigned char> _seed, int rank = 0) : CommonPRG({}, rank) {}

    template <typename T>
    void getNext(T &num) {}

    template <template <typename...> class V, typename T>
    void getNext(V<T> &nums) {}
};
}  // namespace secrecy::random

#endif  // SECRECY_ZERO_RANDOM_GENERATOR_H
