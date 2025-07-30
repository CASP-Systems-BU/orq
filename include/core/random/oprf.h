#pragma once

#include "../containers/encoding.h"

#include "coproto/Socket/AsioSocket.h"
#include "libOTe/config.h"
#include "secure-join/Prf/AltModPrfProto.h"
#include "secure-join/Prf/AltModPrf.h"

// shift up past OLE and past OT
const int OPRF_BASE_PORT = 8877 + (MAX_POSSIBLE_THREADS * 32);

namespace secrecy::random {

    class OPRF {
        int rank;
        
        // our PRF and libOTe's PRG
        oc::PRNG prng;
        
        oc::Socket sock_main_send;
        oc::Socket sock_ole_send;
        oc::Socket sock_main_recv;
        oc::Socket sock_ole_recv;
        bool isServer;

        std::unique_ptr<secJoin::AltModWPrfSender> sender;
        std::unique_ptr<secJoin::AltModWPrfReceiver> receiver;

        secJoin::CorGenerator ole;

        static constexpr size_t key_size = 512;

        public:
            using key_t = secJoin::AltModPrf::KeyType;
    
            OPRF(int rank, int thread) : prng(oc::sysRandomSeed()) {
                isServer = (rank == 0);
                
                sender = std::make_unique<secJoin::AltModWPrfSender>();
                receiver = std::make_unique<secJoin::AltModWPrfReceiver>();

                // setup two sockets
                int port  = OPRF_BASE_PORT + 4 * thread;
                if (rank == 0) {
                    auto addr_main_send = std::string(LIBOTE_SERVER_HOSTNAME ":") + std::to_string(port);
                    auto addr_main_recv = std::string(LIBOTE_SERVER_HOSTNAME ":") + std::to_string(port + 1);
                    auto addr_ole_send  = std::string(LIBOTE_SERVER_HOSTNAME ":") + std::to_string(port + 2);
                    auto addr_ole_recv  = std::string(LIBOTE_SERVER_HOSTNAME ":") + std::to_string(port + 3);
                    
                    sock_main_send = oc::cp::asioConnect(addr_main_send, isServer);
                    sock_main_recv = oc::cp::asioConnect(addr_main_recv, isServer);
                    sock_ole_send  = oc::cp::asioConnect(addr_ole_send, isServer);
                    sock_ole_recv  = oc::cp::asioConnect(addr_ole_recv, isServer);
                } else {
                    auto addr_main_send = std::string(LIBOTE_SERVER_HOSTNAME ":") + std::to_string(port + 1);
                    auto addr_main_recv = std::string(LIBOTE_SERVER_HOSTNAME ":") + std::to_string(port);
                    auto addr_ole_send  = std::string(LIBOTE_SERVER_HOSTNAME ":") + std::to_string(port + 3);
                    auto addr_ole_recv  = std::string(LIBOTE_SERVER_HOSTNAME ":") + std::to_string(port + 2);

                    sock_main_recv = oc::cp::asioConnect(addr_main_recv, isServer);
                    sock_main_send = oc::cp::asioConnect(addr_main_send, isServer);
                    sock_ole_recv  = oc::cp::asioConnect(addr_ole_recv, isServer);
                    sock_ole_send  = oc::cp::asioConnect(addr_ole_send, isServer);
                }
            }

            key_t keyGen() {
                key_t key;
                for (auto& block : key) {
                    block = prng.get<oc::block>();  // generate a random block
                }
                return key;
            }

            ~OPRF() {
                // Flush and close all sockets
                try {
                    oc::cp::sync_wait(sock_main_send.flush());
                    oc::cp::sync_wait(sock_main_send.close());
                
                    oc::cp::sync_wait(sock_main_recv.flush());
                    oc::cp::sync_wait(sock_main_recv.close());
                
                    oc::cp::sync_wait(sock_ole_send.flush());
                    oc::cp::sync_wait(sock_ole_send.close());
                
                    oc::cp::sync_wait(sock_ole_recv.flush());
                    oc::cp::sync_wait(sock_ole_recv.close());
                } catch (const std::exception& e) {
                    // Log error but don't throw from destructor
                    std::cerr << "Error closing OPRF sockets: " << e.what() << std::endl;
                }
                
                // Clear state of sender and receiver
                if (sender) sender->clear();
                if (receiver) receiver->clear();
            }


