#pragma once

/**
 * @brief Numeric codes for each protocol
 *
 */

#define FANTASTIC4 4
#define REPLICATED3 3
#define BEAVER2 2
#define PLAINTEXT1 1
#define DUMMY0 0

/**
 * @brief The default protocol
 *
 */
#ifndef PROTOCOL_NUM
#define PROTOCOL_NUM REPLICATED3
#endif

#define MPI_COMMUNICATOR 1
#define NOCOPY_COMMUNICATOR 4

/**
 * @brief The default communicator
 *
 */
#ifndef COMMUNICATOR_NUM
#define COMMUNICATOR_NUM MPI_COMMUNICATOR
#endif

/**
 * @brief If SINGLE_EXECUTION is defined, don't compile with a communicator. (Useful for debugging)
 *
 */
#ifndef SINGLE_EXECUTION
#if COMMUNICATOR_NUM == MPI_COMMUNICATOR
#define MPC_USE_MPI_COMMUNICATOR 1
#elif COMMUNICATOR_NUM == NOCOPY_COMMUNICATOR
#define MPC_USE_NO_COPY_COMMUNICATOR 1
#endif
#endif

#define MPC_USE_RANDOM_GENERATOR_TRIPLES 1

////////////////////////////////////////
////// Socket Communicator Macros //////
// #define STARTMPC_VERBOSE
// #define SOCKET_COMMUNICATOR_VERBOSE

#define SOCKET_COMMUNICATOR_RING_SIZE 65536UL
#define SOCKET_COMMUNICATOR_BUFFER_SIZE 65536UL  // bytes
#define SOCKET_COMMUNICATOR_WAIT_MS 10

/**
 * @brief Current communicaton API only uses 1 ring element, ring_size can be changed later if
 * needed
 *
 */
#define NOCOPY_COMMUNICATOR_RING_SIZE 16UL
#ifndef NOCOPY_COMMUNICATOR_THREADS
#define NOCOPY_COMMUNICATOR_THREADS -1  // Set to '-1' to use 1 comm thread per ORQ thread
#endif

#define MPC_GENERATE_DATA 1
#define MPC_RANDOM_DATA_RANGE 100
#define MPC_USE_RANDOM_GENERATOR_DATA 1
#define MPC_EVALUATE_CORRECT_OUTPUT 1
#define MPC_CHECK_CORRECTNESS 1

// #define MPC_PRINT_RESULT 1
// #define MPC_COMMUNICATOR_PRINT_DATA 1

// #define USE_PARALLEL_PREFIX_ADDER 1
#define USE_DIVISION_CORRECTION 1
// #define RECYCLE_THREAD_MEMORY 1
#define DEBUG_VECTOR_SAME_AS 1

/**
 * @brief Whether we should run with a LAN (default) or WAN configuration. This currently affects
 * the default batch size and adder circuit, but we may make future changes.
 *
 */
// #define WAN_CONFIGURATION

#ifdef WAN_CONFIGURATION
// In WAN, we use PPA since it has fewer rounds than RCA.
#define USE_PARALLEL_PREFIX_ADDER
#endif

/**
 * @brief If defined, Vectors use index mapping to track access patterns instead of VectorData
 *
 */
#define USE_INDEX_MAPPED_VECTOR

// Print intermediate results of boolean long division
// #define DEBUG_DIVISION
// Collect Thread Instrumentation info
// #define INSTRUMENT_THREADS

// If defined, used boolean subtraction (RCA) to compare values within quicksort
// #define QUICKSORT_USE_SUBTRACTION_CMP

// Whether we should instrument table operations (output sort/agg statistics)
// #define INSTRUMENT_TABLES

// Skip actual table operations, just so we can check intermediate sizes
// Since table ops (sort, agg, joint) are the slowest, skipping them will make
// debug executions a lot faster. Of course, results will be totally incorrect.
// #define DEBUG_SKIP_EXPENSIVE_TABLE_OPERATIONS

