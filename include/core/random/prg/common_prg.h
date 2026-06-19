#pragma once

#include <math.h>
#include <sodium.h>
#include <stdlib.h>

#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>

#include "../correlation/correlation_generator.h"
#include "core/communication/communicator.h"
#include "core/math/util.h"
#include "prg_algorithm.h"

#pragma GCC diagnostic push
// Prevent warning about `num <<= 8` below, when instantiated with 8-bit types
#pragma GCC diagnostic ignored "-Wshift-count-overflow"

namespace orq::random {

class CommonPRG {
    std::unique_ptr<DeterministicPRGAlgorithm> prg_algorithm;

   public:
    /**
     * Default constructor with rank -1.
     */
    CommonPRG() : CommonPRG(-1) {}

    /**
     * Constructor that generates a random seed.
     * @param rank The rank of this party.
     */
    CommonPRG(int rank) {
        std::vector<unsigned char> seed(crypto_aead_aes256gcm_KEYBYTES);
        AESPRGAlgorithm::aesKeyGen(seed);
        prg_algorithm = std::make_unique<AESPRGAlgorithm>(seed);
    }

    /**
     * Constructor with provided PRG algorithm.
     * @param _prg_algorithm The PRG algorithm to use.
     * @param rank The rank of this party.
     * @param _comm Optional communicator.
     */
    CommonPRG(std::unique_ptr<DeterministicPRGAlgorithm> _prg_algorithm, int rank,
              std::optional<Communicator*> _comm = std::nullopt)
        : prg_algorithm(std::move(_prg_algorithm)) {}

    /**
     * Generates random bytes to fill one T.
     * @param num The reference to fill with random bytes.
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    void getNext(T& num) {
        prg_algorithm->getNext(num);
    }

    template <std::ranges::contiguous_range R>
    void getNext(R& range) {
        prg_algorithm->getNext(std::span(range));
    }

    /**
     * Generate many next elements from the PRF.
     * @param nums The Vector to fill with pseudorandom numbers.
     */
    template <typename T>
    void getNext(Vector<T>& nums) {
        prg_algorithm->getNext(nums.span());
    }

    /**
     * Generate many next elements from the PRF.
     * @param nums The span to fill with pseudorandom numbers.
     */
    template <typename T>
    void getNext(std::span<T> nums) {
        prg_algorithm->getNext(nums);
    }

    /**
     * @brief Generate many random GF2E elements using the CommonPRG. Use NTL's bytes-to-GF2X
     * conversion followed by a modular reduction. This is only secure as long as the polynomial
     * modulus has degree 8k for some integer k (thus fitting cleanly into a span of random bytes).
     *
     * @tparam
     * @param nums
     */
    void getNext(Vector<NTL::GF2E>& nums) {
        int poly_bytes = math::gf2e_num_bytes();

        // Get random bytes
        Vector<uint8_t> buf(nums.size() * poly_bytes);
        getNext(buf);

        for (int i = 0; i < nums.size(); i++) {
            // Convert to random polynomial
            auto poly = NTL::GF2XFromBytes(&buf[i], poly_bytes);
            // Mod reduce + convert to GF2E.
            nums[i] = NTL::to_GF2E(poly);
        }
    }

    void getNext(NTL::GF2E& num) {
        int poly_bytes = math::gf2e_num_bytes();
        Vector<uint8_t> buf(poly_bytes);
        getNext(buf);

        auto poly = NTL::GF2XFromBytes(buf.data(), poly_bytes);
        num = NTL::to_GF2E(poly);
    }

    /**
     * Increment nonce if required.
     */
    void incrementNonce() { prg_algorithm->incrementNonce(); }

};  // class CommonPRG

/**
 * @brief Manages CommonPRG objects for multiple parties and groups.
 *
 * Maps party-relative requests and explicit groups to their corresponding CommonPRG instances.
 */
class CommonPRGManager {
   public:
    enum class RelativeRankMode {
        INCLUDED,
        EXCLUDED,
    };

   private:
    const int num_parties;
    const int party_id;
    RelativeRankMode relative_rank_mode;

