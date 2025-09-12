#pragma once

#include <chrono>

#include "debug/orq_debug.h"

#define LABEL_WIDTH 16
#define STOPWATCH_PREC 4

namespace orq::benchmarking::stopwatch {

using sec = std::chrono::duration<float, std::chrono::seconds::period>;

int partyID = 0;

std::chrono::steady_clock::time_point _tp_first;

// profiling
static std::map<std::string, double> profile_times;
static std::map<std::string, double> preproc_times;
static std::map<std::string, double> comm_times;

static std::chrono::steady_clock::time_point profile_last;
static std::chrono::steady_clock::time_point preproc_last;

/**
 * @brief Mark a timepoint with the given label and output the elapsed time on
 * Party 0. If this is the first time `timepoint` has been called, just print
 * the label, and start the clock. Otherwise, outut the time since the last
 * timepoint.
 *
 * @param label
 */
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

/**
 * @brief Get the elapsed time without printing. Still registers intervals in
 * the manner of `timepoint`. This means that interleaved calls to `timepoint`
 * and `get_elapsed` will return the time elapsed since either was last called.
 *
 * @return float
 */
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

/**
 * @brief Output the time elapsed since the _first_ call to `timepoint` or
 * `get_elapsed`. This is useful to call at the end of a program to see how long
 * the entire execution takes.
 *
 * `done` can be called multiple times; it will always output the elapsed time
 * since the same initial timepoint.
 *
 */
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

/**
 * @brief Initialized the profiler.
 *
 * ORQ provides a primitive profiling utility based on the stopwatch which
 * simply aggregates elapsed times registered under each label. The profiler
 * should not be used when high-accuracy measurements are needed, but it is
 * sufficient for simple tests and benchmarks.
 */
void profile_init() {
    profile_times.clear();
    preproc_times.clear();
    comm_times.clear();
    profile_last = std::chrono::steady_clock::now();
    preproc_last = std::chrono::steady_clock::now();
}

/**
 * @brief Register a profile timepoint under `label`. Semantics are similar to
 * `timepoint`, but nothing is printed.
 *
 * @param label
 */
void profile_timepoint(std::string label) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<sec>(now - profile_last).count();
    profile_times[label] += elapsed;
    profile_last = now;
}

/**
 * @brief Register a preprocessing timepoint with an optional label. If no label
 * is provided, update the last timepoint but do not measure the elapsed time.
 * If a label is provided, measure the elapsed time for both this label and the
 * special `PREPROCESSING` symbol.
 *
 * @param label
 */
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

/**
 * @brief Register a given interval for the given communication category. This
 * function behaves differently due to architectural differences in measuring
 * compute versus communication, but (TODO) should probably be updated.
 *
 * @param label
 * @param t time in seconds
 */
void profile_comm(std::string label, double t) { comm_times[label] += t; }

/**
 * @brief Complete profiling and output a profiling report. Prints each category
 * of aggregated times and separates out preprocessing. Also prints out a
 * breakdown of offline versus online time.
 *
 */
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
}  // namespace orq::benchmarking::stopwatch