/**
 * @brief Whether to use the original protocol of Dalskov et al., or our custom version
 *
 */
#define USE_DALSKOV_FANTASTIC_FOUR

#ifdef DEBUG_SKIP_EXPENSIVE_TABLE_OPERATIONS
#warning DEBUG_SKIP_EXPENSIVE_TABLE_OPERATIONS enabled!
#endif

// whether to use a seeded PRG (AES; CommonPRG) or /dev/random as the local PRG
// Turn this option on to make it easier to debug random errors.
// #define USE_SEEDED_PRG

/**
 * @brief To run on a multi-node cluster, set this define to be the cluster-routable
 * hostname of the server (party 0). For example, on CloudLab, this should be `node0`
 *
 * NOTE: if 2PC with real triples is frozen, this is probably why.
 *
 */
#ifndef LIBOTE_SERVER_HOSTNAME
#define LIBOTE_SERVER_HOSTNAME "localhost"
#endif

/**
 * @brief Print usage statistics for an MPC protocol
 *
 */
#define PRINT_PROTOCOL_STATISTICS

// Print communicator statistics
// #define PRINT_COMMUNICATOR_STATISTICS

/**
 * @brief Preprocessor defines to generate proper namespaces
 *
 */
#ifdef MPC_USE_MPI_COMMUNICATOR
#define SERVICE_NAMESPACE orq::service::mpi_service
#elif MPC_USE_NO_COPY_COMMUNICATOR
#define SERVICE_NAMESPACE orq::service::nocopy_service
#endif

#if PROTOCOL_NUM == FANTASTIC4
#define MPC_PROTOCOL_FANTASTIC_FOUR
#define MALICIOUS_PROTOCOL
#define COMPILED_MPC_PROTOCOL_NAMESPACE SERVICE_NAMESPACE::fantastic_4pc
#elif PROTOCOL_NUM == REPLICATED3
#define MPC_PROTOCOL_REPLICATED_THREE
#define COMPILED_MPC_PROTOCOL_NAMESPACE SERVICE_NAMESPACE::replicated_3pc
#elif PROTOCOL_NUM == BEAVER2
#define MPC_PROTOCOL_BEAVER_TWO
#define COMPILED_MPC_PROTOCOL_NAMESPACE SERVICE_NAMESPACE::beaver_2pc
#elif PROTOCOL_NUM == PLAINTEXT1
#define MPC_PROTOCOL_PLAINTEXT_ONE
// No communicator for plaintext one-party protocol.
#undef MPC_USE_MPI_COMMUNICATOR

#undef SERVICE_NAMESPACE
#define SERVICE_NAMESPACE orq::service::null_service

#define COMPILED_MPC_PROTOCOL_NAMESPACE SERVICE_NAMESPACE::plaintext_1pc
#elif PROTOCOL_NUM == DUMMY0
#define MPC_PROTOCOL_DUMMY_ZERO
#undef MPC_USE_MPI_COMMUNICATOR
#define ZEROPC_DUMMY_VECTOR

#undef SERVICE_NAMESPACE
#define SERVICE_NAMESPACE orq::service::null_service

#define COMPILED_MPC_PROTOCOL_NAMESPACE SERVICE_NAMESPACE::dummy_0pc
#else
#error Invalid protocol.
#endif

#include <algorithm>
#include <bitset>
#include <cassert>
#include <chrono>
#include <climits>
#include <cmath>
#include <iomanip>
#include <ios>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Print `x` on party zero, only, with a newline
 *
 */
#define single_cout(x)                                                        \
    {                                                                         \
        if (orq::service::runTime->getPartyID() == 0) std::cout << x << "\n"; \
    }

/**
 * @brief Print `x` on all parties, prepending with party ID
 *
 */
#define all_cout(x)                                                                    \
    {                                                                                  \
        std::cout << "[P" << orq::service::runTime->getPartyID() << "] " << x << "\n"; \
    }

/**
 * @brief Print `x` on party zero, only, without a newline
 *
 */
