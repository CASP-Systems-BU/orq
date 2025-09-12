#pragma once

#include <sodium.h>

namespace orq {

/**
 * @brief Implementation of the "Fantastic Four" paper by Dalskov et al.
 *
 * This version implements functionalities exactly as specified in the paper,
 * without any optimizations.
 *
 * @tparam Data Plaintext data type.
 * @tparam Share Replicated share type.
 * @tparam Vector Data container type.
 * @tparam EVector Share container type.
 */
template <typename Data, typename Share, typename Vector, typename EVector>
class Fantastic_4PC : public Protocol<Data, Share, Vector, EVector> {
    std::map<std::pair<int, int>, std::unique_ptr<crypto_generichash_state>> hash_states;

    /**
     * @brief Computes the JMP receiver.
     *
     * Designed to give each party a chance to be the receiver.
     * This is required for open to be correct.
     *
     * @param i First party.
     * @param j Second party.
     * @return Party ID of the receiver.
     */
    inline int next_party(int i, int j) {
        int a = std::min(i, j);
        int b = std::max(i, j);

        if (b == (a + 1) % 4) {
            return (b + 1) % 4;
        } else {
            return (a + 1) % 4;
        }
    }

    /**
     * @brief Return the index of the missing party.
     *
     * All party indices will sum to 6 (0 + 1 + 2 + 3), so just subtract the
     * given parties to find the missing one.
     *
     * @param i First party.
     * @param j Second party.
     * @param k Third party.
     * @return Index of the missing party.
     */
    inline int excluded_party(int i, int j, int k) { return (0 + 1 + 2 + 3) - (i + j + k); }

    /**
     * @brief Convert absolute party ID to relative party ID.
     *
     * @param p Absolute party ID.
     * @return Relative party ID.
     */
    inline int abs2rel(int p) { return (p - this->partyID + 4) % 4; }

    /**
     * @brief Convert relative party ID to absolute party ID.
     *
     * @param p Relative party ID.
     * @return Absolute party ID.
     */
    inline int rel2abs(int p) { return (this->partyID + p) % 4; }

    /**
     * @brief Compute which relative share is \f$x_p\f$ (my share excluding party p).
     *
     * If self, return 0, even though that's not a valid share (this is to
     * prevent out-of-bounds array accesses on party-agnostic code).
     *
     * @param p Party ID.
     * @return Relative share index.
     */
    inline int abs2sh(int p) { return p == this->partyID ? 0 : (abs2rel(p) - 1 + 4) % 4; }

    /**
     * @brief Given two JMP senders, decide who will hash and who will send.
     *
     * Different strategies here could change performance by redistributing work.
     * This function must be commutative (who_hashes(a, b) == who_hashes(b, a)).
     *
     * @param a First party.
     * @param b Second party.
     * @return Party who should hash.
     */
    inline int who_hashes(int a, int b) {
        assert(a != b);
        return std::max(a, b);
    }

    /**
     * @brief Initialize hash state for malicious security.
     *
     * @param i First party (must be less than j).
     * @param j Second party.
     */
    void init_hash(int i, int j) {
        assert(i < j);

        crypto_generichash_init(hash_states[{i, j}].get(), NULL, 0, crypto_generichash_BYTES);

        // Seed the hash with the party ID for domain separation
        uint8_t seed = i << 4 | j;
        crypto_generichash_update(hash_states[{i, j}].get(), reinterpret_cast<u_char *>(&seed),
                                  sizeof(seed));
    }

    /**
     * @brief Internal method to open shares using JMP protocol.
     *
     * @param sh Input shared vector.
     * @return Opened plaintext vector.
     */
    Vector _open_shares(const EVector &sh) {
        size_t N = sh.size();
        Vector sh3(N);

        // Pairs of parties JMP their common share to the appropriate party
        // missing it.
        for (int Pi = 0; Pi < 4; Pi++) {
            // 0, 1;  1, 2;  2, 3;  3, 0
            int Pj = (Pi + 1) % 4;
            int Pr = next_party(Pi, Pj);

            if (this->partyID == Pr) {
                // Receive into extra vector
                _jmp_recv(sh3, Pi, Pj, Pr);
            } else if (this->partyID == Pi || this->partyID == Pj) {
                // Send missing share
                auto rel_sh = abs2sh(Pr);
                _jmp_send(sh(rel_sh), Pi, Pj, Pr);
            }
        }
        return sh3;
    }

