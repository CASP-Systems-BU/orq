#ifndef SECRECY_COMMON_PRG_H
#define SECRECY_COMMON_PRG_H

#include <math.h>
#include <sodium.h>
#include <stdlib.h>

#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include "correlation_generator.h"
#include "prg_algorithm.h"

#pragma GCC diagnostic push
// Prevent warning about `num <<= 8` below, when instantiated with 8-bit types
#pragma GCC diagnostic ignored "-Wshift-count-overflow"

namespace secrecy::random {

class CommonPRG : public CorrelationGenerator {
    std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm;

public:
    CommonPRG() : CommonPRG(-1) {}

    CommonPRG(int rank) : CorrelationGenerator(rank) {
        std::vector<unsigned char> seed(crypto_aead_aes256gcm_KEYBYTES);
        AESPRGAlgorithm::aesKeyGen(seed);
        prg_algorithm = std::make_unique<AESPRGAlgorithm>(seed);
    }

    CommonPRG(
        std::unique_ptr<DeterministicPRGAlgorithm> _prg_algorithm,
        int rank,
        std::optional<Communicator*> _comm = std::nullopt)
        : prg_algorithm(std::move(_prg_algorithm)), CorrelationGenerator(rank) {}

    /**
     * getNext - Generates random bytes to fill one T.
     * @param num The reference to fill with random bytes.
     */
    template <typename T>
    void getNext(T& num) {
        prg_algorithm->getNext(num);
    }

    /**
     * getNext (vector version) - Generate many next elements from the PRF.
     * @param nums - The vector to fill with pseudorandom numbers.
     */
    template <typename T>
    void getNext(Vector<T>& nums) {
        prg_algorithm->getNext(nums);
    }

    /**
     * incrementNonce - Increment nonce if required.
     */
    void incrementNonce() {
        prg_algorithm->incrementNonce();
    }

};  // class CommonPRG

// acts as a mapping from relative party rank r to the CommonPRG object
// that the current party shares with party r
class CommonPRGManager {
    int num_parties;

    // the vector of CommonPRG pointers
    std::vector<std::shared_ptr<CommonPRG>> common_prgs;

    // map of groups to CommonPRG pointers
    std::map<std::set<int>, std::shared_ptr<CommonPRG>> common_prg_group_map;

   public:
    /**
     * CommonPRGManager - Initializes the CommonPRGManager object with the current party index.
     * @param _num_parties - The number of parties.
     * @param _rank - The absolute index of the party in the parties ring.
     */
    CommonPRGManager(int _num_parties) : num_parties(_num_parties) {
        // initialize the vector of CommonPRG
        common_prgs.resize(num_parties);
        // CommonPRG with oneself is undefined
        common_prgs[0] = NULL;
    }

    /**
     * add - Add a CommonPRG object to the manager. The manager handles the index mapping.
     * @param common_prg - A pointer to the CommonPRG to be added to the manager.
     * @param relative_rank - The relative rank of the party the prg is shared with.
     */
    void add(std::shared_ptr<CommonPRG>& common_prg, int relative_rank) {
        // add num_parties to make sure the result is positive
        int index = (num_parties + relative_rank) % num_parties;
        common_prgs[index] = common_prg;
    }

    /**
     * add - Add a CommonPRG object to the manager by group. The manager handles the group mapping.
     * @param common_prg - A pointer to the CommonPRG to be added to the manager.
     * @param group - The group that shares the CommonPRG.
     */
    void add(std::shared_ptr<CommonPRG>& common_prg, std::set<int> group) {
        common_prg_group_map.insert({group, common_prg});
    }

    /**
     * get - Get the CommonPRG object shared with the party given by relative rank.
     * @param relative_rank - The rank of the other party relative to the current party.
     * @return - A pointer to the CommonPRG shared with the other party.
     */
    std::shared_ptr<CommonPRG> get(int relative_rank) {
        // add num_parties to make sure the result is positive
        int index = (num_parties + relative_rank) % num_parties;
        return common_prgs[index];
    }

    /**
     * get - Get the CommonPRG object shared with the group.
     * @param group - The group that shares the CommonPRG.
     * @return - A pointer to the CommonPRG shared with the other party.
     */
    std::shared_ptr<CommonPRG> get(std::set<int> group) {
        auto itr = common_prg_group_map.find(group);
        if (itr == common_prg_group_map.end()) {
            // error, group not found
            throw std::runtime_error("CommonPRG group not found for group: " +
                                     debug::container2str(group));
            return nullptr;
        } else {  // found
            return itr->second;
        }
    }

};  // class CommonPRGManager
}  // namespace secrecy::random

#pragma GCC diagnostic pop

#endif