#define single_cout_nonl(x)                                           \
    {                                                                 \
        if (orq::service::runTime->getPartyID() == 0) std::cout << x; \
    }

/**
 * @brief Pass a variable's name and value to `std::cout`. Use like:
 * ```c++
 * std::cout << VAR(x) << VAR(y) << "\n";
 * ```
 *
 * This will output something like
 * ```
 * x = 3 y = 10
 * ```
 *
 */
#define VAR(x) (#x " = ") << x << " "

#define VECTOR_SPACING 6
#define TABLE_SPACING 14

#define DEBUG_PRINT(x) orq::debug::print(#x, x)

/**
 * @brief Compiler hack to print a type via compiler error. Just do print_type<X>.
 * Useful for figuring out complex type aliases.
 * @tparam typename
 */
template <typename>
struct print_type;

/**
 * @brief Custom 128-bit handler for std::cout
 *
 * @param os
 * @param n
 * @return std::ostream&
 */
std::ostream& operator<<(std::ostream& os, __int128 n) {
    if (n == 0) {
        return os << "0";
    }

    if (n < 0) {
        os << "-";
        n = -n;
    }

    std::string result;
    while (n > 0) {
        result += '0' + (n % 10);
        n /= 10;
    }

    std::reverse(result.begin(), result.end());
    return os << result;
}

