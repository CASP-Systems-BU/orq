#pragma once

#include <set>

#include "../prg/common_prg.h"

namespace orq::random {

/**
 * @brief Generates zero sharings for secure computation.
 *
 * Creates sharings of zero for arithmetic and binary operations.
 */
class ZeroSharingGenerator : public CorrelationGenerator {
    // the number of parties
    int num_parties;

    // the CommonPRGManager used to access CommonPRGs to generate the randomness
    std::shared_ptr<orq::random::CommonPRGManager> commonPRGManager;

    // 2pc ZSG needs to know rank of the current party
    int rank;

    std::optional<Communicator *> comm;

    /**
     * Check if arithmetic sharing needs sign flip for 2PC.
     * @return True if Party 0 in 2PC, false otherwise.
     */
    bool arithmeticFlip() { return num_parties == 2 && rank == 0; }

    /**
     * Check if plaintext zero should be returned.
     * @return True if single party computation, false otherwise.
     */
    bool returnPlaintextZero() { return num_parties == 1; }

   public:
    /**
     * Creates a ZeroSharingGenerator object.
     * @param _num_parties The number of parties.
     * @param _commonPRGManager The CommonPRGManager used to access CommonPRGs.
     * @param _rank The rank of this party.
     * @param _comm Optional communicator.
     */
    ZeroSharingGenerator(const int _num_parties,
                         std::shared_ptr<orq::random::CommonPRGManager> _commonPRGManager,
                         int _rank = 0, std::optional<Communicator *> _comm = std::nullopt)
        : num_parties(_num_parties),
          commonPRGManager(_commonPRGManager),
          rank(_rank),
          CorrelationGenerator(_rank) {}

    /**
     * Generate the next pseudorandom arithmetic zero sharing.
     * @param num The variable to fill with a random number.
     */
    template <typename T>
    void getNextArithmetic(T &num) {
        T prev;
        T next;

        if (returnPlaintextZero()) {
            num = 0;
            return;
        }

        commonPRGManager->get(-1)->getNext(prev);
        commonPRGManager->get(+1)->getNext(next);

        if (arithmeticFlip()) {
            num = -num;
        }
    }

    /**
     * Generate the next pseudorandom binary zero sharing.
     * @param num The variable to fill with a random number.
     */
    template <typename T>
    void getNextBinary(T &num) {
        T prev;
        T next;

        if (returnPlaintextZero()) {
            num = 0;
            return;
        }

        commonPRGManager->get(-1)->getNext(prev);
        commonPRGManager->get(+1)->getNext(next);

        num = prev ^ next;
    }

    /**
     * Generate many next pseudorandom arithmetic zero
     * sharings.
     * @param nums The vector to fill with random numbers.
     */
    template <typename T>
    void getNextArithmetic(Vector<T> &nums) {
        if (returnPlaintextZero()) {
            nums.zero();
            return;
        }

        Vector<T> next(nums.size());

        // This was `prev`
        commonPRGManager->get(-1)->getNext(nums);
        commonPRGManager->get(+1)->getNext(next);

        // Temporary solution to increment nonces evenly across parties in 4PC,
        // update with a more general solution later
        if (num_parties == 4) {
            commonPRGManager->get(+2)->incrementNonce();
        }

        for (size_t i = 0; i < nums.size(); ++i) {
            nums[i] -= next[i];
        }

        if (arithmeticFlip()) {
            nums = -nums;
        };
    }

    /**
     * Generate many next pseudorandom binary zero sharings.
     * @param nums The vector to fill with random numbers.
     */
    template <typename T>
    void getNextBinary(Vector<T> &nums) {
        Vector<T> prev(nums.size());
        Vector<T> next(nums.size());

        if (returnPlaintextZero()) {
            nums = prev;  // a Vector of zeros
            return;
        }

        commonPRGManager->get(-1)->getNext(prev);
        commonPRGManager->get(+1)->getNext(next);
        for (size_t i = 0; i < nums.size(); ++i) {
            nums[i] = prev[i] ^ next[i];
        }
    }

    /**
     * Generate many next pseudorandom arithmetic zero sharings.
     * @param nums The vector of vectors to fill with random numbers.
     * @param group The group of parties generating the zero sharing.
     *
     * Since this algorithm works over a group, it natively supports 2PC,
     * and does not require correction (as above) by `arithmeticFlip`.
     */
    template <typename T>
    void groupGetNextArithmetic(std::vector<Vector<T>> &nums, std::set<int> group) {
        auto commonPRG = commonPRGManager->get(group);
        // Last vector is "dependent"
        nums[nums.size() - 1].zero();
        for (size_t i = 0; i < nums.size() - 1; i++) {
            commonPRG->getNext(nums[i]);
            nums[nums.size() - 1] -= nums[i];
        }
    }

    /**
     * Generate many next pseudorandom binary zero sharings.
     * @param nums The vector of vectors to fill with random numbers.
     * @param group The group of parties generating the zero sharing.
     */
    template <typename T>
    void groupGetNextBinary(std::vector<Vector<T>> &nums, std::set<int> group) {
        auto commonPRG = commonPRGManager->get(group);
        // Last vector is "dependent"
        nums[nums.size() - 1].zero();
        for (size_t i = 0; i < nums.size() - 1; i++) {
            commonPRG->getNext(nums[i]);
            nums[nums.size() - 1] ^= nums[i];
        }
    }
};
}  // namespace orq::random
