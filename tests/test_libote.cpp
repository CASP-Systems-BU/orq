/**
 * @file test_libote.cpp
 * @brief Test that libOTe is properly installed and basic functionality works.
 * 
 */

#include "../include/secrecy.h"

// This test only runs on 2pc
#ifdef MPC_PROTOCOL_BEAVER_TWO

#include "coproto/Socket/AsioSocket.h"
#include "coproto/Socket/BufferingSocket.h"
#include "libOTe/config.h"
#include "libOTe/Tools/CoeffCtx.h"
#include "libOTe/Vole/Noisy/NoisyVoleReceiver.h"
#include "libOTe/Vole/Noisy/NoisyVoleSender.h"
#include "libOTe/Vole/Silent/SilentVoleReceiver.h"
#include "libOTe/Vole/Silent/SilentVoleSender.h"
#include "libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h"
#include "libOTe/TwoChooseOne/Silent/SilentOtExtSender.h"
#include "cryptoTools/Common/MatrixView.h"

using namespace oc;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#include <string>
#include <bit>

#include <macoro/sync_wait.h>
#include <macoro/task.h>
#include <macoro/when_all.h>

// Globally working over arithmetic VOLE (integer context) for now
using Ctx = CoeffCtxInteger;

// Don't conflict with Secrecy's libOTe instance
#define TESTING_ADDRESS "localhost:29876"

inline auto eval(macoro::task<>& t0, macoro::task<>& t1)
{
    auto r = macoro::sync_wait(macoro::when_all_ready(std::move(t0), std::move(t1)));
    std::get<0>(r).result();
    std::get<1>(r).result();
}

/**
 * @brief Test Noisy VOLE over a local socket (i.e., sender and receiver are two
 * threads of the same process)
 * 
 * @tparam F
 * @param n 
 */
template<typename F>
void test_noisy_vole_local(u64 n)
{

    auto L = std::numeric_limits<std::make_unsigned_t<F>>::digits;
    std::cout << "Local Noisy VOLE " << L << "-bit... ";

    std::vector<F> A(n), B(n), C(n);

    PRNG prng(sysRandomSeed());
    
    // Generate (delta, C) randomly
    F delta = prng.get();
    prng.get(C.data(), C.size());

    Ctx ctx;

    auto send = std::make_unique<NoisyVoleSender<F, F, Ctx>>();
    auto recv = std::make_unique<NoisyVoleReceiver<F, F, Ctx>>();

    auto chls = cp::LocalAsyncSocket::makePair();

    // Spoof the base OTs
    BitVector recvChoice = ctx.binaryDecomposition(delta);
    std::vector<block> otRecvMsg(recvChoice.size());
    std::vector<std::array<block, 2>> otSendMsg(recvChoice.size());
    prng.get<std::array<block, 2>>(otSendMsg);
    for (u64 i = 0; i < recvChoice.size(); ++i) {
        otRecvMsg[i] = otSendMsg[i][recvChoice[i]];
    }

    auto p0 = recv->receive(C, A, prng, otSendMsg, chls[1], ctx);
    auto p1 = send->send(delta, B, prng, otRecvMsg, chls[0], ctx);
    eval(p0, p1);

    bool all_zero = true;

    for (int i = 0; i < A.size(); i++) {
        assert(A[i] == (F) (B[i] + C[i] * delta));
        if (A[i] != 0) {
            all_zero = false;
        }
    }
    // Make sure this isn't the trivial (zero) correlation
    assert(! all_zero);

    std::cout << "OK\n";
}

/**
 * @brief Test Silent OLE over a local socket (i.e., sender and receiver are
 * two threads of the same process)
 * 
 * @tparam F 
 * @tparam Ctx 
 * @param n 
 */
template<typename F>
void test_silent_vole_local(u64 n)
{
    auto L = std::numeric_limits<std::make_unsigned_t<F>>::digits;
    std::cout << "Local Silent VOLE " << L << "-bit... ";

    using VecF = typename Ctx::template Vec<F>;

    VecF A(n), B(n), C(n);

    PRNG prng(sysRandomSeed());
    F delta = prng.get();

    Ctx ctx;

    auto chls = cp::LocalAsyncSocket::makePair();

    auto recv = std::make_unique<SilentVoleReceiver<F, F, Ctx>>();
    auto send = std::make_unique<SilentVoleSender<F, F, Ctx>>();

    auto p0 = recv->silentReceive(C, A, prng, chls[0]);
    auto p1 = send->silentSend(delta, B, prng, chls[1]);
    eval(p0, p1);

    bool all_zero = true;

    for (int i = 0; i < A.size(); i++) {
        assert(A[i] == (F) (B[i] + C[i] * delta));
        if (A[i] != 0) {
            all_zero = false;
        }
    }

    assert(! all_zero);

    std::cout << "OK\n";
}