            template<typename T>
            Vector<T> evaluate_sender(key_t key, int n) {
                // set the key and key OTs
		        std::vector<oc::block> rk(key_size);
                for (oc::u64 i = 0; i < key_size; i++) {
                    rk[i] = oc::block(i, *oc::BitIterator((oc::u8*)&key, i));
                }
                //sender->setKeyOts(key, rk);

                std::vector<oc::block> y(n);
                
                // initialize the OLE generators
                ole.init(sock_ole_send.fork(), prng, !isServer, 1, 1 << 18, 0);
                
                // handle the thread pool, not quite sure what this does
                macoro::thread_pool pool;
                auto e = pool.make_work();  // this line is CRITICAL, even though e is never used (reference counting)
                pool.create_threads(1);

                sock_main_send.setExecutor(pool);
                sock_ole_send.setExecutor(pool);

                // evaluate
                sender->init(n, ole, secJoin::AltModPrfKeyMode::SenderOnly, secJoin::AltModPrfInputMode::ReceiverOnly, key, rk);
                sender->preprocess();

                auto r = coproto::sync_wait(coproto::when_all_ready(
                    ole.start() | macoro::start_on(pool),
                    sender->evaluate({}, y, sock_main_send, prng) | macoro::start_on(pool)
                ));

                oc::cp::sync_wait(sock_main_send.flush());   // flush the socket
                oc::cp::sync_wait(sock_ole_send.flush());   // flush the socket

                // clear the state of the sender
                sender->clear();

                // copy the output into the return vector and return
                Vector<T> ret(n);
                for (int i = 0; i < n; i++) {
                    ret[i] = y[i].get<T>(0);
                }
                return ret;
            }

            template<typename T>
            Vector<T> evaluate_receiver(Vector<T> input) {
                int n = input.size();

                // set the key OTs
        		std::vector<std::array<oc::block, 2>> sk(key_size);
                for (oc::u64 i = 0; i < key_size; i++) {
                    sk[i][0] = oc::block(i, 0);
                    sk[i][1] = oc::block(i, 1);
                }
                //receiver->setKeyOts(sk);

                std::vector<oc::block> y(n);

                // set the input correctly
                std::vector<T> x_vec = input.as_std_vector();
                std::vector<oc::block> x(
                    reinterpret_cast<oc::block*>(x_vec.data()),
                    reinterpret_cast<oc::block*>(x_vec.data() + n)
                );

                // initialize the OLE generators
                ole.init(sock_ole_recv.fork(), prng, !isServer, 1, 1 << 18, 0);
                
                // handle the thread pool, not quite sure what this does
                macoro::thread_pool pool;
                auto e = pool.make_work();  // this line is CRITICAL, even though e is never used (reference counting)
                pool.create_threads(1);

                sock_main_recv.setExecutor(pool);
                sock_ole_recv.setExecutor(pool);
                
                // evaluate
                receiver->init(n, ole, secJoin::AltModPrfKeyMode::SenderOnly, secJoin::AltModPrfInputMode::ReceiverOnly, {}, sk);
                receiver->preprocess();

                auto r = coproto::sync_wait(coproto::when_all_ready(
                    ole.start() | macoro::start_on(pool),
                    receiver->evaluate(x, y, sock_main_recv, prng) | macoro::start_on(pool)
                ));

                oc::cp::sync_wait(sock_main_recv.flush());   // flush the socket
                oc::cp::sync_wait(sock_ole_recv.flush());   // flush the socket
                
                // clear the state of the receiver
                receiver->clear();

                // copy the output into the return vector and return
                Vector<T> ret(n);
                for (int i = 0; i < n; i++) {
                    ret[i] = y[i].get<T>(0);
                }
                return ret;
            }

            template<typename T>
            Vector<T> evaluate_plaintext(std::vector<__int128_t> input, key_t key) {
                int n = input.size();
                std::vector<oc::block> x_vec(
                    reinterpret_cast<oc::block*>(input.data()),
                    reinterpret_cast<oc::block*>(input.data() + input.size())
                ); 
                std::vector<oc::block> y_vec(n);
                std::span<oc::block> x(x_vec), y(y_vec);
                
                secJoin::AltModPrf prf;
                prf.setKey(key);
                prf.eval(x, y);

                std::vector<oc::block> x_out(x.data(), x.data() + x.size());
                std::vector<oc::block> y_out(y.data(), y.data() + y.size());
                
                Vector<T> ret(n);
                for (int i = 0; i < n; i++) {
                    ret[i] = y_out[i].get<T>(0);
                }
                return ret;
            }
    };

} // namespace secrecy::random