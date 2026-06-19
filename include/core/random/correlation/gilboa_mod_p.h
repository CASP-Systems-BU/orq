/**
 * @file gilboa_mod_p.h
 *
 *
 * Gilboa mod a prime, for small primes (currently implemented for 8 bits.)
 */
#pragma once

#include <span>

#include "backend/common/libote_io.h"
#include "core/containers/encoding.h"
#include "ole_generator.h"

#ifdef USE_LIBOTE
#include "coproto/Socket/AsioSocket.h"
#include "libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h"
#include "libOTe/TwoChooseOne/Silent/SilentOtExtSender.h"
#include "libOTe/TwoChooseOne/SoftSpokenOT/SoftSpokenShOtExt.h"
#include "libOTe/config.h"
#endif

// We don't need a separate instance for each type, so just get one port per thread. Use the range
// just below the standard OLE base.
const int GILBOA_PRIME_BASE_PORT = GILBOA_OLE_BASE_PORT - MAX_POSSIBLE_THREADS;

namespace orq::random {

class GilboaModPrime : public OLEGenerator<uint8_t, uint16_t>,
                       public OLEmodPrimeInterface<uint8_t> {
#ifdef USE_LIBOTE

    using Base = OLEmodPrimeInterface<uint8_t>;
    using OLE = OLEGenerator<uint8_t, uint16_t>;
    using T = uint8_t;
    using wide_t = uint16_t;

    oc::PRNG prng;
    oc::Socket sock;
    bool isSender;

    /**
     * @brief The base OT providers. Change this to any libOTe OT extension class, including
     * silent generators.
     *
     */
    std::unique_ptr<OTRecvT> recv;
    std::unique_ptr<OTSendT> send;

    /**
     * @brief Sender side of an additive correlated OT mod prime.
     * Optimized to send type T instead of full blocks by reducing mod prime immediately.
     *
     * @param messages span of chosen messages. The other OT message will be a constant additive
     * factor.
     * @return coproto::task<>
     */
    coproto::task<> sendCorrelatedAdditive(std::span<oc::block> messages) {
        oc::AlignedUnVector<std::array<oc::block, 2>> temp(messages.size());
        oc::AlignedUnVector<T> temp2(messages.size());

        co_await (send->send(temp, prng, sock));

        auto p = getPrime();

        for (uint64_t i = 0; i < static_cast<uint64_t>(messages.size()); ++i) {
            // Reduce OT messages mod prime immediately
            T msg_val = messages[i].get<T>(0);

            // To avoid bias, don't downcast yet. Keep the values as u128's and then reduce mod p.
            // This gives bias at most p / 2^128 << 2^-40
            T m0 = temp[i][0].get<__uint128_t>(0) % p;
            T m1 = temp[i][1].get<__uint128_t>(0) % p;

            // Compute delta: m1 - (m0 + msg_val) mod p
            temp2[i] = static_cast<T>((m1 + p - ((m0 + msg_val) % p)) % p);
            messages[i].set<T>(0, m0);
        }

        // Send delta
        co_await (sock.send(std::move(temp2)));
    }

    /**
     * @brief Receiver side of an additive correlated OT mod prime.
     * Optimized to receive type T instead of full blocks.
     *
     * @param choices
     * @param msg Reference (span) of the output
     * @return coproto::task<>
     */
    coproto::task<> receiveCorrelatedAdditive(const oc::BitVector& choices, std::span<wide_t> msg) {
        MACORO_TRY {
            auto OTbuff = oc::AlignedUnVector<oc::block>(msg.size());
            auto temp = oc::AlignedUnVector<T>(msg.size());

            co_await (recv->receive(choices, OTbuff, prng, sock));
            co_await (sock.recv(temp));

            auto p = getPrime();

            auto iter = choices.begin();
            for (uint64_t i = 0; i < temp.size(); ++i, ++iter) {
                // Read r_b as u128 then reduce mod p to avoid truncation
                T r_b = OTbuff[i].get<__uint128_t>(0) % p;
                // temp[i] is delta = (r1 - r0 - a) mod p from sender
                // choice=0: want r0 = r_b - 0
                // choice=1: want r0 + a = r_b - delta = r1 - (r1 - r0 - a) = r0 + a
                //
                // In the following line, *iter is equivalent to choice[i]
                msg[i] = (r_b + ((*iter) ? (p - temp[i]) : 0)) % p;
            }
        }
        MACORO_CATCH(eptr) {
            if (!sock.closed()) co_await sock.close();
            std::rethrow_exception(eptr);
        }
    }

    /**
     * @brief Function for the OT sender to run.
     *
     * @param a input vector (multiplicative share)
     * @return Vector<T> additive share
     */
    Vector<wide_t> senderFunction(Vector<T>& a) {
        auto n = a.size();
        Vector<wide_t> b(n);

        oc::AlignedUnVector<oc::block> msg(n);

        auto p = getPrime();
        auto L = primeBits();

        for (int bit = 0; bit < L; bit++) {
            for (size_t i = 0; i < n; i++) {
                // Store a[i] as T (already < prime)
                msg[i].set<T>(0, a[i]);
            }

            oc::cp::sync_wait(sendCorrelatedAdditive(msg));
            oc::cp::sync_wait(sock.flush());

            for (size_t i = 0; i < n; i++) {
                T val = msg[i].get<T>(0);

                // make negative by subtracting from p
                b[i] += p - ((val << bit) % p);
                // reduce
                b[i] %= p;
            }
        }

        return b;
    }

    /**
     * @brief Function for the OT receiver to run
     *
     * @param x input vector (multiplicative share)
     * @return Vector<T> additive share
     */
    Vector<wide_t> receiverFunction(Vector<T>& x) {
        auto n = x.size();
        Vector<wide_t> y(n), m(n);
        oc::BitVector ch(n);

        auto p = getPrime();
        auto L = primeBits();

        for (int bit = 0; bit < L; bit++) {
            for (int i = 0; i < n; i++) {
                ch[i] = (x[i] >> bit) & 1;
            }

            oc::cp::sync_wait(receiveCorrelatedAdditive(ch, m.span()));
            oc::cp::sync_wait(sock.flush());

            for (size_t i = 0; i < n; i++) {
                // m[i] is already reduced mod p from receiveCorrelatedAdditive
                y[i] += m[i] << bit;
                y[i] %= p;
            }
        }

        return y;
    }

   public:
    /**
     * @brief Constructor
     *
     * @param rank
     * @param m
     * @param comm
     * @param thread
     */
    GilboaModPrime(int rank, std::shared_ptr<CommonPRGManager> m, Communicator* comm,
                   int thread = 0)
        : OLE(rank, m, comm), prng(oc::sysRandomSeed()) {
        // P0 is the sender
        isSender = (rank == 0);

        int port = GILBOA_PRIME_BASE_PORT + thread;

        // Obtain shared io_context (initialised elsewhere during setup)
        auto& ioc = orq::libote_io::libOTeContext::get_ioc();

        auto addr = std::string(comm->host_prefix + ":") + std::to_string(port);
        sock = oc::cp::asioConnect(addr, isSender, ioc);

        if (isSender) {
            send = std::make_unique<OTSendT>();
        } else {
            recv = std::make_unique<OTRecvT>();
        }
    }

    ~GilboaModPrime() {
#ifdef PRINT_COMMUNICATOR_STATISTICS
        if (this->rank == 0) {
            std::cout << "[crt] Sent: " << sock.bytesSent() + sock.bytesReceived() << " bytes\n";
        }
#endif
    }

    /**
     * Generate OLE correlations.
     * @param n The number of OLE pairs to generate.
     * @return A tuple of two vectors representing the OLE correlation.
     */
    Base::ole_t getNext(const size_t n) override {
        // Both parties generate and randomize a multiplicative share. We will
        // compute additive shares y(1) = a(0) * x(1) - b(0)

        Vector<T> mul(n);
        randomize(mul);

        auto add = isSender ? senderFunction(mul) : receiverFunction(mul);

        return {mul, add};
    }

    /**
     * @brief Generate chosen-message OLE correlations
     *
     * @param mul_share
     * @param add_share only passed by sender
     * @return Vector<T> receiver's additive share
     */
    Vector<wide_t> getNext(Vector<T>& mul_share, std::optional<Vector<T>> add_share = {}) override {
        // Get a randomized additive share
        // In the language of [DHI+25], this is b_i for the sender and z_i for the receiver
        // Then adjust.

        auto p = getPrime();

        if (isSender) {
            auto a0 = senderFunction(mul_share);
            Vector<T> corrected = (a0 - *add_share + p) % p;

            // Use AlignedUnVector for sending
            oc::AlignedUnVector<T> aligned_corrected(corrected.size());
            std::copy(corrected.begin(), corrected.end(), aligned_corrected.begin());

            oc::cp::sync_wait(sock.send(std::move(aligned_corrected)));

            return *add_share;
        } else {
            auto a1 = receiverFunction(mul_share);

            // Use AlignedUnVector for receiving
            oc::AlignedUnVector<T> o(mul_share.size());
            oc::cp::sync_wait(sock.recv(o));

            // Convert AlignedUnVector to Vector for arithmetic operations
            Vector<T> o_vec(o.size());

            std::copy(o.begin(), o.end(), o_vec.begin());

            return (a1 + o_vec + p) % p;
        }
    }

    /**
     * @brief Randomization overload
     *
     * TODO: switch this to using rejection sampling.
     *
     * @param x
     */
    void randomize(Vector<T>& x) override {
        OLE::randomize(x);
        x = Base::reduce(x);
        std::cout << "WARNING: calling insecure randomization function.\n";
    }

    void assertCorrelated(const Base::ole_t& ole) const override { OLE::assertCorrelated(ole); }

    bool isCorrelated(Vector<T>& A, Vector<T>& B, Vector<T>& C, Vector<T>& D) const override {
        auto p = getPrime();

        // make sure all values are valid
        if (!((A < p).all_true() && (B < p).all_true() && (C < p).all_true() &&
              (D < p).all_true())) {
            std::cout << "Values out of range!\n";
            return false;
        }

        // prevent overflow by casting up to the wide type
        auto sum = (static_cast<Vector<wide_t>>(A) + B) % p;
        auto prod = (static_cast<Vector<wide_t>>(C) * D) % p;

        return sum.same_as(prod);
    }

    const int getRank() const override { return this->rank; }

    Communicator* getComm() const override { return this->comm.value_or(nullptr); }

#endif
};

};  // namespace orq::random