const uint OT_BLOCK_SIZE = 128;
const uint LOG_OT_BLOCK_SIZE = std::bit_width(OT_BLOCK_SIZE) - 1;

/**
 * @brief Test Silent OT extension over a local socket. After performing 128-bit
 * OTs, transform them into the normal VOLE-like form, where `Mb = A ^ (d & ch)`
 * 
 * @param n input size, which must be a multiple of 128
 */
template<typename F>
void test_silent_ot_local(u64 n)
{
    assert(n % OT_BLOCK_SIZE == 0);
    auto L = std::numeric_limits<std::make_unsigned_t<F>>::digits;
    std::cout << "Local Silent OT " << L << "-bit... ";

    PRNG prng(sysRandomSeed());

    auto chls = cp::LocalAsyncSocket::makePair();

    auto recv = std::make_unique<SilentOtExtReceiver>();
    auto send = std::make_unique<SilentOtExtSender>();

    recv->configure(n);
    send->configure(n);

    AlignedUnVector<block> r_msg(n);
    BitVector choice(n);

    AlignedUnVector<std::array<block, 2>> s_msg(n);

    auto p0 = recv->silentReceive(choice, r_msg, prng, chls[0]);
    auto p1 = send->silentSend(s_msg, prng, chls[1]);
    eval(p0, p1);

    // postprocessing...

    // extract A, delta;
    AlignedUnVector<block> A(n), delta(n);
    for (int i = 0; i < n; i++) {
        A[i] = s_msg[i][0];
        delta[i] = A[i] ^ s_msg[i][1];
    }
    
    std::span<block> Am(A);
    std::span<block> dm(delta);
    std::span<block> Mb(r_msg);

    // transpose 128x128 bit blocks
    for (int offset = 0; offset < n; offset += OT_BLOCK_SIZE) {
        transpose128(Am.data() + offset);
        transpose128(dm.data() + offset);
        transpose128(Mb.data() + offset);
    }

    
    auto ch = choice.blocks();

    // Check relationship over 128-bit blocks
    for (int i = 0; i < n; i++) {
        assert(Mb[i] == (Am[i] ^ dm[i] & ch[i >> ::LOG_OT_BLOCK_SIZE]));
    }

    auto scaled_n = Am.size_bytes() / sizeof(F);

    std::span<F> AF(reinterpret_cast<F*>(Am.data()), scaled_n);
    std::span<F> dF(reinterpret_cast<F*>(dm.data()), scaled_n);
    std::span<F> MF(reinterpret_cast<F*>(Mb.data()), scaled_n);
    std::span<F> chF = choice.getSpan<F>();
    
    auto z = chF.size();
    auto scale = OT_BLOCK_SIZE / (8 * sizeof(F));
    auto scale_mask = scale - 1;

    for (int i = 0; i < scaled_n; i++) {
        auto c_idx = (i & scale_mask) + ((i >> (::LOG_OT_BLOCK_SIZE)) & ~scale_mask);
        // std::cout << VAR(i) << VAR(c_idx) << "\n";
        auto c = chF[c_idx];
        assert(MF[i] == (AF[i] ^ dF[i] & c));
    }

    std::cout << "OK\n";
}

/**
 * @brief Test silent VOLE with two parties (presumed to already be running the
 * test; e.g., over MPI). Set up an actual network connection between the two
 * parties over the loopback interface. This test exercises the real deployment
 * scenario.
 * 
 * @tparam F 
 * @param n 
 */
template<typename F>
void test_silent_vole_loopback(u64 n)
{
    auto L = std::numeric_limits<std::make_unsigned_t<F>>::digits;
    single_cout_nonl("LAN Silent OLE " << L << "-bit... ");

    using VecF = typename Ctx::template Vec<F>;
    Ctx ctx;

    // Each party instantiates their own PRNG
    PRNG prng(sysRandomSeed());

    bool isServer = (runTime->getPartyID() == 0);

    auto sock = cp::asioConnect(TESTING_ADDRESS, isServer);

    VecF A(n), B(n), C(n);
    F delta = 0;

    if (isServer) {
        auto recv = std::make_unique<SilentVoleReceiver<F, F, Ctx>>();
        cp::sync_wait(recv->silentReceive(C, A, prng, sock));

        // Receive B, delta from other party
        cp::sync_wait(sock.recv(B));
        cp::sync_wait(sock.recv(delta));
    } else {
        auto send = std::make_unique<SilentVoleSender<F, F, Ctx>>();
        delta = prng.get();

        cp::sync_wait(send->silentSend(delta, B, prng, sock));

        // Send B, delta
        cp::sync_wait(sock.send(std::move(B)));
        cp::sync_wait(sock.send(delta));
    }

    cp::sync_wait(sock.flush());

    // Only server (P0) does the check
    if (! isServer) {
        return;
    }

    bool all_zero = true;

    for (int i = 0; i < A.size(); i++) {
        // std::cout << i << " " << (int64_t) A[i] << "\t== " << (int64_t) B[i] << "\t+ " << (int64_t) C[i] << "\t* " << (int64_t) delta << "\n";
        assert(A[i] == (F) (B[i] + C[i] * delta));
        if (A[i] != 0) {
            all_zero = false;
        }
    }

    assert(! all_zero);

    std::cout << "OK\n";
}

