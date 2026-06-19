#pragma once

#include <NTL/GF2E.h>
#include <NTL/GF2X.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <typeindex>
#include <vector>

#include "core/protocols/dalskov_4pc.h"

namespace orq::debug {

/**
 * @brief Helper function to print GF2E elements in base64
 *
 * @param os
 * @param a
 */
inline void print_gf2e_base64(std::ostream& os, const NTL::GF2E& a) {
    NTL::GF2X poly = rep(a);
    long num_bits = NTL::GF2E::degree();
    long num_bytes = (num_bits + 7) / 8;

    // Extract bytes from polynomial
    std::vector<unsigned char> bytes(num_bytes, 0);
    for (long byte_idx = 0; byte_idx < num_bytes; byte_idx++) {
        unsigned char byte_val = 0;
        for (long bit = 7; bit >= 0; bit--) {
            long coeff_idx = byte_idx * 8 + bit;
            if (coeff_idx < num_bits && coeff_idx <= deg(poly)) {
                byte_val = (byte_val << 1) | (IsOne(coeff(poly, coeff_idx)) ? 1 : 0);
            } else {
                byte_val <<= 1;
            }
        }
        bytes[byte_idx] = byte_val;
    }

    // Encode to base64 using libsodium
    char base64_buf[sodium_base64_ENCODED_LEN(num_bytes, sodium_base64_VARIANT_ORIGINAL)];
    sodium_bin2base64(base64_buf, sizeof(base64_buf), bytes.data(), num_bytes,
                      sodium_base64_VARIANT_ORIGINAL);
    os << base64_buf;
}

/**
 * @brief Helper function to print GF2E vectors in base64
 *
 * @param vec
 * @param partyID
 */
void print_vec_gf2e(orq::Vector<NTL::GF2E>& vec, const int partyID) {
    std::cout << std::setfill(' ');
    if (partyID == 0) {
        for (size_t i = 0; i < vec.size(); ++i) {
            std::cout << std::right << " " << std::setw(VECTOR_SPACING);
            print_gf2e_base64(std::cout, vec[i]);  // Use base64 printer
        }
        std::cout << std::endl;
    }
}
}  // namespace orq::debug

namespace orq {

using VerifierBase = Fantastic_4PC<NTL::GF2E, std::vector<NTL::GF2E>, orq::Vector<NTL::GF2E>,
                                   orq::EVector<NTL::GF2E, 3>, true>;

/**
 * @brief A subclass of the Fantastic_4PC protocol instantiate over the field \f$2^{256}\f$. Used
 * for verifying hash correctness in the fix of [BS26]
 *
 */
class Verifier_4PC : public VerifierBase {
   private:
    struct HashBucketKey {
        PartyAssignment pa;
        std::type_index type_id;

        auto operator<=>(const HashBucketKey&) const = default;
    };

    PartyID me;

    using HashStatePtr = std::unique_ptr<Hash>;
    // We'll use a map of {PartyAssignment, T} -> *Hash -- one per thread
    using ThreadHashStateMap = std::map<HashBucketKey, HashStatePtr>;

    using EVector = VerifierBase::EVector_t;
    using Vector = VerifierBase::Vector_t;

    // Synchronization for finalization: only one thread runs it, others wait
    std::atomic_flag finalize_in_progress;
    // Raised when complete
    std::atomic_flag finalize_complete;
    // Result of the malicious check (cleared if check failed)
    std::atomic_flag malicious_check_ok;

    // Each worker thread writes only to its own index. This avoids needing any synchronization
    std::vector<ThreadHashStateMap> hash_states_by_thread;

    const int num_threads;