    /**
     * @brief Generate shares for given data with specified encoding.
     *
     * @tparam E Encoding type (AShared or BShared).
     * @param data Input plaintext vector.
     * @return Vector of shared vectors for all parties.
     */
    template <typename orq::Encoding E>
    std::vector<EVector> get_shares(const Vector &data) {
        auto size = data.size();
        Vector s0(size), s1(size), s2(size);
        this->randomnessManager->localPRG->getNext(s0);
        this->randomnessManager->localPRG->getNext(s1);
        this->randomnessManager->localPRG->getNext(s2);

        Vector s3(size);
        if constexpr (E == Encoding::AShared) {
            s3 = data - s0 - s1 - s2;
        } else if constexpr (E == Encoding::BShared) {
            s3 = data ^ s0 ^ s1 ^ s2;
        }

        return {EVector({s1, s2, s3}), EVector({s2, s3, s0}), EVector({s3, s0, s1}),
                EVector({s0, s1, s2})};
    }

    /**
     * @brief Internal secret sharing method with specified encoding.
     *
     * @tparam E Encoding type (AShared or BShared).
     * @param data Input plaintext vector.
     * @param data_party Party that owns the data.
     * @return This party's shared vector.
     */
    template <orq::Encoding E>
    EVector _secret_share(const Vector &data, const PartyID &data_party) {
        auto size = data.size();
        if (this->partyID == data_party) {
            auto shares = get_shares<E>(data);

            // send to everyone else
            for (int rel = 1; rel < 4; rel++) {
                this->communicator->sendShares(shares[rel](0), rel, size);
                this->communicator->sendShares(shares[rel](1), rel, size);
                this->communicator->sendShares(shares[rel](2), rel, size);
            }

            return shares[0];
        } else {
            EVector s(size);
            int recv_from = data_party - this->partyID;
            // Receive second shared vector from the predecessor
            this->communicator->receiveShares(s(0), recv_from, size);
            this->communicator->receiveShares(s(1), recv_from, size);
            this->communicator->receiveShares(s(2), recv_from, size);
            return s;
        }
    }

    /**
     * @brief Compute JMP assignments for parties Pi and Pj.
     *
     * @param Pi First party.
     * @param Pj Second party.
     * @return Tuple containing hash party, send party, and hash ID.
     */
    std::tuple<int, int, std::pair<int, int>> _jmp_assignments(int Pi, int Pj) {
        int hash_party = who_hashes(Pi, Pj);
        int send_party = Pi == hash_party ? Pj : Pi;

        auto hash_id = hash_party < send_party ? std::make_pair(hash_party, send_party)
                                               : std::make_pair(send_party, hash_party);

        assert(hash_states.contains(hash_id));

        return {hash_party, send_party, hash_id};
    }

    /**
     * @brief JMP receive operation for malicious security.
     *
     * @param x Vector to receive data into.
     * @param Pi First sender party.
     * @param Pj Second sender party.
     * @param Pr Receiver party (must be this party).
     */
    void _jmp_recv(Vector &x, int Pi, int Pj, int Pr) {
        // ONLY receiver can call this.
        assert(this->partyID == Pr);

        auto [_, send_party, hash_id] = _jmp_assignments(Pi, Pj);

        auto span = x.batch_span();
        auto byte_ptr = reinterpret_cast<u_char *>(span.data());

        // Receive & update my hash.
        this->communicator->receiveShares(x, abs2rel(send_party), x.size());
        crypto_generichash_update(hash_states[hash_id].get(), byte_ptr, span.size_bytes());
    }