/**
 * @brief Test silent OT with two parties. Set up an actual network connection
 * between the two parties over the loopback interface. This test exercises the
 * real deployment scenario.
 * 
 * @tparam F 
 * @param n input size, which must be a multiple of 128
 */
template<typename F>
void test_silent_ot_loopback(u64 n)
{
    assert(n % OT_BLOCK_SIZE == 0);
    auto L = std::numeric_limits<std::make_unsigned_t<F>>::digits;

    bool isServer = (runTime->getPartyID() == 0);

    if (! isServer) {
        std::cout << "LAN Silent OT " << L << "-bit... ";
    }

    PRNG prng(sysRandomSeed());
    auto sock = cp::asioConnect(TESTING_ADDRESS, isServer);

    // OT gives 128 bit outputs, so we actually need fewer than requested;
    // exact amount depends on size of the element requested.
    auto ot_scale = OT_BLOCK_SIZE / L;
    auto numGen = n / ot_scale;

    assert(numGen % 128 == 0);

    if (isServer) {
        // rOT Receiver
        auto recv = std::make_unique<SilentOtExtReceiver>();
        recv->configure(numGen);

        AlignedUnVector<block> r_msg(numGen);
        BitVector choice(numGen);

        cp::sync_wait(recv->silentReceive(choice, r_msg, prng, sock));

        // contiguous memory view of the OT'd bits
        std::span<block> mR(r_msg);

        // transpose chunk-by-chunk to get "VOLE" relation
        for (int offset = 0; offset < numGen; offset += OT_BLOCK_SIZE) {
            transpose128(mR.data() + offset);
        }

        // transposed contiguous memory region reinterpreted as proper type
        std::span<F> mR_F(reinterpret_cast<F*>(mR.data()), n);
        // bitvector interpreted as span of F
        std::span<F> ch_F = choice.getSpan<F>();

        cp::sync_wait(sock.send(mR_F));
        cp::sync_wait(sock.send(ch_F));
        // after sending, do nothing
    } else {
        // rOT Sender
        auto send = std::make_unique<SilentOtExtSender>();
        send->configure(numGen);

        // each vector element consists of (m0, m1)
        std::vector<std::array<block, 2>> s_msg(numGen); 
        cp::sync_wait(send->silentSend(s_msg, prng, sock));

        AlignedUnVector<block> A(n), delta(n);
        for (int i = 0; i < numGen; i++) {
            A[i] = s_msg[i][0];
            delta[i] = s_msg[i][0] ^ s_msg[i][1];
        }

        std::span<block> mA(A), mD(delta);

        for (int offset = 0; offset < numGen; offset += OT_BLOCK_SIZE) {
            transpose128(mA.data() + offset);
            transpose128(mD.data() + offset);
        }

        std::span<F> mA_F(reinterpret_cast<F*>(mA.data()), n);
        std::span<F> mD_F(reinterpret_cast<F*>(mD.data()), n);

        // receive buffers
        std::vector<F> mR_F(n), ch_F(n / OT_BLOCK_SIZE);
        cp::sync_wait(sock.recv(mR_F));
        cp::sync_wait(sock.recv(ch_F));

        auto scale_mask = ot_scale - 1;

        for (int i = 0; i < n; i++) {
            // figure out choice bit offsets.
            // mask out 2 LSBs + higher order bits, shifting back to round
            // see ...TODO... for explanation of this logic
            auto ch_idx = (i & scale_mask) + ((i >> (::LOG_OT_BLOCK_SIZE)) & ~scale_mask);
            auto ch_i = ch_F[ch_idx];
            // std::cout << VAR(i) << VAR(ch_idx) << (int64_t) mR_F[i] << "\t== " << (int64_t) mA_F[i] << "\t^ " << (int64_t) ch_i << "\t& " << (int64_t) mD_F[i] << "\n";
            assert(mR_F[i] == (F) (mA_F[i] ^ (mD_F[i] & ch_i)));
        }

        std::cout << "OK\n";
    }

    cp::sync_wait(sock.flush());
}

