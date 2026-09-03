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
#include <span>
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
     * @brief Batched F_ConsistentShare for many (Pi, x) pairs.
     *
     * Public schedule: slot k is dealt by dealers[k]. The plaintext values[k] is
     * consumed only if me == dealers[k]; otherwise it is ignored (never sent).
     *
     * Round structure (2 network rounds):
     *   R1: every dealer sends share[Pg] to {Pj, Ph}  (multi-exchange)
     *   R2: Pj <-> Ph gossip share[Pg] and compare     (multi-exchange)
     *
     * PRG advance order matches the old per-call loop (lockstep over slots).
     */
    EVector consistent_input_batch(const std::vector<PartyID>& dealers, const Vector& values) {
        const size_t n = dealers.size();
        assert(values.size() == n);

        // Per-slot absolute shares; share[Pi] stays 0 by public convention.
        // IMPORTANT: Vector's copy ctor is shallow — do not use vector(n, Vector(4)).
        std::vector<Vector> shares;
        shares.reserve(n);
        for (size_t k = 0; k < n; k++) {
            shares.emplace_back(4);
        }

        // Outbound distribute buffers keyed by absolute peer.
        std::vector<std::vector<NTL::GF2E>> dist_send(4);

        // Expected receive: for slots where we are Pj/Ph, one share from Pi.
        std::vector<PartyID> dist_from(n, -1);

        for (size_t k = 0; k < n; k++) {
            PartyID Pi = dealers[k];
            PartyID Pj = (Pi + 1) % 4;
            PartyID Pg = (Pi + 2) % 4;
            PartyID Ph = (Pi + 3) % 4;

            // Same PRG calls / groups as consistent_input_from — all parties
            // advance in public slot order (roles decide who calls which PRG).
            if (me != Pj) {
                this->randomnessManager->commonPRGManager->get({Pi, Pg, Ph})
                    ->getNext(shares[k][Pj]);
            }
            if (me != Ph) {
                this->randomnessManager->commonPRGManager->get({Pi, Pj, Pg})
                    ->getNext(shares[k][Ph]);
            }

            if (me == Pi) {
                shares[k][Pg] = values[k] - shares[k][Pi] - shares[k][Pj] - shares[k][Ph];
                dist_send[Pj].push_back(shares[k][Pg]);
                dist_send[Ph].push_back(shares[k][Pg]);
            } else if (me == Pj || me == Ph) {
                dist_from[k] = Pi;
            }
        }

        // ---- R1: distribute Pi -> {Pj, Ph} via one multi-exchange ----
        // Lengths are a function of the public dealer schedule only.
        std::vector<orq::Vector<uint8_t>> send_bufs, recv_bufs;
        std::vector<PartyID> to_ids, from_ids;
        std::vector<PartyID> peer_order;

        for (PartyID p = 0; p < 4; p++) {
            if (p == me) {
                continue;
            }
            // Always include all 3 peers so the round shape is identical for
            // every party (empty payload if this edge is unused). Empty ≠ trusted
            // zero share: we never read recv data on unused edges.
            peer_order.push_back(p);
            to_ids.push_back(abs2rel(p));
            from_ids.push_back(abs2rel(p));

            // Recv element count must match peer's send count — publicly computable:
            size_t expect = 0;
            for (size_t k = 0; k < n; k++) {
                PartyID Pi = dealers[k];
                if (Pi != p) {
                    continue;
                }
                PartyID Pj = (Pi + 1) % 4;
                PartyID Ph = (Pi + 3) % 4;
                if (me == Pj || me == Ph) {
                    expect++;
                }
            }

            // MPI exchangeShares requires equal send/recv sizes on each edge.
            // Pad both directions to max(...); only the real prefix is used.
            const size_t nbytes = math::gf2e_num_bytes();
            const size_t send_elms = dist_send[p].size();
            const size_t edge_elms = std::max(send_elms, expect);

            orq::Vector<uint8_t> send_buf(edge_elms * nbytes);
            if (send_elms > 0) {
                auto packed = serializeGF2E(std::span<const NTL::GF2E>(dist_send[p]));
                std::copy(packed.begin(), packed.end(), send_buf.begin());
            }
            send_bufs.push_back(std::move(send_buf));
            recv_bufs.push_back(orq::Vector<uint8_t>(edge_elms * nbytes));
        }

        this->communicator->exchangeShares(send_bufs, recv_bufs, to_ids, from_ids);

        // Unpack only on edges where dist_from[k] == peer (designated Pi).
        // Recv buffers may be padded; only the real prefix (expect elms) is valid.
        for (size_t i = 0; i < peer_order.size(); i++) {
            PartyID p = peer_order[i];

            size_t expect = 0;
            for (size_t k = 0; k < n; k++) {
                if (dist_from[k] == p) {
                    expect++;
                }
            }
            if (expect == 0) {
                continue;
            }

            Vector recv = deserializeGF2E(recv_bufs[i].data(), expect);
            size_t t = 0;
            for (size_t k = 0; k < n; k++) {
                if (dist_from[k] != p) {
                    continue;
                }
                PartyID Pg = (dealers[k] + 2) % 4;
                shares[k][Pg] = recv[t++];
            }
            assert(t == recv.size());
        }

        // ---- R2: Pj <-> Ph gossip + equality check ----
        std::vector<std::vector<NTL::GF2E>> gossip_send(4);
        std::vector<PartyID> gossip_peer(n, -1);

        for (size_t k = 0; k < n; k++) {
            PartyID Pi = dealers[k];
            PartyID Pj = (Pi + 1) % 4;
            PartyID Pg = (Pi + 2) % 4;
            PartyID Ph = (Pi + 3) % 4;
            if (me == Pj || me == Ph) {
                PartyID other = (me == Pj) ? Ph : Pj;
                gossip_peer[k] = other;
                gossip_send[other].push_back(shares[k][Pg]);
            }
        }

        send_bufs.clear();
        recv_bufs.clear();
        to_ids.clear();
        from_ids.clear();
        peer_order.clear();

        for (PartyID p = 0; p < 4; p++) {
            if (p == me) {
                continue;
            }
            peer_order.push_back(p);
            to_ids.push_back(abs2rel(p));
            from_ids.push_back(abs2rel(p));

            send_bufs.push_back(serializeGF2E(std::span<const NTL::GF2E>(gossip_send[p])));
            // Symmetric: peer sends us one element per slot where we gossip with them.
            recv_bufs.push_back(orq::Vector<uint8_t>(send_bufs.back().size()));
        }

        this->communicator->exchangeShares(send_bufs, recv_bufs, to_ids, from_ids);

        for (size_t i = 0; i < peer_order.size(); i++) {
            PartyID p = peer_order[i];
            if (recv_bufs[i].size() == 0) {
                continue;
            }
            size_t elms = recv_bufs[i].size() / math::gf2e_num_bytes();
            Vector check = deserializeGF2E(recv_bufs[i].data(), elms);
            size_t t = 0;
            for (size_t k = 0; k < n; k++) {
                if (gossip_peer[k] != p) {
                    continue;
                }
                PartyID Pg = (dealers[k] + 2) % 4;
                if (check[t++] != shares[k][Pg]) {
                    std::cout << "P" << me << ": Inconsistent share with P" << p << " via sender P"
                              << dealers[k] << "\n";
                    malicious_check_ok.clear();
                }
            }
            assert(t == check.size());
        }

        // Build EVector result: my three held shares per slot.
        Vector s0(n), s1(n), s2(n);
        for (size_t k = 0; k < n; k++) {
            s0[k] = shares[k][(me + 1) % 4];
            s1[k] = shares[k][(me + 2) % 4];
            s2[k] = shares[k][(me + 3) % 4];
        }
        return EVector({s0, s1, s2});
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

        // Public canonical PA order (same as before).
        std::vector<PartyAssignment> pas;
        pas.reserve(hash_size);
        for (int i = 0; i < 4; i++) {
            for (int j = i + 1; j < 4; j++) {
                for (int r = 0; r < 4; r++) {
                    if (i == r || j == r) {
                        continue;
                    }
                    pas.push_back(_jmp_assignments(i, j, r));
                }
            }
        }
        assert(pas.size() == hash_size);

        // Slot layout (public), interleaved like the original loop:
        // for each PA k: dealers[2k]=receiver, dealers[2k+1]=hasher.
        // values[i] is only read when me == dealers[i].
        std::vector<PartyID> dealers(2 * hash_size);
        Vector values(2 * hash_size);
        for (size_t k = 0; k < hash_size; k++) {
            dealers[2 * k] = pas[k].receiver;
            dealers[2 * k + 1] = pas[k].hasher;

            auto it = aggregate_hashes_by_pa.find(pas[k]);
            const bool have = (it != aggregate_hashes_by_pa.end());

            // Only the designated dealer may supply a digest for this PA.
            // Missing digest while being dealer => check fails (do not treat as valid share of 0).
            if (me == pas[k].receiver) {
                if (!have) {
                    malicious_check_ok.clear();
                } else {
                    values[2 * k] = it->second;
                }
            }
            if (me == pas[k].hasher) {
                if (!have) {
                    malicious_check_ok.clear();
                } else {
                    values[2 * k + 1] = it->second;
                }
            }
            // else: values left default; ignored because me != dealers[i]
        }

        // 2 rounds total (was ~48 sequential send/recv pairs)
        EVector shared = consistent_input_batch(dealers, values);

        // hashes[k] = share(d^H) + share(d^R) for PA k (char-2 field: equal digests => 0)
        for (size_t k = 0; k < hash_size; k++) {
            hashes.slice(k, k + 1) =
                shared.slice(2 * k + 1, 2 * k + 2) + shared.slice(2 * k, 2 * k + 1);
        }
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