    /**
     * @brief JMP send operation for malicious security.
     *
     * @param x Vector to send.
     * @param Pi First sender party.
     * @param Pj Second sender party.
     * @param Pr Receiver party.
     */
    void _jmp_send(const Vector &x, int Pi, int Pj, int Pr) {
        auto [hash_party, send_party, hash_id] = _jmp_assignments(Pi, Pj);

        auto span = x.batch_span();
        auto byte_ptr = reinterpret_cast<const u_char *>(span.data());

        if (this->partyID == send_party) {
            this->communicator->sendShares(x, abs2rel(Pr), x.size());
        } else if (this->partyID == hash_party) {
            // update hash
            crypto_generichash_update(hash_states[hash_id].get(), byte_ptr, span.size_bytes());
        }
    }

   public:
    /**
     * @brief Override of default groups to achieve malicious security.
     *
     * This group selection gives us two copies of each share per group (redundancy).
     *
     * @return Vector of party groups for malicious security.
     */
    std::vector<std::set<int>> getGroups() const {
        return {{0, 1, 2}, {1, 2, 3}, {2, 3, 0}, {3, 0, 1}};
    }

    // Configuration Parameters
    static int parties_num;

    /**
     * @brief Constructor for Fantastic_4PC protocol (Dalskov implementation).
     *
     * @param _partyID Party identifier.
     * @param _communicator Pointer to communicator.
     * @param _randomnessManager Pointer to randomness manager.
     */
    Fantastic_4PC(PartyID _partyID, Communicator *_communicator,
                  random::RandomnessManager *_randomnessManager)
        : Protocol<Data, Share, Vector, EVector>(_communicator, _randomnessManager, _partyID, 4,
                                                 3) {
        for (int Pi = 0; Pi < 4; Pi++) {
            for (int Pj = Pi + 1; Pj < 4; Pj++) {
                hash_states[{Pi, Pj}] = std::make_unique<crypto_generichash_state>();

                init_hash(Pi, Pj);
            }
        }
    }

    /**
     * @brief Joint message passing protocol.
     *
     * @param x Vector of (non-secret shared) data.
     * @param from Owner of this data.
     * @param also_from Co-owner of data.
     * @param to Party who will receive data from both.
     */
    void jmp(Vector &x, int from, int also_from, int to) {
        if (this->partyID == to) {
            _jmp_recv(x, from, also_from, to);
        } else if (this->partyID == from || this->partyID == also_from) {
            _jmp_send(x, from, also_from, to);
        }
        // excluded party nops (doesn't even need to call, but probably best
        // to do so)
    }

    /**
     * @brief Shared-input function.
     *
     * Two parties, who both know a plaintext value x, secret-share it with the other two parties.
     *
     * @tparam E Encoding type (A- or B-shared).
     * @param x Plaintext data.
     * @param Pi First owner.
     * @param Pj Second owner.
     * @param Pg Optional third party (computed if not provided).
     * @param Ph Optional fourth party (computed if not provided).
     * @return Shared vector.
     */
    template <orq::Encoding E>
    EVector inp(const Vector &x, int Pi, int Pj, std::optional<int> Pg = {},
                std::optional<int> Ph = {}) {
        size_t n = x.size();
        EVector r(n);

        // Shares are: (assuming i < j < g < h)
        //   Pi:  . 0 xg xh
        //   Pj:  0 . xg xh
        //   Pg:  0 0 .  xh
        //   Ph:  0 0 xg .
        // with xi = xj = 0

        if (!Pg) {
            Pg = next_party(Pi, Pj);
        }

        if (!Ph) {
            Ph = excluded_party(Pi, Pj, *Pg);
        }

        // Pg, Ph respectively will just have junk here, but it's ignored
        Vector xg = r(abs2sh(*Pg));
        Vector xh = r(abs2sh(*Ph));

        if (this->partyID != Pg) {
            // Pi, Pj, Ph generate random, excluding Pg
            this->randomnessManager->commonPRGManager->get(abs2rel(*Pg))->getNext(xg);

            if (this->partyID != Ph) {
                // Pi Pj generate xh
                if constexpr (E == Encoding::AShared) {
                    xh = x - xg;
                } else if constexpr (E == Encoding::BShared) {
                    xh = x ^ xg;
                }
            }
        }

        // Ph does nothing in this function
        jmp(xh, Pi, Pj, *Pg);

        return r;
    }