template<typename F>
void test_silent_ot_chosen(u64 n) {
    auto L = std::numeric_limits<std::make_unsigned_t<F>>::digits;

    bool isServer = (runTime->getPartyID() == 0);

    if (! isServer) {
        std::cout << "LAN Silent OT Chosen Message " << L << "-bit... ";
    }

    PRNG prng(sysRandomSeed());
    auto sock = cp::asioConnect(TESTING_ADDRESS, isServer);

    if (isServer) {
        // rOT Receiver
        auto recv = std::make_unique<SilentOtExtReceiver>();
        recv->configure(n);

        AlignedVector<block> r_msg(n);
        BitVector choice(n);
        choice.randomize(prng);

        // NOTE: this could really be a silent receive, just need to write
        // wrapper
        cp::sync_wait(recv->receiveChosen(choice, r_msg, prng, sock));

        cp::sync_wait(sock.send(r_msg));
        cp::sync_wait(sock.send(choice));
    } else {
        // rOT Sender
        auto send = std::make_unique<SilentOtExtSender>();
        send->configure(n);

        // each vector element consists of (m0, m1)
        oc::AlignedVector<std::array<block, 2>> s_msg(n);
        for (int i = 0; i < n; i++) {
            s_msg[i][0] = block(0, i);
            s_msg[i][1] = block(0, i * 47201 + 31);
        }

        cp::sync_wait(send->sendChosen(s_msg, prng, sock));

        // receive buffers
        AlignedVector<block> r(n);
        BitVector ch(n);
        cp::sync_wait(sock.recv(r));
        cp::sync_wait(sock.recv(ch));

        for (int i = 0; i < n; i++) {
            // std::cout << r[i] << " == " << s_msg[i][0] << " " << s_msg[i][1] << " [" << ch[i] << "]\n";
            assert(r[i] == s_msg[i][ch[i]]);
        }

        std::cout << "OK\n";
    }

    cp::sync_wait(sock.flush());
}

void test_softspoken(u64 n) {
    bool isRecv = (runTime->getPartyID() == 0);

    if (! isRecv) {
        std::cout << "LAN SoftSpoken OT... ";
    }

    PRNG prng(sysRandomSeed());
    auto sock = cp::asioConnect(TESTING_ADDRESS, isRecv);

    BitVector ch(n);
    AlignedUnVector<block> r_msg(n);
    
    if (isRecv) {
        auto recv = SoftSpokenShOtReceiver<>();

        ch.randomize(prng);

        cp::sync_wait(recv.receive(ch, r_msg, prng, sock));
        cp::sync_wait(sock.flush());

        cp::sync_wait(sock.send(r_msg));
        cp::sync_wait(sock.send(ch));
    } else {
        auto send = SoftSpokenShOtSender<>();
        
        AlignedUnVector<std::array<block, 2>> msg(n);

        cp::sync_wait(send.send(msg, prng, sock));

        cp::sync_wait(sock.recv(r_msg));
        cp::sync_wait(sock.recv(ch));

        for (int i = 0; i < n; i++) {
            assert(msg[i][0] != msg[i][1]);
            assert(r_msg[i] == msg[i][ch[i]]);
        }

        std::cout << "OK\n";
    }
}

int main(int argc, char** argv)  {
    secrecy_init(argc, argv);

    auto pID = runTime->getPartyID();

    const int test_size = 1 << 12;

    if (pID == 0) {
        // Single-party tests of base functionality
        test_noisy_vole_local<int8_t>(test_size);
        test_noisy_vole_local<int32_t>(test_size);
        test_noisy_vole_local<int64_t>(test_size);

        test_silent_vole_local<int8_t>(test_size);
        test_silent_vole_local<int32_t>(test_size);
        test_silent_vole_local<int64_t>(test_size);

        test_silent_ot_local<int8_t>(test_size);
        test_silent_ot_local<int32_t>(test_size);
        test_silent_ot_local<int64_t>(test_size);
    }

    // 2PC tests of Silent VOLE
    test_silent_vole_loopback<int8_t>(test_size);
    test_silent_vole_loopback<int32_t>(test_size);
    test_silent_vole_loopback<int64_t>(test_size);

    // 2PC tests of Silent OT
    test_silent_ot_loopback<int8_t>(test_size);
    test_silent_ot_loopback<int32_t>(test_size);
    test_silent_ot_loopback<int64_t>(test_size);

    test_silent_ot_chosen<int32_t>(test_size);

    test_softspoken(test_size);

    MPI_Finalize();
}
#else // MPC_PROTOCOL_BEAVER_TWO

// Dummy test for non-2pc

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    secrecy_init(argc, argv);
    single_cout("libOTe tests only for 2PC");
#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
}

#endif // MPC_PROTOCOL_BEAVER_TWO