    /**
     * @brief Party Pi shares GF2E element x with two other parties, who then gossip to confirm
     * consistency. Fourth party only uses a common PRG to set its shares, and does not communicate.
     * Share xi (not held by the sending party) is kept at zero.
     *
     * @param Pi
     * @param x
     * @return EVector
     */
    EVector consistent_input_from(PartyID Pi, NTL::GF2E& x) {
        Vector shares(4);

        // arbitrary
        PartyID Pj = (Pi + 1) % 4;
        PartyID Pg = (Pi + 2) % 4;
        PartyID Ph = (Pi + 3) % 4;

        // Let share Pi be zero: everyone knows it, so no use generating.

        if (me != Pj) {
            this->randomnessManager->commonPRGManager->get({Pi, Pg, Ph})->getNext(shares[Pj]);
        }

        if (me != Ph) {
            this->randomnessManager->commonPRGManager->get({Pi, Pj, Pg})->getNext(shares[Ph]);
        }

        if (me == Pi) {
            shares[Pg] = x - shares[Pi] - shares[Pj] - shares[Ph];
            // These could happen in parallel... maybe using multi-send?

            this->communicator->sendShare(shares[Pg], abs2rel(Pj));
            this->communicator->sendShare(shares[Pg], abs2rel(Ph));
        } else if (me == Pj || me == Ph) {
            // Pj or Ph receive x_g from Pi
            this->communicator->receiveShare(shares[Pg], abs2rel(Pi));

            PartyID other = (me == Pj) ? Ph : Pj;

            this->communicator->sendShare(shares[Pg], abs2rel(other));

            NTL::GF2E check;
            this->communicator->receiveShare(check, abs2rel(other));

            if (check != shares[Pg]) {
                std::cout << "P" << me << ": Inconsistent share with P" << other << " via sender P"
                          << Pi << "\n";
                malicious_check_ok.clear();
            }
        }

        return {shares.singleton((me + 1) % 4), shares.singleton((me + 2) % 4),
                shares.singleton((me + 3) % 4)};
    }

    /**
     * @brief Run the actual finalize operation / malicious check. Gated by atomics to prevent
     * multiple threads from executing.
     *
     */
    void run_finalize_once() {
        // Reduce [thread_id -> [(pa, T) -> hash]] into [pa -> hash]
        std::map<PartyAssignment, std::unique_ptr<Hash>> aggregate;

        for (auto& thread_map : hash_states_by_thread) {
            for (auto& [key, state_ptr] : thread_map) {
                if (!state_ptr) {
                    continue;
                }

                auto& aggregate_hash = aggregate[key.pa];
                // If map not populated, do so now
                if (!aggregate_hash) {
                    aggregate_hash = std::make_unique<Hash>();
                }

                // Finalize in place: thread-local state is one-shot per check.
                auto thread_digest = state_ptr->finalize();
                state_ptr = std::make_unique<Hash>();

                aggregate_hash->update(thread_digest.span());
            }
        }

        // Final aggregate hash per `pa`, represented as a single GF2E element.
        std::map<PartyAssignment, NTL::GF2E> aggregate_hashes_by_pa;
        for (auto& [pa, aggregate_state] : aggregate) {
            auto digest = aggregate_state->finalizeArray();
            auto fieldRepr = orq::math::deserializeGF2E(digest.data());
            aggregate_hashes_by_pa.emplace(pa, fieldRepr);

            // reset the state to a new hash
            aggregate_state = std::make_unique<Hash>();
        }

        // consistent secret sharing + only share my hashes

        size_t hash_size = 12;

        // Create a length-12 Vector of hashes in canonical order (4 parties * 3 roles)
        EVector hashes(hash_size);

        auto idx = -1;
        for (int i = 0; i < 4; i++) {
            for (int j = i + 1; j < 4; j++) {
                for (int r = 0; r < 4; r++) {
                    if (i == r || j == r) {
                        continue;
                    }

                    idx++;

                    auto pa = _jmp_assignments(i, j, r);
                    auto hash = aggregate_hashes_by_pa[pa];

                    // These are each 2 rounds
                    // TODO: consolidate into one function call?
                    // Maybe add hashing, then check after the loop
                    // There really could just be a SINGLE check outside this loop.
                    auto sr = consistent_input_from(pa.receiver, hash);
                    auto sh = consistent_input_from(pa.hasher, hash);

                    // Insert both into the hashes vec
                    hashes.slice(idx, idx + 1) = sh + sr;
                }
            }
        }
        // 12 * 4 = 48 rounds overall...
        // Could be two rounds (send, confirm), or one if we combine with next

        // Can do a single unified hash check at the end (per share)
        if (this->randomnessManager->csq->commitmentsAvailable() < 16) {
            // TODO: this may not be synchronized!
            std::cout << "Verifier: Repopulate CSQ\n";
            this->randomnessManager->csq->repopulateQueue();
        }

        EVector r(hash_size), z(1);

        for (auto g : this->getGroups()) {
            // Generate random vector (or maybe zero, if I'm not in the group)
            Vector rand_share(hash_size);

            // Everyone needs to call this, so queues are updated
            // 1 round each for checking commitment
            auto gen = this->randomnessManager->csq->nextPRGforGroup(g);

            if (g.contains(me)) {
                gen->getNext(rand_share);
            }

            // add it to the running random value
            add_a(public_share(rand_share, g), r, r);
        }
        // 4 groups -> 4 rounds
        // Could be 1 round

        // random linear combination z = <hashes, r>
        // 6 rounds
        dot_product_a(hashes, r, z);
        // Could be 1 round

        // this needs to be a checked open. uses _jmp double-send overloads below
        // 4 rounds
        auto ret = internal_open_a(z);
        // Could be 1 round

        if (!NTL::IsZero(ret[0])) {
            // nonzero dot product! check failed
#ifndef MAL_TEST_MODE
            std::cout << "P" << me << ": Non-zero check in verifier.\n";
#endif
            malicious_check_ok.clear();
        }

        // TODO: Alternatively, could use count_nonzero(), but that exists at the SharedVector
        // level. Something like PR #920 could help. e.g.
        //   if (hashes.count_nonzero()->open()[0] > 0) ...
        // This is what HP-MPC has implemented, I think. Counterintuitively, it may have better
        // round complexity!
    }

