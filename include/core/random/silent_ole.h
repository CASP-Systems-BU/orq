#pragma once

#include "../containers/encoding.h"
#include "ole_generator.h"

#ifdef USE_LIBOTE
#include "coproto/Socket/AsioSocket.h"
#include "libOTe/Tools/CoeffCtx.h"
#include "libOTe/TwoChooseOne/SoftSpokenOT/SoftSpokenShOtExt.h"
#include "libOTe/config.h"
#endif


const int SILENT_OLE_BASE_PORT = 8877;
const int NUM_POSSIBLE_TYPES = 16; // support for all n-byte types up to 128 bits / 16 bytes

namespace secrecy::random {
template <typename T>
class SilentOLE : public OLEGenerator<T, Encoding::AShared> {
#ifdef USE_LIBOTE
  using OLEBase = OLEGenerator<T, Encoding::AShared>;

  oc::PRNG prng;

  oc::Socket sock;
  bool isServer;

  std::unique_ptr<oc::SoftSpokenShOtReceiver<>> recv;
  std::unique_ptr<oc::SoftSpokenShOtSender<>> send;

  size_t L = std::numeric_limits<std::make_unsigned_t<T>>::digits;

 public:
  SilentOLE(int rank, Communicator *comm, int thread)
      : OLEBase(rank, comm), prng(oc::sysRandomSeed()) {
    // P0 (machine-1) is server
    isServer = (rank == 0);

    int port = SILENT_OLE_BASE_PORT + (thread * NUM_POSSIBLE_TYPES) + sizeof(T);

    auto addr = std::string(LIBOTE_SERVER_HOSTNAME ":") + std::to_string(port);
    sock = oc::cp::asioConnect(addr, isServer);

    if (isServer) {
      recv = std::make_unique<oc::SoftSpokenShOtReceiver<>>();
    } else {
      send = std::make_unique<oc::SoftSpokenShOtSender<>>();
    }
  }

  OLEBase::ole_t getNext(size_t n) {
    // Both parties generate and randomize a multiplicative share.
    Vector<T> x(n), r(n), m(n), y(n);
    // TODO: this should not be zero. Maybe have localPRG special option to
    // use rejection sampling?
    prng.get(x.span());
    oc::BitVector ch(n);

    oc::AlignedUnVector<std::array<oc::block, 2>> msg(n);
    oc::AlignedUnVector<oc::block> r_msg(n);

    for (int b = 0; b < L; b++) {
      if (isServer) {

        // receiver chooses according to b-bit of x; copy into bitvector
        r = (x >> b) & 1;
        for (int j = 0; j < n; j++) {
          ch[j] = r[j];
        }

        // TODO: choice bit _could_ be randomized by this call; would need
        // to implement receiveChosen [message] with random choice?
        // Then construct x bit-by-bit.
        oc::cp::sync_wait(recv->receiveChosen(ch, r_msg, prng, sock));
        oc::cp::sync_wait(sock.flush());

        // Update additive share based on response
        for (int i = 0; i < n; i++) {
          y[i] += r_msg[i].get<T>(0);
        }
      } else {
        // generate random mask, apply to b-shifted share
        prng.get(r.span());
        m = r + (x << b);

        // OT messages are [r_i, r_i + (x_i << b)]
        // TODO: could do better packing here, maybe?
        // Messages [ r_i0        | r_i1        | ... ] and
        //          [ r_i0 + x_i0 | r_i1 + x_i1 | ... ] ?
        // But then choice bits can't be fully specified.
        for (int i = 0; i < n; i++) {
          msg[i][0].set<T>(0, r[i]);
          msg[i][1].set<T>(0, m[i]);
        }

        oc::cp::sync_wait(send->sendChosen(msg, prng, sock)); 
        oc::cp::sync_wait(sock.flush());

        y -= r;        
      }
    }

    return {y, x};
  }
#endif
};
};  // namespace secrecy::random