    /**
     * @brief Multiply two arithmetic-shared vectors.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void multiply_a(const EVector &x, const EVector &y, EVector &z) {
        int Pi, Pj, Pg, Ph, hi, gi;
        EVector r(x.size());

        // Iteration order:
        // (0, 1) (0, 2) (0, 3)
        // (1, 2) (1, 3)
        // (2, 3)
        for (Pi = 0; Pi < 4; Pi++) {
            for (Pj = Pi + 1; Pj < 4; Pj++) {
                // need to convert absolute share indices to relative...
                // other parties
                Pg = next_party(Pi, Pj);
                Ph = excluded_party(Pi, Pj, Pg);

                if (this->partyID == Pg || this->partyID == Ph) {
                    // hack for now: these parties pass in nothing on this call
                    // just need a vector to pull the size out of
                    // TODO: make an alternative signature that just takes `n`
                    r += inp<Encoding::AShared>(r(0), Pi, Pj, Pg, Ph);
                } else {
                    // compute relative share indices
                    hi = abs2sh(Ph);
                    gi = abs2sh(Pg);

                    r += inp<Encoding::AShared>(x(hi) * y(gi) + x(gi) * y(hi), Pi, Pj, Pg, Ph);
                }
            }
        }

        // self terms.
        z = r + x * y;

        this->handle_precision(x, y, z);
        this->truncate(z);
    }

    /**
     * @brief Divide an arithmetic-shared vector by a constant.
     *
     * @param x Input vector.
     * @param c Constant divisor.
     * @return Pair of vectors (quotient and error correction).
     */
    std::pair<EVector, EVector> div_const_a(const EVector &x, const Data &c) {
        auto size = x.size();
        EVector res(size), err(size);

        // Computation
        if (this->partyID == 3 || this->partyID == 1) {
            res(1) = x(0) + x(1);
            auto [res_, err_] = res(1).divrem(c);

#ifdef USE_DIVISION_CORRECTION
            auto x_sum_neg = res(1) < 0;

            res_ -= x_sum_neg;
            err_ += x_sum_neg * c;

            if (this->partyID == 3) {
                err_ = err_ - c;
            }

            err = inp<Encoding::AShared>(err_, 3, 2) + inp<Encoding::AShared>(err_, 1, 0);
#endif
            res = inp<Encoding::AShared>(res_, 3, 2) + inp<Encoding::AShared>(res_, 1, 0);

        } else if (this->partyID == 2 || this->partyID == 0) {
            res(2) = x(1) + x(2);
            auto [res_, err_] = res(2).divrem(c);

#ifdef USE_DIVISION_CORRECTION
            auto x_sum_neg = res(2) < 0;

            res_ -= x_sum_neg;
            err_ += x_sum_neg * c;

            if (this->partyID == 2) {
                err_ = err_ - c;
            }
            err = inp<Encoding::AShared>(err_, 3, 2) + inp<Encoding::AShared>(err_, 1, 0);
#endif
            res = inp<Encoding::AShared>(res_, 3, 2) + inp<Encoding::AShared>(res_, 1, 0);
        }

        return {res, err};
    }

    /**
     * @brief Boolean AND operation.
     *
     * @param x First input vector.
     * @param y Second input vector.
     * @param z Output vector.
     */
    void and_b(const EVector &x, const EVector &y, EVector &z) {
        int Pi, Pj, Pg, Ph, hi, gi;
        EVector r(x.size());

        for (Pi = 0; Pi < 4; Pi++) {
            for (Pj = Pi + 1; Pj < 4; Pj++) {
                Pg = next_party(Pi, Pj);
                Ph = excluded_party(Pi, Pj, Pg);

                if (this->partyID == Pg || this->partyID == Ph) {
                    r ^= inp<Encoding::BShared>(r(0), Pi, Pj, Pg, Ph);
                } else {
                    hi = abs2sh(Ph);
                    gi = abs2sh(Pg);

                    r ^= inp<Encoding::BShared>(x(hi) & y(gi) ^ x(gi) & y(hi), Pi, Pj, Pg, Ph);
                }
            }
        }

        // self terms.
        z = r ^ (x & y);

        this->handle_precision(x, y, z);
    }

