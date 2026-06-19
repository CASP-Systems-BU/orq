#pragma once

#include <iomanip>
#include <map>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace orq::benchmarking::stopwatch {
extern int partyID;
}

namespace orq::benchmarking {

// Storage for general benchmark outputs (key-value pairs)
inline std::map<std::string, std::variant<double, int64_t>> general_outputs;

/**
 * @brief Output a general key-value pair to be included in the JSON results.
 *
 * This function allows you to record arbitrary metrics (e.g., accuracy,
 * precision, counts) that will be written to `.experiment.json` alongside
 * stopwatch timing results.
 *
 * @param key The label/key for this output (must not contain quotes)
 * @param value The value to record (supports double, int64_t, or string)
 *
 * @example
 *   orq::benchmarking::output("Accuracy", 0.95);
 *   orq::benchmarking::output("Iterations", 100);
 *   orq::benchmarking::output("Status", "success");
 */
template <typename T>
void output(const std::string& key, const T& value) {
    // Validate key (same validation as stopwatch labels)
    if ((key.find('"') != std::string::npos) || (key.find("'") != std::string::npos)) {
        throw std::invalid_argument("Output key contains a quote character which is not allowed.");
    }

    // Only record on party 0
    if (stopwatch::partyID != 0) {
        return;
    }

    // Store the value (convert to appropriate variant type)
    if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {  // float / double
        general_outputs[key] = static_cast<double>(value);
    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {  // integer
        general_outputs[key] = static_cast<int64_t>(value);
    } else {
        throw std::invalid_argument("Unsupported type for output: " +
                                    std::string(typeid(T).name()));
    }
}

/**
 * @brief Clear all general outputs. Useful for resetting between runs.
 */
inline void clear_outputs() { general_outputs.clear(); }

/**
 * @brief Write general outputs to JSON stream. Used by stopwatch::to_json().
 *
 * @param out Output stream to write to
 * @param first Reference to boolean tracking if this is the first entry
 */
inline void write_outputs_to_json(std::ostream& out, bool& first) {
    for (const auto& kv : general_outputs) {
        if (!first) out << ",";
        first = false;
        out << "\"" << kv.first << "\":";

        // Write value based on variant type
        std::visit(
            [&out](const auto& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, double>) {
                    out << std::fixed << std::setprecision(STOPWATCH_PREC) << val;
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    out << val;
                } else if constexpr (std::is_same_v<T, std::string>) {
                    // Escape quotes in string values
                    out << "\"";
                    for (char c : val) {
                        if (c == '"' || c == '\\') {
                            out << '\\';
                        }
                        out << c;
                    }
                    out << "\"";
                }
            },
            kv.second);
    }
}

}  // namespace orq::benchmarking
