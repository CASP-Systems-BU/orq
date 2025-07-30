#ifndef SECRECY_BENCHMARKING_STOPWATCH
#define SECRECY_BENCHMARKING_STOPWATCH

#include <chrono>

#include "../debug/debug.h"

#define LABEL_WIDTH 16
#define STOPWATCH_PREC 4

namespace secrecy::benchmarking::stopwatch {

using sec = std::chrono::duration<float, std::chrono::seconds::period>;

int partyID = 0;

std::chrono::steady_clock::time_point _tp_first;

// profiling
static std::map<std::string, double> profile_times;
static std::map<std::string, double> preproc_times;
static std::map<std::string, double> comm_times;

static std::chrono::steady_clock::time_point profile_last;
static std::chrono::steady_clock::time_point preproc_last;

void timepoint(std::string label) {
    static std::chrono::steady_clock::time_point then, now;
    static bool first_time = true;

    if (partyID != 0) {
        return;
    }

    if (first_time) {
        _tp_first = then = now = std::chrono::steady_clock::now();
        std::cout << "[=SW] " << std::setw(LABEL_WIDTH) << label << "\n";
        first_time = false;
        return;
    }

    now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<sec>(now - then).count();
    std::cout << "[ SW] " << std::setw(LABEL_WIDTH) << std::right << label << " "
              << std::setprecision(STOPWATCH_PREC) << std::setw(STOPWATCH_PREC + 4) << std::left
              << elapsed << " sec\n";
    then = now;
};

// get the elapsed time since the last timepoint
float get_elapsed() {
    static std::chrono::steady_clock::time_point then, now;
    static bool first_time = true;

    if (partyID != 0) {
        return 0;
    }

    if (first_time) {
        _tp_first = then = now = std::chrono::steady_clock::now();
        first_time = false;
        return 0.0f;
    }

    now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<sec>(now - then).count();
    then = now;
    return elapsed;
};

void done() {
    if (partyID != 0) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<sec>(now - _tp_first).count();
    std::cout << "[=SW] " << std::setw(LABEL_WIDTH) << std::right << "Overall" << " "
              << std::setprecision(STOPWATCH_PREC) << std::setw(STOPWATCH_PREC + 4) << std::left
              << elapsed << " sec\n";
}

void profile_init() {
    profile_times.clear();
    preproc_times.clear();
    comm_times.clear();
    profile_last = std::chrono::steady_clock::now();
    preproc_last = std::chrono::steady_clock::now();
}

void profile_timepoint(std::string label) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<sec>(now - profile_last).count();
    profile_times[label] += elapsed;
    profile_last = now;
}

void profile_preprocessing(std::optional<std::string> label = {}) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<sec>(now - preproc_last).count();

    if (label) {
        // for convenient bookkeeping.
        profile_times["PREPROCESSING"] += elapsed;
        preproc_times[*label] += elapsed;
    }

    preproc_last = now;
}

void profile_comm(std::string label, double t) { comm_times[label] += t; }

void profile_done() {
    if (partyID != 0) {
        return;
    }

    double total = 0.0;

    bool print_online = false;

    for (auto& [label, time] : preproc_times) {
        std::cout << "[=PREPROC] " << std::setw(LABEL_WIDTH) << std::right << label << " "
                  << std::setprecision(STOPWATCH_PREC) << std::setw(STOPWATCH_PREC + 4) << std::left
                  << time << " sec\n";
    }

    for (auto& [label, time] : profile_times) {
        if (label == "PREPROCESSING") {
            print_online = true;
            continue;
        }
        std::cout << "[=PROFILE] " << std::setw(LABEL_WIDTH) << std::right << label << " "
                  << std::setprecision(STOPWATCH_PREC) << std::setw(STOPWATCH_PREC + 4) << std::left
                  << time << " sec\n";
        total += time;
    }

    if (print_online) {
        // Print the total online time
        std::cout << "[=PROFILE] " << std::setw(LABEL_WIDTH) << std::right << "TotalOnline "
                  << std::setprecision(STOPWATCH_PREC) << std::setw(STOPWATCH_PREC + 4) << std::left
                  << total - profile_times["PREPROCESSING"] << " sec\n";
    }

    for (auto& [label, time] : comm_times) {
        std::cout << "[=COMM] " << std::setw(LABEL_WIDTH) << std::right << label << " "
                  << std::setprecision(STOPWATCH_PREC) << std::setw(STOPWATCH_PREC + 4) << std::left
                  << time << " sec\n";
    }
}
}  // namespace secrecy::benchmarking::stopwatch

#endif