    /**
     * @brief Boolean NOT operation.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void not_b(const EVector &x, EVector &y) {
        int p = this->partyID;
        y = {
            (p == 2 ? ~x(0) : x(0)),
            (p == 1 ? ~x(1) : x(1)),
            (p == 0 ? ~x(2) : x(2)),
        };
    }

    /**
     * @brief Boolean NOT operation for the least significant bit.
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void not_b_1(const EVector &x, EVector &y) {
        int p = this->partyID;
        Vector x0 = x(0) & 1, x1 = x(1) & 1, x2 = x(2) & 1;

        y = {
            (p == 2 ? !x0 : x0),
            (p == 1 ? !x1 : x1),
            (p == 0 ? !x2 : x2),
        };
    }

    /**
     * @brief Convert a boolean-shared bit in the least significant position
     * to an arithmetic sharing:
     *
     * 4PC share distribution:
     * P0: [B, C, D]  ->       B   C   D
     * P1: [C, D, A]  ->   A       C   D
     * P2: [D, A, B]  ->   A   B       D
     * P3: [A, B, C]  ->   A   B   C
     *
     * Reducing the 4-share to a 2-share:
     * share0 = C ^ D
     * share1 = A ^ B
     *
     * P0 and P1 can both calculate share0.
     * P2 and P3 can both calculate share1.
     *
     * Since 2 parties can calculate each of the above, INP can be
     * used to generate a secret-share for share0 and share1.
     *
     * XOR evaluated as: x ^ y = (x - y)^2 (holds true for binary input)
     *
     * @param x Input vector.
     * @param y Output vector.
     */
    void b2a_bit(const EVector &x, EVector &y) {
        EVector x_prime(x);
        x_prime.mask(1);

        Vector s0(x.size()), s1(x.size());

        // Reducing to a 2-share
        if (this->partyID == 0) {
            s0 = x_prime(1) ^ x_prime(2);
        } else if (this->partyID == 1) {
            s0 = x_prime(0) ^ x_prime(1);
        } else if (this->partyID == 2) {
            s1 = x_prime(1) ^ x_prime(2);
        } else if (this->partyID == 3) {
            s1 = x_prime(0) ^ x_prime(1);
        }

        // Distributing reduced shares
        // Reusing vectors since inp<Encoding::AShared> operates on a new vector internally
        auto share0 = inp<Encoding::AShared>(s0, 1, 0);
        auto share1 = inp<Encoding::AShared>(s1, 3, 2);

        // precision is 0 automatically

        // XOR: (x-y)^2
        share0 -= share1;
        multiply_a(share0, share0, y);  // Reusing share1 to avoid extra vector allocation
    }

    /**
     * @brief Redistribute boolean shares.
     *
     * @param x Input vector.
     * @return Pair of vectors (redistributed shares).
     */
    std::pair<EVector, EVector> redistribute_shares_b(const EVector &x) {
        auto size = x.size();
        EVector res_1(size), res_2(size);

        // Computation
        if (this->partyID == 0 || this->partyID == 2) {
            res_2(0) = x(0) + x(1);
        } else if (this->partyID == 3 || this->partyID == 1) {
            res_2(0) = x(1) + x(2);
        }

        res_1 = inp<Encoding::BShared>(res_2(0), 0, 3);
        res_2 = inp<Encoding::BShared>(res_2(0), 2, 1);

        return {res_1, res_2};
    }

    /**
     * @brief Reconstruct arithmetic shares from a vector of shares.
     *
     * @param shares Input shares.
     * @return Reconstructed arithmetic shares.
     */
    Data reconstruct_from_a(const std::vector<Share> &shares) {
        return shares[0][0] + shares[1][0] + shares[2][0] + shares[3][0];
    }