    /**
     * @brief Double-send implementation of jmp (receive side)
     *
     * @param x
     * @param Pi
     * @param Pj
     * @param Pr
     */
    void _jmp_recv(orq::Vector<NTL::GF2E>& x, int Pi, int Pj, int Pr,
                   JmpBehavior behavior = JmpBehavior::Unbatched) override {
        // assert(behavior == JmpBehavior::Unbatched);
        assert(me == Pr);

        auto pa = _jmp_assignments(Pi, Pj, Pr);
        orq::Vector<NTL::GF2E> x2(x.size());

        this->communicator->receiveShares(x, abs2rel(pa.sender));
        this->communicator->receiveShares(x2, abs2rel(pa.hasher));

        assert(x.same_as(x2));
    }

    /**
     * @brief Double-send implementation of jmp (send side)
     *
     * @param x
     * @param Pi
     * @param Pj
     * @param Pr
     */
    void _jmp_send(const orq::Vector<NTL::GF2E>& x, int Pi, int Pj, int Pr,
                   JmpBehavior behavior = JmpBehavior::Unbatched) override {
        // assert(behavior == JmpBehavior::Unbatched);
        auto pa = _jmp_assignments(Pi, Pj, Pr);

        if (me == pa.sender || me == pa.hasher) {
            this->communicator->sendShares(x, abs2rel(Pr));
        }
    }

    /**
     * @brief Start a new check. Returns true to indicate readiness.
     * Actual finalization happens lazily in finalize_malicious_check_internal().
     *
     * @return true
     */
    bool start_malicious_check_internal() override {
        // Reset synchronization flags for next malicious_check() call
        finalize_in_progress.clear();
        finalize_complete.clear();
        return true;
    }

