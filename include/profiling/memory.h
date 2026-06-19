#pragma once

/**
 * @file memory.h
 * @brief Stopwatch-like memory checkpoints for the current process.
 *
 * Utilities to query current and peak resident set size (RSS) and a
 * `mempoint(label)` helper that prints formatted checkpoints. Output is restricted
 * to Party 0 for consistency with the stopwatch utilities.
 * All values are displayed in MiB/GiB using 1024-based units.
 *
 * Platform notes:
 * - Linux: reads /proc/self/status (VmRSS for current RSS, VmHWM for peak RSS)
 *   for accurate values.
 * - Other OS: falls back to getrusage; often only peak RSS is available.
 *
 * Party gating:
 * - This header aliases `orq::benchmarking::stopwatch::partyID`, so existing
 *   runtime/communicator initialization automatically controls whether memory
 *   checkpoints print (Party 0 only).
 *
 * Example:
 * @code
 * #include "profiling/memory.h"
 * orq::benchmarking::memory::mempoint("Start");
 * // ... workload that allocates memory ...
 * orq::benchmarking::memory::mempoint("After Load");
 * @endcode
 */

#include <sys/resource.h>
#include <unistd.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "stopwatch.h"

namespace orq::benchmarking::memory {

// alias the stopwatch partyID so gating is automatic wherever stopwatch is set
static int& partyID = orq::benchmarking::stopwatch::partyID;

// 1024-based conversion constant for MiB
inline constexpr double kBytesPerMiB = 1024.0 * 1024.0;

/**
 * @brief Convert bytes to MiB.
 *
 * @param bytes The number of bytes to convert.
 * @return The number of MiB.
 */
inline double bytes_to_mib(long long bytes) noexcept {
    return static_cast<double>(bytes) / kBytesPerMiB;
}

/**
 * @brief Read a kB-valued field from /proc/self/status.
 *
 * Parses the first line whose prefix matches the provided key and returns the
 * numeric value (in kilobytes) as reported by the kernel.
 *
 * @param key The field name prefix to search for (e.g., "VmRSS:" or "VmHWM:").
 * @return size_t The parsed value in kilobytes, or 0 if not found or on error.
 */
static size_t read_value_kb_from_proc_status(const char* key) noexcept {
    std::ifstream status("/proc/self/status");
    if (!status) return 0;

    std::string line;
    const std::string prefix(key);
    while (std::getline(status, line)) {
        if (line.compare(0, prefix.size(), prefix) == 0) {
            std::istringstream iss(line.substr(prefix.size()));
            size_t value_kb = 0;
            iss >> value_kb;  // value reported is in kB
            return value_kb;
        }
    }
    return 0;
}

/**
 * @brief Returns current Resident Set Size (RSS) in bytes.
 */
size_t get_rss_bytes() noexcept {
#if defined(__linux__)
    size_t rss_kb = read_value_kb_from_proc_status("VmRSS:");
    return rss_kb * 1024ULL;
#elif defined(__APPLE__) && defined(__MACH__)
    // Fallback on macOS: ru_maxrss is in bytes
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<size_t>(usage.ru_maxrss);
    }
    return 0;
#else
    // Fallback on other systems: ru_maxrss is in kilobytes
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<size_t>(usage.ru_maxrss) * 1024ULL;
    }
    return 0;
#endif
}

/**
 * @brief Returns peak Resident Set Size (high-water mark) in bytes.
 */
size_t get_peak_rss_bytes() noexcept {
#if defined(__linux__)
    size_t hwm_kb = read_value_kb_from_proc_status("VmHWM:");
    if (hwm_kb) return hwm_kb * 1024ULL;
    // Fallback to getrusage if VmHWM not available (kilobytes on Linux)
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<size_t>(usage.ru_maxrss) * 1024ULL;
    }
    return 0;
#elif defined(__APPLE__) && defined(__MACH__)
    // On macOS, ru_maxrss is in bytes
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<size_t>(usage.ru_maxrss);
    }
    return 0;
#else
    // On other systems, ru_maxrss is typically in kilobytes
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<size_t>(usage.ru_maxrss) * 1024ULL;
    }
    return 0;
#endif
}

/**
 * @brief Prints a memory checkpoint with label, showing current RSS, delta since
 * last checkpoint, and peak RSS. Mirrors stopwatch-style formatting.
 *
 * @param label The label to print.
 */
void mempoint(const std::string& label) {
    static size_t last_rss = 0;
    static bool first_time = true;

    if (partyID != 0) {
        return;
    }

    size_t rss_bytes = get_rss_bytes();
    size_t peak_bytes = get_peak_rss_bytes();

    if (first_time) {
        std::cout << "[=MEM] " << std::setw(LABEL_WIDTH) << label << "\n";
        first_time = false;
        last_rss = rss_bytes;
        return;
    }

    double rss_mib = bytes_to_mib(rss_bytes);
    long long delta_bytes = static_cast<long long>(rss_bytes) - static_cast<long long>(last_rss);
    double delta_mib = bytes_to_mib(delta_bytes);
    double peak_mib = bytes_to_mib(peak_bytes);

    // Choose MiB or GiB units based on magnitude (>= 1024 MiB -> GiB)
    double cur_val = (rss_mib >= 1024.0) ? (rss_mib / 1024.0) : rss_mib;
    const char* cur_unit = (rss_mib >= 1024.0) ? "GiB" : "MiB";

    double del_abs_mib = std::fabs(delta_mib);
    double del_val = (del_abs_mib >= 1024.0) ? (del_abs_mib / 1024.0) : del_abs_mib;
    const char* del_unit = (del_abs_mib >= 1024.0) ? "GiB" : "MiB";
    const char* del_sign = (delta_mib >= 0.0) ? "+" : "-";

    double peak_val = (peak_mib >= 1024.0) ? (peak_mib / 1024.0) : peak_mib;
    const char* peak_unit = (peak_mib >= 1024.0) ? "GiB" : "MiB";

    std::cout << "[ MEM] " << std::setw(LABEL_WIDTH) << std::right << label << " " << std::fixed
              << std::setprecision(2) << std::setw(8) << std::left << cur_val << " " << cur_unit
              << "  (" << del_sign << std::fixed << std::setprecision(2) << del_val << " "
              << del_unit << ")" << "  [peak: " << std::fixed << std::setprecision(2) << peak_val
              << " " << peak_unit << "]\n";

    last_rss = rss_bytes;
}

}  // namespace orq::benchmarking::memory