    /**
     * @brief Reconstruct arithmetic shares from a vector of shares.
     *
     * @param shares Input shares.
     * @return Reconstructed arithmetic shares.
     */
    Vector reconstruct_from_a(const std::vector<EVector> &shares) {
        return shares[0](0) + shares[1](0) + shares[2](0) + shares[3](0);
    }

    /**
     * @brief Reconstruct boolean shares from a vector of shares.
     *
     * @param shares Input shares.
     * @return Reconstructed boolean shares.
     */
    Data reconstruct_from_b(const std::vector<Share> &shares) {
        return shares[0][0] ^ shares[1][0] ^ shares[2][0] ^ shares[3][0];
    }

    /**
     * @brief Reconstruct boolean shares from a vector of shares.
     *
     * @param shares Input shares.
     * @return Reconstructed boolean shares.
     */
    Vector reconstruct_from_b(const std::vector<EVector> &shares) {
        return shares[0](0) ^ shares[1](0) ^ shares[2](0) ^ shares[3](0);
    }

    /**
     * @brief Open arithmetic shares to reveal plaintext.
     *
     * @param shares Input shared vector.
     * @return Opened plaintext vector.
     */
    Vector open_shares_a(const EVector &shares) {
        auto sh3 = _open_shares(shares);
        return shares(0) + shares(1) + shares(2) + sh3;
    }

    /**
     * @brief Open boolean shares to reveal plaintext.
     *
     * @param shares Input shared vector.
     * @return Opened plaintext vector.
     */
    Vector open_shares_b(const EVector &shares) {
        auto sh3 = _open_shares(shares);
        return shares(0) ^ shares(1) ^ shares(2) ^ sh3;
    }

    /**
     * @brief Malicious check for hash consistency.
     *
     * @param should_abort Whether to abort on failure.
     * @return True if checks passed, false otherwise.
     */
    bool malicious_check(bool should_abort = true) {
        auto N = crypto_generichash_blake2b_BYTES;
        orq::Vector<int8_t> hash(N);
        bool ok = true;
        for (int i = 0; i < 4; i++) {
            for (int j = i + 1; j < 4; j++) {
                auto hasher = who_hashes(i, j);
                auto recv = next_party(i, j);
                if (this->partyID != hasher && this->partyID != recv) {
                    continue;
                }

                // Bit hacky, but sodium needs a pointer, while communicator
                // wants a orq::Vector
                crypto_generichash_final(hash_states[{i, j}].get(), (uint8_t *)&hash[0], N);

                //// Uncomment the below to look at each hash view ////
                // std::cout << "P" << this->partyID << "'s view of H" << i << "," << j << ": ";
                // std::cout << std::hex << std::setfill('0');
                // for (int b = 0; b < N; b++) {
                //     std::cout << std::setw(2) << (int) (hash[b] & 0xFF);
                // }
                // std::cout << "\n";
                // std::cout << std::dec << std::setfill(' ');

                // reinitialize hash (not allowed to keep hashing on top of
                // finalized hash)
                // maybe this should be optional?
                init_hash(i, j);

                // hasher sends to receiver, abort if disagree
                if (this->partyID == hasher) {
                    this->communicator->sendShares(hash, abs2rel(recv), N);
                } else if (this->partyID == recv) {
                    orq::Vector<int8_t> recv_hash(N);
                    this->communicator->receiveShares(recv_hash, abs2rel(hasher), N);

                    ok &= recv_hash.same_as(hash, false);

                    if (!ok) {
                        std::cerr << "P" << this->partyID << " recv bad hash from P" << hasher
                                  << "!\n";

                        if (should_abort) {
                            std::cerr << "Aborting.\n";
                            abort();
                        }
                    }
                }
            }
        }

        return ok;
    }

    /**
     * @brief Generate arithmetic shares for a vector.
     *
     * @param data Input data vector.
     * @return Vector of shares for all parties.
     */
    std::vector<EVector> get_shares_a(const Vector &data) {
        return get_shares<Encoding::AShared>(data);
    }

