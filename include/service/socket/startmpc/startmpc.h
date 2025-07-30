#pragma once

#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../network_utils.h"

enum ExecMode { LOCAL_EXEC_MODE = 0, REMOTE_EXEC_MODE = 1 };

template <typename... Args>
void startmpc_print(Args&&... args) {
#if defined(STARTMPC_VERBOSE)
    (std::cout << ... << args) << std::endl;
#endif
}

/**
 * @brief Create an outgoing connection and assign it to the socket map.
 * Each thread runs this function (along with `listen_connections`)
 * concurrently, but since threads access disjoint vector indices (and no host
 * sends and listens on the same port), this is thread safe.
 *
 * The prior version of these functions used a `vector<map<...>>`, which lead
 * to a concurrent-write race condition when multiple threads tried to write to
 * the map at the same time.
 *
 * TODO: socket_maps should probably be a `shared_ptr`, eventually.
 *
 * @param host_rank
 * @param to_rank
 * @param thread_num
 * @param ip_addr
 * @param connect_start_port
 * @param socket_maps
 */
void send_connections(int host_rank, int to_rank, int thread_num, std::string ip_addr,
                      int connect_start_port, std::vector<std::vector<int>>* socket_maps) {
    // Create a socket connection for each thread
    for (int i = 0; i < thread_num; i++) {
        int connect_port = connect_start_port + i;
        startmpc_print("Host ", host_rank, " | Thread ", i, " | Sending to: ", to_rank, " on ",
                       connect_port);

        assert(socket_maps->size() > i && (*socket_maps)[i].size() > to_rank);
        (*socket_maps)[i][to_rank] = socket_connect(ip_addr, connect_port);
    }
}

/**
 * @brief Listen for an incoming connection and assign it to the socket map. See
 * `send_connections` for more details.
 *
 * @param host_rank
 * @param from_rank
 * @param thread_num
 * @param listen_start_port
 * @param socket_maps
 */
void listen_connections(int host_rank, int from_rank, int thread_num, int listen_start_port,
                        std::vector<std::vector<int>>* socket_maps) {
    // Create a socket connection for each thread
    for (int i = 0; i < thread_num; i++) {
        int listen_port = listen_start_port + i;
        startmpc_print("Host ", host_rank, " | Thread ", i, " | Listening from: ", from_rank,
                       " on ", listen_port);

        int listen_sockfd = socket_create(listen_port);
        listen(listen_sockfd, 0);

        assert(socket_maps->size() > i && (*socket_maps)[i].size() > from_rank);
        (*socket_maps)[i][from_rank] = accept(listen_sockfd, NULL, NULL);
    }
}

void startmpc_init(int* rank, int protocol_count, int thread_num,
                   std::vector<std::vector<int>>& socket_maps) {
    const char* startmpc_exec_env = std::getenv("STARTMPC_EXEC_MODE");
    const char* host_count_env = std::getenv("STARTMPC_HOST_COUNT");
    const char* host_rank_env = std::getenv("STARTMPC_HOST_RANK");
    const char* base_port_env = std::getenv("STARTMPC_BASE_PORT");

    int startmpc_exec_mode = startmpc_exec_env ? std::atoi(startmpc_exec_env) : -1;
    int host_count = host_count_env ? std::atoi(host_count_env) : -1;
    int host_rank = host_rank_env ? std::atoi(host_rank_env) : -1;

    // Arbitrary start port
    // Range of ports used: base_port -> (base_port + host_count * host_count)
    int base_port = base_port_env ? std::atoi(base_port_env) : -1;

    // Fill in IP address list
    std::vector<std::string> ip_addr_list;
    if (startmpc_exec_mode == REMOTE_EXEC_MODE) {
        const char* host_list_env = std::getenv("STARTMPC_HOST_LIST");
        std::stringstream ss(host_list_env);
        while (ss.good()) {
            std::string ip_addr;
            getline(ss, ip_addr, ',');
            ip_addr_list.push_back(ip_addr);
        }
    } else if (startmpc_exec_mode == LOCAL_EXEC_MODE) {
        ip_addr_list.assign(host_count, "127.0.0.1");
    }
    if (ip_addr_list.size() != host_count)
        throw std::runtime_error("startmpc_init: Invalid ip_addr_list created");

    if (protocol_count != host_count) {
        std::string msg =
            "Invalid host count provided for " + std::to_string(protocol_count) + "pc";
        throw std::runtime_error(msg);
    }

    startmpc_print("Rank: ", host_rank, " Count: ", host_count);

    std::vector<std::thread> connection_threads;

    // Send connection attempt to parties with a higher host_rank
    for (int i = host_rank + 1; i < host_count; i++) {
        // Start port for connections to party i
        int connect_start_port =
            base_port + (host_count * thread_num * host_rank) + (thread_num * i);

        connection_threads.emplace_back(send_connections, host_rank, i, thread_num, ip_addr_list[i],
                                        connect_start_port, &socket_maps);
    }

    // Listen for incoming connections from parties with a lower host_rank
    for (int i = 0; i < host_rank; i++) {
        // Start port for connections from party i
        int listen_start_port =
            base_port + (host_count * thread_num * i) + (thread_num * host_rank);

        connection_threads.emplace_back(listen_connections, host_rank, i, thread_num,
                                        listen_start_port, &socket_maps);
    }

    for (auto& thread : connection_threads) {
        if (thread.joinable()) thread.join();
    }

    if (rank != nullptr) {
        *rank = host_rank;
    }

    secrecy::benchmarking::stopwatch::partyID = *rank;
}