namespace orq::debug {

/**
 * @brief Print numbers in binary
 *
 * @param num1
 * @param num2
 * @param add_line
 * @param party_num
 */
static void print_bin_(const int& num1, const int& num2, bool add_line, int party_num = 0) {
    if (party_num == 0) {
        std::bitset<32> x(num1);
        std::bitset<32> y(num2);
        std::cout << x << "\t\t" << y << "\t\t";

        if (add_line) {
            std::cout << std::endl;
        }
    }
}

/**
 * @brief Print a vector on a single party
 *
 * @tparam VectorType
 * @param vec
 * @param partyID party to output, default P0
 */
template <typename VectorType>
static void print(VectorType&& vec, const int& partyID = 0) {
    // Get the mask value for this type.
    using T = std::remove_reference_t<decltype(vec[0])>;
    const T MASK_VALUE = std::numeric_limits<T>::max();

    std::cout << std::setfill(' ');

    if (partyID == 0) {
        for (size_t i = 0; i < vec.size(); ++i) {
            std::cout << std::right << " " << std::setw(VECTOR_SPACING);
            if (vec[i] == MASK_VALUE) {
                std::cout << "~";
            } else {
                if constexpr (std::is_same<T, int8_t>::value) {
                    std::cout << (int32_t)vec[i];
                } else {
                    std::cout << vec[i];
                }
            }
        }
        std::cout << std::endl;
    }
}

/**
 * @brief Print a vector, prepended with a label
 *
 * @tparam T
 * @param name
 * @param vec
 */
template <typename T>
static void print(std::string name, T&& vec) {
    std::cout << name << " = ";
    print(vec);
}

/**
 * @brief Print a plaintext table.
 *
 * @tparam TableType can be any vector<vector> or similar
 * @param table
 * @param partyID
 */
template <typename TableType>
static void print_table(const TableType& table, const int& partyID = 0) {
    if (partyID == 0) {
        std::cout << "################################################################"
                  << std::endl;
        std::cout << "################################################################"
                  << std::endl;
        for (size_t i = 0; i < table.size(); ++i) {
            print(table[i], partyID);
        }
        std::cout << "################################################################"
                  << std::endl;
        std::cout << "################################################################"
                  << std::endl;
    }
}

/**
 * @brief Improved table output. Call with `print_table(T.open_with_schema())`.
 * (Once plaintext tables are implemented, they will replace the current
 * hacky version.
 *
 * @tparam TableType
 * @param table
 * @param partyID
 */
template <typename TableType>
static void print_table(const std::pair<TableType, std::vector<std::string>>& table,
                        const int& partyID = 0) {
    if (partyID != 0) {
        return;
    }

    auto data = table.first;
    auto labels = table.second;
    auto num_columns = data.size();
    const int width = num_columns * TABLE_SPACING;

    std::cout << std::right;
    std::cout << std::string(width, '/') << std::endl;

    // Get the mask value for the table.
    // NOTE: if we ever support tables with differently-typed columns,
    // this type assignment should somehow become part of the loop.
    using T = std::remove_reference_t<decltype(data[0][0])>;
    const T MASK_VALUE = std::numeric_limits<T>::max();

    for (int i = 0; i < num_columns; ++i) {
        std::cout << std::setw(TABLE_SPACING) << labels[i];
    }

    std::cout << std::endl << std::string(width, '-') << std::endl;

    for (size_t j = 0; j < data[0].size(); j++) {
        for (int i = 0; i < num_columns; i++) {
            std::cout << std::setw(TABLE_SPACING);
            if (data[i][j] == MASK_VALUE) {
                std::cout << "~";
            } else {
                std::cout << (int64_t)data[i][j];
            }
        }
        std::cout << "\n";
    }

    std::cout << std::string(width, '-') << std::endl;
    for (int i = 0; i < num_columns; ++i) {
        std::cout << std::setw(TABLE_SPACING) << labels[i];
    }
    std::cout << "\n";

    auto prefix =
        std::string(width / 4, '\\') + " " + std::to_string((int)data[0].size()) + " rows ";
    const int pre_sz = prefix.size();
    std::cout << prefix << std::string(std::max(0, width - pre_sz), '\\') << std::endl;
    std::cout << std::string(width, '\\') << std::endl << std::endl;

    std::cout << std::left;
}

template <typename T>
T get_bit(const T& s, int i) {
    using Unsigned_type = typename std::make_unsigned<T>::type;
    return ((Unsigned_type)s >> i) & (Unsigned_type)1;
}

/**
 * Prints to stdout the binary representation of `s`.
 * @tparam T The data type of the input `s`
 * @param s The input whose binary representation we want to print.
 */
template <typename T>
void print_binary(const T& s) {
    // NOTE: Rounding is needed because signed share types have one digit less.
    static const int MAX_BITS_NUMBER =
        std::pow(2, std::ceil(std::log2(std::numeric_limits<T>::digits)));
    char bits[MAX_BITS_NUMBER + 1];  // +1 for the final '\0'
    bits[MAX_BITS_NUMBER] = '\0';
    for (int i = 0; i < MAX_BITS_NUMBER; i++) {
        if (get_bit(s, i) == 1) {
            bits[MAX_BITS_NUMBER - i - 1] = '1';
        } else {
            bits[MAX_BITS_NUMBER - i - 1] = '0';
        }
    }
    std::cout << bits << "\t";
}

/**
 * Prints to stdout the binary representation of the elements in `v`.
 * @tparam VectorType The vector type.
 * @param v The input vector.
 * @param partyID The ID of the party that calls this function.
 */
template <typename VectorType>
void print_binary(const VectorType& v, int partyID) {
    std::cout << "[" << partyID << "]: ";
    for (size_t i = 0; i < v.size(); i++) print_binary(v[i]);
    std::cout << std::endl;
}

/**
 * @brief Print the keys in map `m` separated by spaces
 *
 * @tparam K
 * @tparam V
 * @param m
 */
template <typename K, typename V>
void print_map_keys(std::map<K, V> m) {
    for (auto k : m) {
        std::cout << k.first << " ";
    }
    std::cout << "\n";
}

/**
 * @brief Convert any container to string, enclosing it in curly brackets. Works on vectors, sets,
 * etc.
 *
 * @tparam T
 * @param S
 * @return std::string
 */
template <typename T>
std::string container2str(const T& S) {
    std::string out = "{";

    for (const auto& val : S) {
        out += std::to_string(val) + " ";
    }

    if (out.size() == 1) {
        out += '}';
    } else {
        out.back() = '}';
    }
    return out;
}

}  // namespace orq::debug
