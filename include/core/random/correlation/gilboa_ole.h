#pragma once

#include <span>

#include "core/containers/encoding.h"
#include "ole_generator.h"

#ifdef USE_LIBOTE
#include "backend/common/libote_io.h"
#include "coproto/Socket/AsioSocket.h"
#include "libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h"
#include "libOTe/TwoChooseOne/Silent/SilentOtExtSender.h"
#include "libOTe/TwoChooseOne/SoftSpokenOT/SoftSpokenShOtExt.h"
#include "libOTe/config.h"
#endif

namespace orq::random {
/**
 * @brief Gilboa Oblivious Linear Evaluation generator.
 *
 * @tparam T The data type for the correlation elements
 */
template <typename T>
class GilboaOLE : public OLEGenerator<T> {
#ifdef USE_LIBOTE
    using OLEBase = OLEGenerator<T>;

    oc::PRNG prng;

    std::shared_ptr<oc::Socket> sock;
    bool isSender;

    /**
     * @brief The base OT providers. Change this to any libOTe OT extension class, including
     * silent generators.
     *
     */
    std::unique_ptr<OTRecvT> recv;
    std::unique_ptr<OTSendT> send;

    static constexpr size_t L = std::numeric_limits<std::make_unsigned_t<T>>::digits;

    /**
     * @brief Sender side of an additive correlated OT. libOTe provides XOR-correlations; we
     * extend here to provide additive (mod 2^k) correlations.
     *
     * @param messages span of chosen messages. The other OT message will be a constant additive
     * factor.
     * @return coproto::task<>
     */
    coproto::task<> sendCorrelatedAdditive(std::span<oc::block> messages) {
        oc::AlignedUnVector<std::array<oc::block, 2>> temp(messages.size());
        oc::AlignedUnVector<T> temp2(messages.size());

        co_await (send->send(temp, prng, *sock));

        for (uint64_t i = 0; i < static_cast<uint64_t>(messages.size()); ++i) {
            temp2[i] = temp[i][1].get<T>(0) - (temp[i][0].get<T>(0) + messages[i].get<T>(0));
            messages[i] = temp[i][0];
        }

        co_await (sock->send(std::move(temp2)));
    }

    /**
     * @brief Receiver side of an additive correlated OT.
     *
     * @param choices
     * @param msg Reference (span) of the output
     * @return coproto::task<>
     */
    coproto::task<> receiveCorrelatedAdditive(const oc::BitVector& choices, std::span<T> msg) {
        MACORO_TRY {
            auto OTbuff = oc::AlignedUnVector<oc::block>(msg.size());
            auto temp = oc::AlignedUnVector<T>(msg.size());

            co_await (recv->receive(choices, OTbuff, prng, *sock));
            co_await (sock->recv(temp));

            auto iter = choices.begin();
            for (uint64_t i = 0; i < temp.size(); ++i, ++iter) {
                msg[i] = OTbuff[i].get<T>(0) - ((*iter) ? temp[i] : 0);
            }
        }
        MACORO_CATCH(eptr) {
            if (!sock->closed()) co_await sock->close();
            std::rethrow_exception(eptr);
        }
    }

    /**
     * @brief Function for the OT sender to run.
     *
     * @param a input vector (multiplicative share)
     * @return Vector<T> additive share
     */
    Vector<T> senderFunction(Vector<T>& a) {
        auto n = a.size();
        Vector<T> b(n);

        oc::AlignedUnVector<oc::block> msg(n);

        for (int bit = 0; bit < L; bit++) {
            for (size_t i = 0; i < n; i++) {
                msg[i].set<T>(0, a[i]);
            }

            oc::cp::sync_wait(sendCorrelatedAdditive(msg));
            oc::cp::sync_wait(sock->flush());

            for (size_t i = 0; i < n; i++) {
                b[i] -= msg[i].get<T>(0) << bit;
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
    Vector<T> receiverFunction(Vector<T>& x) {
        auto n = x.size();
        Vector<T> y(n), m(n);
        oc::BitVector ch(n);

        for (int bit = 0; bit < L; bit++) {
            for (int i = 0; i < n; i++) {
                ch[i] = (x[i] >> bit) & 1;
            }

            oc::cp::sync_wait(receiveCorrelatedAdditive(ch, m.span()));
            oc::cp::sync_wait(sock->flush());

            y += m << bit;
        }

        return y;
    }

   public:
    GilboaOLE(int rank, std::shared_ptr<CommonPRGManager> m, Communicator* comm, int thread)
        : OLEBase(rank, m, comm), prng(oc::sysRandomSeed()) {
        // P0 is the sender
        isSender = (rank == 0);

        static_assert(std::is_trivial_v<T>, "T must be trivial");
        static_assert(std::is_standard_layout_v<T>, "T must be standard_layout");

        int port = GILBOA_OLE_BASE_PORT + (thread * NUM_POSSIBLE_TYPES) + sizeof(T);

        // Obtain shared io_context (initialised elsewhere during setup)
        auto& ioc = orq::libote_io::libOTeContext::get_ioc();

        auto addr = std::string(comm->host_prefix + ":") + std::to_string(port);
        sock = std::make_shared<oc::Socket>(oc::cp::asioConnect(addr, isSender, ioc));

        if (isSender) {
            send = std::make_unique<orq::OTSendT>();
        } else {
            recv = std::make_unique<orq::OTRecvT>();
        }
    }

    ~GilboaOLE() {
#ifdef PRINT_COMMUNICATOR_STATISTICS
        if (this->rank == 0) {
            std::cout << "[gil] Sent: " << sock->bytesSent() + sock->bytesReceived() << " bytes\n";
        }
#endif

        oc::cp::sync_wait(sock->flush());
    }

    /**
     * Generate OLE correlations.
     * @param n The number of OLE pairs to generate.
     * @return A tuple of two vectors representing the OLE correlation.
     */
    OLEBase::ole_t getNext(size_t n) override {
        // Both parties generate and randomize a multiplicative share. We will
        // compute additve shares y(1) = a(0) * x(1) - b(0)

        Vector<T> mul(n);
        OLEBase::randomize(mul);

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
    Vector<T> getNext(Vector<T>& mul_share, std::optional<Vector<T>> add_share = {}) override {
        // Get a randomized additive share
        // In the language of [DHI+25], this is b_i for the sender and z_i for the receiver
        // Then adjust

        if (isSender) {
            auto addRand = senderFunction(mul_share);

            oc::cp::sync_wait(sock->send((*add_share - addRand).span()));
            return *add_share;
        } else {
            auto y = receiverFunction(mul_share);
            Vector<T> o(mul_share.size());
            oc::cp::sync_wait(sock->recv(o.span()));
            return y - o;
        }
    }

#endif
};

};  // namespace orq::random
