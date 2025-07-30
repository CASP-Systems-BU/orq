#pragma once

#include <unistd.h>

#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>

// TODO: maybe move these to utility file or something

// Function to execute shell commands and capture output
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string prependHash(const std::string& str) {
    std::string result;
    std::istringstream stream(str);
    std::string line;
    while (std::getline(stream, line)) {
        result += "# " + line + "\n";
    }
    return result;
}

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

std::string hostname() {
    char hn[HOST_NAME_MAX];
    int ret = gethostname(hn, HOST_NAME_MAX);
    if (ret != 0) {
        std::cerr << "error: could not get hostname (" << ret << ")\n";
        exit(1);
    }
    return std::string(hn);
}

namespace secrecy::instrumentation::thread_stopwatch {
/**
 * @brief Map from thread ID to timing information. Switching to atomic u64 for
 * communication timing purposes only.
 *
 */
std::map<int64_t, std::atomic_uint64_t> timing;

uint64_t get_now_ns() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}

// NOTE: Format of timing store has changed, so `write` not currently supported.
// We can replace this later as we considered better ways to instrument the code

// void write(int pid = 0) {
//     if (pid != 0) {
//         return;
//     }

//     if (timing.empty()) {
//         std::cerr << "No events recorded!\n";
//         return;
//     }

//     auto host = hostname();

//     auto filename = "thread-" + std::to_string(getpid()) + "-out.csv";
//     std::ofstream csv(filename);
//     if (!csv.is_open()) {
//         std::cerr << "Could not open csv file " << filename << " for thread timeline output\n";
//         return;
//     }

//     // output meta
//     csv << "# host: " << host << std::endl;
//     csv << "# nproc: " << exec("nproc");
//     csv << "# protocol: " << PROTOCOL_NUM << std::endl;

//     csv << "# cmdline: ";
//     std::ifstream cmdlineFile("/proc/self/cmdline", std::ios::binary);
//     if (cmdlineFile) {
//         std::vector<char> cmdline((std::istreambuf_iterator<char>(cmdlineFile)),
//                                   std::istreambuf_iterator<char>());

//         // Replace all null characters with spaces
//         std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');

//         // Print the modified cmdline string
//         csv << std::string(cmdline.begin(), cmdline.end()) << std::endl;
//     } else {
//         csv << "unavailable" << std::endl;
//     }

//     try {
//         std::string gitCommit = exec("git rev-parse HEAD");
//         std::string gitBranch = exec("git rev-parse --abbrev-ref HEAD");
//         std::string gitStatus = exec("git status --porcelain");

//         csv << "# branch: " << gitBranch;
//         csv << "# commit: " << gitCommit;

//         if (gitStatus.empty()) {
//             csv << "# status: clean" << std::endl;
//         } else {
//             csv << "# status: " << std::endl;
//             csv << prependHash(gitStatus);
//         }
//     } catch (const std::exception& e) {
//         csv << "# git failure: " << e.what() << std::endl;
//     }

//     csv << "#\ntid,t_start,elapsed,meta\n";

//     for (const auto& [tid, deq] : timing) {
//         for (const auto& [ts, te, m] : deq) {
//             csv << tid << "," << ts << "," << te << "," << m << "\n";
//         }
//     }

//     csv.close();

//     std::cout << host << ": wrote to " << filename << "\n";
// }

double get_aggregate_comm(int pid = 0) {
    if (pid != 0) return 0;

    uint64_t total_ns = 0;

    // RACE CONDITION:
    // Worker threads update their `InstrumentBlock`s just before returning,
    // but the destructor is called *after* the the relevant barrier returns.
    // Therefore, the main thread might return from the runtime (and call this
    // function) before a thread has finished updating, and start iterating
    // through a map which is about to be updated.
    //
    // I "fixed" this for now by removing instrumentation from the runtime: we
    // only care about communication for this test.

    for (auto& [tid, u] : timing) {
        // increment our counter and reset `u` to 0.
        total_ns += u.exchange(0);
    }
    
    return (double) total_ns / 1e9;
}

void init_map(std::thread::id tid) {
    timing[std::hash<std::thread::id>{}(tid)] = 0;
}

class InstrumentBlock {
    const int64_t tid;
    const uint64_t start;
    uint64_t end;

    const std::string meta;

   public:
#ifdef INSTRUMENT_THREADS
    InstrumentBlock(int64_t tid, const std::string& meta = "")
        : tid(tid), start(get_now_ns()), meta(meta) {}

    /**
     * @brief Auto-ID assigning constructor. Use the hash of the thread ID:
     * this is meaningless but guaranteed to be unique.
     *
     * @param meta optional metadata for this timepoint.
     */
    InstrumentBlock(const std::string& meta = "")
        : tid(std::hash<std::thread::id>{}(std::this_thread::get_id())),
          start(get_now_ns()),
          meta(meta) {}

    ~InstrumentBlock() {
        end = get_now_ns();
        // timing[tid].push_back({start, end - start, meta});
        timing[tid] += end - start;
    }
#else
    template <typename... T>
    InstrumentBlock(T... args) : tid(0), start(0) {}

#endif
};
}  // namespace secrecy::instrumentation::thread_stopwatch