    // Local PRG for this party only.
    std::shared_ptr<CommonPRG> local_prg;

    // map of groups to CommonPRG pointers
    std::map<std::set<int>, std::shared_ptr<CommonPRG>> common_prg_group_map;

    std::set<int> everyone;

    int getAbsoluteRank(int relative_rank) const {
        int index = (party_id + relative_rank) % num_parties;
        if (index < 0) {
            index += num_parties;
        }
        return index;
    }

    std::set<int> buildGroupFromRelativeRank(int relative_rank) const {
        const int target_party = getAbsoluteRank(relative_rank);

        if (relative_rank_mode == RelativeRankMode::INCLUDED) {
            return {party_id, target_party};
        }

        std::set<int> group;
        for (int p = 0; p < num_parties; ++p) {
            if (p != target_party) {
                group.insert(p);
            }
        }
        return group;
    }

   public:
    static constexpr struct AllSemaphore {
    } ALL{};

    /**
     * @brief Initializes the CommonPRGManager object with the current party index.
     * @param _num_parties The number of parties.
     * @param _party_id The absolute ID of the current party.
     * @param _relative_rank_mode How relative ranks map to groups.
     */
    CommonPRGManager(int _num_parties, int _party_id = 0,
                     RelativeRankMode _relative_rank_mode = RelativeRankMode::INCLUDED)
        : num_parties(_num_parties), party_id(_party_id), relative_rank_mode(_relative_rank_mode) {
        local_prg = std::make_shared<CommonPRG>(CommonPRG());
        common_prg_group_map[{party_id}] = local_prg;
        common_prg_group_map[{}] = local_prg;

        for (int i = 0; i < num_parties; i++) {
            everyone.insert(i);
        }
    }

    /**
     * @brief Add a CommonPRG object by relative rank behavior.
     *
     * INCLUDED: group is {party_id, target_party}.
     * EXCLUDED: group is all parties except target_party.
     */
    void add(std::shared_ptr<CommonPRG> common_prg, int relative_rank) {
        auto group = buildGroupFromRelativeRank(relative_rank);
        add(common_prg, group);
    }

    /**
     * @brief Add a CommonPRG object by explicit group.
     */
    void add(std::shared_ptr<CommonPRG> common_prg, std::set<int> group) {
        if (common_prg_group_map.contains(group)) {
            std::cout << "WARNING: already have CommonPRG " << debug::container2str(group) << "\n";
        }

        common_prg_group_map[group] = common_prg;
    }

    /**
     * @brief Get a CommonPRG object by relative rank behavior.
     *
     * INCLUDED: group is {party_id, target_party}.
     * EXCLUDED: group is all parties except target_party.
     */
    std::shared_ptr<CommonPRG> get(int relative_rank) {
        // special overload
        if (relative_rank == 0) {
            return local_prg;
        }

        auto group = buildGroupFromRelativeRank(relative_rank);
        return get(group);
    }

    /**
     * @brief Get the CommonPRG object shared with an explicit group.
     */
    std::shared_ptr<CommonPRG> get(std::set<int> group) {
        if (group.empty()) {
            return local_prg;
        }

        auto itr = common_prg_group_map.find(group);
        if (itr == common_prg_group_map.end()) {
            throw std::runtime_error(
                "CommonPRG group not found for group: " + debug::container2str(group) + " @ P" +
                std::to_string(party_id));
        }
        return itr->second;
    }

    /**
     * @brief Get the everyone group.
     *
     * @return std::shared_ptr<CommonPRG>
     */
    std::shared_ptr<CommonPRG> get() { return common_prg_group_map.find(everyone)->second; }

    /**
     * @brief Print all group keys currently present in the manager map (for debugging)
     */
    void printGroupKeys() const {
        std::cout << "CommonPRGManager groups (" << common_prg_group_map.size()
                  << "):" << std::endl;
        for (const auto& [group, _] : common_prg_group_map) {
            std::cout << "  " << debug::container2str(group) << std::endl;
        }
    }

};  // class CommonPRGManager
}  // namespace orq::random

#pragma GCC diagnostic pop