    /**
     * @brief Finalize check. Only executes the internal function once per malicious_check() call.
     * First thread (/protocol type instance) to arrive runs finalization; other threads wait for
     * completion. All threads then return the same result.
     *
     * @return true malicious check passed
     * @return false malicious behavior was detected (aborts unless in test mode)
     */
    bool finalize_malicious_check_internal() override {
        if constexpr (SKIP_MALICIOUS_CHECK_FLAG) {
            return true;
        }

        if (!finalize_in_progress.test_and_set()) {
            // First worker runs finalize
            run_finalize_once();
            finalize_complete.test_and_set();
            finalize_complete.notify_all();
        } else {
            // Another thread is running finalization - wait for it to complete
            finalize_complete.wait(false);
        }

        return malicious_check_ok.test();
    }

   public:
    /**
     * @brief Constructor for Verifier object
     *
     * @param partyID
     * @param wc
     * @param communicator
     * @param randomnessManager
     */
    Verifier_4PC(PartyID partyID, WorkerConfig wc, Communicator* communicator,
                 random::RandomnessManager* randomnessManager)
        : VerifierBase(partyID, {.num_workers = 1, .worker_id = 0}, communicator,
                       randomnessManager),
          hash_states_by_thread(wc.num_workers),
          num_threads(wc.num_workers),
          me(this->partyID) {
        malicious_check_ok.test_and_set();

        if constexpr (SKIP_MALICIOUS_CHECK_FLAG) {
            if (partyID == 0) {
                std::cout << "WARNING: skipping malicious check!\n";
            }
        }
    }

    Verifier_4PC(const Verifier_4PC&) = delete;
    Verifier_4PC& operator=(const Verifier_4PC&) = delete;

    /**
     * @brief Accept a type-T hash from the specified thread, with party assignment pa. Add it to
     * the appropriate hash bucket in a thread-safe manner.
     *
     * @tparam T
     * @param thread_id
     * @param pa
     * @param my_hash
     */
    template <typename T>
    void aggregate_hash(int thread_id, PartyAssignment pa, orq::Vector<uint8_t> my_hash) {
        assert(thread_id >= 0 && thread_id < num_threads);

        const HashBucketKey key{pa, std::type_index(typeid(T))};

        // Each thread indexes into the vector
        auto& thread_map = hash_states_by_thread[thread_id];
        // Get this hash state
        auto& state = thread_map[key];
        if (!state) {
            state = std::make_unique<Hash>();
        }

        assert(thread_map.contains(key));

        // and update it
        state->update(my_hash.span());
    }

    void reset_malicious_state() override {
        start_malicious_check_internal();
        malicious_check_ok.test_and_set();

        for (auto& m : hash_states_by_thread) {
            m.clear();
        }
    }

    /**
     * @brief Overload reshare to prevent calls to shuffle from within this protocol. Since we use
     * the double-send malicious check for the verifier protocol, shuffle is not malicious secure.
     * However, we should never need reshare inside the verifier protocol.
     *
     * @param v
     * @param group
     * @param binary
     */
    void reshare(orq::EVector<NTL::GF2E, 3>& v, const std::set<int> group,
                 const bool binary) override {
        throw std::logic_error("reshare not allowed inside verification protocol");
    }
};

namespace detail {
    /**
     * @brief Get the singleton static verifier instance. There is a _single_ verifier instance for
     * each execution, shared by all threads and protocol type instances.
     *
     * @param partyID
     * @param wc
     * @param communicator
     * @param randomnessManager
     * @return std::shared_ptr<Verifier_4PC>
     */
    inline std::shared_ptr<Verifier_4PC> getVerifierInstance(
        PartyID partyID, WorkerConfig wc, Communicator* communicator,
        random::RandomnessManager* randomnessManager) {
        static std::shared_ptr<Verifier_4PC> instance =
            std::make_shared<Verifier_4PC>(partyID, wc, communicator, randomnessManager);
        return instance;
    }
}  // namespace detail

}  // namespace orq