    /**
     * @brief Generate boolean shares for a vector.
     *
     * @param data Input data vector.
     * @return Vector of shares for all parties.
     */
    std::vector<EVector> get_shares_b(const Vector &data) {
        return get_shares<Encoding::BShared>(data);
    }

    /**
     * @brief Secret share data using the custom 4PC protocol.
     *
     * @param data Input data vector.
     * @param data_party Party ID of the data party.
     * @return Secret shared vector.
     */
    EVector secret_share_b(const Vector &data, const PartyID &data_party = 0) {
        return _secret_share<Encoding::BShared>(data, data_party);
    }

    /**
     * @brief Secret share data using the custom 4PC protocol.
     *
     * @param data Input data vector.
     * @param data_party Party ID of the data party.
     * @return Secret shared vector.
     */
    EVector secret_share_a(const Vector &data, const PartyID &data_party = 0) {
        return _secret_share<Encoding::AShared>(data, data_party);
    }

    /**
     * @brief Public sharing of a vector x.
     *
     * P0 gets: (   0, 0, 0)
     * P1 gets: (x,    0, 0)
     * P2 gets: (x, 0,    0)
     * P3 gets: (x, 0, 0   )
     *
     * @param x Input data vector.
     * @return EVector
     */
    EVector public_share(const Vector &x) {
        auto size = x.size();
        // zero initialized
        EVector r(size);

        if (this->partyID > 0) {
            // P1 share 2, P2 share 1, P3 share 0
            r(3 - this->partyID) = x;
        }

        return r;
    }

    /**
     * @brief An override version of reshare for malicious security in 4PC.
     *
     * @param v The vector to be rerandomized and reshared.
     * @param group The group performing the resharing.
     * @param binary A flag indicating an arithmetic or binary encoding of the vector.
     */
    void reshare(EVector &v, const std::set<int> group, bool binary) {
        assert(group.size() == 3);

        // find receive party
        int receiver = (0 + 1 + 2 + 3) - std::accumulate(group.begin(), group.end(), 0);

        // all parties in the group generate a zero sharing to rerandomize the vector
        if (receiver != this->partyID) {
            // generate randomness
            std::vector<Vector> rand;
            for (int i = 0; i < 4; i++) {
                rand.push_back(Vector(v.size()));
            }

            if (binary) {
                this->randomnessManager->zeroSharingGenerator->groupGetNextBinary(rand, group);
            } else {
                this->randomnessManager->zeroSharingGenerator->groupGetNextArithmetic(rand, group);
            }

            auto my_shares = this->getPartyShareMappings()[this->partyID];
            // Generating too many random values here: each party only needs
            // RepNum random vectors, but is generating PartyNum
            for (int i = 0; i < my_shares.size(); i++) {
                if (binary) {
                    v(i) ^= rand[my_shares[i]];
                } else {
                    v(i) += rand[my_shares[i]];
                }
            }
        }

        // distribute new shares - by absolute index
        for (int sh = 0; sh < 4; sh++) {
            // receiver does not get or have this share
            if (receiver == sh) {
                continue;
            }

            int Pi = -1;
            int Pj = -1;

            // Pi, Pj are the two parties who have this share
            // Order not important because jmp assigns canonical roles
            for (auto g : group) {
                if (sh == g) continue;
                if (Pi < 0) {
                    Pi = g;
                } else if (Pj < 0) {
                    Pj = g;
                    break;
                }
            }

            auto rel_sh = abs2sh(sh);

            jmp(v(rel_sh), Pi, Pj, receiver);
        }
    }
};

template <typename D, typename S, typename V, typename E>
int Fantastic_4PC<D, S, V, E>::parties_num = 4;

/**
 * @brief Factory type alias for Fantastic_4PC protocol (Dalskov implementation).
 *
 * @tparam Share Share type template.
 * @tparam Vector Vector type template.
 * @tparam EVector Encoding vector type template.
 */
template <template <typename> class Share, template <typename> class Vector,
          template <typename> class EVector>
using Fantastic_4PC_Factory = DefaultProtocolFactory<Fantastic_4PC, Share, Vector, EVector>;

}  // namespace orq
