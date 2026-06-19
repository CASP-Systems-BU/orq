#pragma once
#ifdef USE_LIBOTE
#include "backend/common/libote_io.h"
#include "coproto/Socket/AsioSocket.h"
#include "libOTe/TwoChooseOne/SoftSpokenOT/SoftSpokenShOtExt.h"
#include "libOTe/config.h"
#endif

#include <mutex>

#include "constants.h"
#include "core/containers/encoding.h"
#include "ot_generator.h"

const int SILENT_OT_BASE_PORT = GILBOA_OLE_BASE_PORT + (MAX_POSSIBLE_THREADS * 16);

namespace orq::random {
const size_t OT_BLOCK_SIZE_BITS = 128;
const size_t LOG_OT_BLOCK_SIZE = std::bit_width(OT_BLOCK_SIZE_BITS) - 1;

template <typename T>
class SilentOT;

#ifdef USE_LIBOTE
/**
 * @brief "Base" silent OT generator. Since OT is a bitwise operation,
 * reuse the same generator across all datatypes, chopping up the result
 * as necessary for the user-specified type.
 *
 * BitSilentOT should not be called directly by operators.
 *
 * There is a static vector of instances of this class (one instance per thread).
 *
 */
class BitSilentOT {
    oc::PRNG prng;
    std::shared_ptr<oc::Socket> sock;
    bool isServer;

    std::unique_ptr<OTRecvT> recv;
    std::unique_ptr<OTSendT> send;

    static std::vector<std::unique_ptr<BitSilentOT>> thread_map;
    static std::vector<std::once_flag> init_flags;

   protected:
    /**
     * Receive side of the silent OT protocol.
     * @param n The number of OT instances to perform.
     * @return A pair of bit vectors.
     */
    std::pair<oc::BitVector, oc::BitVector> getNext_recv(size_t n) {
        assert(n > 0);

        // Server is receiver
        // recv.configure(n);

        // I don't think we need to dynamically allocate here, because
        // oc::AlignedUnVector appears to do its own memory (de/)allocation. See
        // cryptoTools/Common/Aligned.h, oc::AlignedUnVector::resize()
        oc::AlignedVector<oc::block> r_msg(n);
        oc::BitVector choice(n);
        oc::BitVector r0(n);
        choice.randomize(prng);

        oc::cp::sync_wait(recv->receive(choice, r_msg, prng, *sock));

        // Copy LSB into bit vector
        for (size_t i = 0; i < r0.size(); i++) {
            r0[i] = r_msg[i].data()[0] & 1;
        }

        return std::make_pair(r0, choice);
    }

    /**
     * Sender side of the silent OT protocol.
     * @param n The number of OT instances to perform.
     * @return A pair of bit vectors.
     */
    std::pair<oc::BitVector, oc::BitVector> getNext_send(size_t n) {
        assert(n > 0);

        // Client is sender
        // send.configure(n);

        oc::AlignedVector<std::array<oc::block, 2>> s_msg(n);

        oc::cp::sync_wait(send->send(s_msg, prng, *sock));
        oc::cp::sync_wait(sock->flush());

        // silentSend gives us a vector of {block, block}. We need to copy
        // LSBs only into two (bit) vectors

        oc::BitVector b0(n), d0(n);

        for (size_t i = 0; i < b0.size(); i++) {
            b0[i] = s_msg[i][0].data()[0] & 1;
            d0[i] = s_msg[i][1].data()[0] & 1;
        }

        // d0 should actually be the bitwise difference (delta) between the
        // two messages.
        d0 ^= b0;

        return std::make_pair(b0, d0);
    }

   public:
    /**
     * Constructor for the bit-level silent OT generator.
     * @param rank The rank of this party.
     * @param thread The thread identifier for port allocation.
     */
    BitSilentOT(int rank, std::string host_prefix, int thread) : prng(oc::sysRandomSeed()) {
        isServer = (rank == 0);
        int port = SILENT_OT_BASE_PORT + thread;

        // Obtain shared io_context (initialised elsewhere during setup)
        auto& ioc = orq::libote_io::libOTeContext::get_ioc();

        auto addr = std::string(host_prefix + ":") + std::to_string(port);
        sock = std::make_shared<oc::Socket>(oc::cp::asioConnect(addr, isServer, ioc));

        if (isServer) {
            recv = std::make_unique<OTRecvT>();
        } else {
            send = std::make_unique<OTSendT>();
        }
    }

    ~BitSilentOT() { oc::cp::sync_wait(sock->flush()); }

    /**
     * Get or create a BitSilentOT instance for the specified thread.
     * @param rank The rank of this party.
     * @param thread The thread identifier.
     * @return A reference to the BitSilentOT instance.
     */
    static BitSilentOT& get(int rank, std::string host_prefix, int thread) {
        if ((thread < 0) || (thread >= MAX_POSSIBLE_THREADS)) {
            throw std::runtime_error("Invalid thread index. Aborting before segfault.");
        }
        std::call_once(init_flags[thread], [rank, host_prefix, thread]() {
            thread_map[thread] = std::make_unique<BitSilentOT>(rank, host_prefix, thread);
        });
        return *thread_map[thread];
    }

    template <typename T>
    friend class SilentOT;
};

// Static vector initialization - each value defaults to nullptr
std::vector<std::unique_ptr<BitSilentOT>> BitSilentOT::thread_map(MAX_POSSIBLE_THREADS);
std::vector<std::once_flag> BitSilentOT::init_flags(MAX_POSSIBLE_THREADS);
#endif

/**
 *
 * @brief Typed Silent OT generator. Outputs correlations of the form
 *     A = B ^ C & D
 * where one party gets (A, C) and the other gets (B, D)
 *
 * @tparam T output correlation type
 */
template <typename T>
class SilentOT : public OTGenerator<T> {
#ifdef USE_LIBOTE
    using OTBase = OTGenerator<T>;

    BitSilentOT* bitOT;
    bool isServer;
    int thread;

    size_t L = std::numeric_limits<std::make_unsigned_t<T>>::digits;

   public:
    /**
     * Constructor for the typed silent OT generator.
     * @param rank The rank of this party.
     * @param comm The communicator for this party.
     * @param thread The thread identifier.
     */
    SilentOT(int rank, std::shared_ptr<CommonPRGManager> m, Communicator* comm, int thread)
        : OTBase(rank, m, comm), thread(thread) {
        isServer = (rank == 0);
        bitOT = &BitSilentOT::get(rank, comm->host_prefix, thread);
    }

    /**
     * Generate silent OT correlations.
     * @param n The number of OT pairs to generate.
     * @return A tuple of two vectors representing the OT correlation.
     */
    OTBase::ole_t getNext(const size_t n) {
        auto bits = n * L;

        oc::BitVector x(bits), y(bits);

        // TODO: confirm this is using move version of BitVector::op=
        if (isServer) {
            // m, c
            std::tie(x, y) = bitOT->getNext_recv(bits);
        } else {
            // b, d
            std::tie(x, y) = bitOT->getNext_send(bits);
        }

        // Chunk blocks of random bits into span with T-size elements. Then
        // convert to Vector and return
        Vector<T> xv(x.getSpan<T>());
        Vector<T> yv(y.getSpan<T>());

        return {yv, xv};
    }

#endif
};
}  // namespace orq::random
