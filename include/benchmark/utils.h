#ifndef SECRECY_BENCHMARKING_UTILS_H
#define SECRECY_BENCHMARKING_UTILS_H

#include <sys/time.h>

#include <vector>

#include "../debug/debug.h"

using sec = std::chrono::duration<float, std::chrono::seconds::period>;
#define PROFILE(EXPR, NAME)                                                         \
    {                                                                               \
        auto t1 = std::chrono::steady_clock::now();                                 \
        (EXPR);                                                                     \
        auto t2 = std::chrono::steady_clock::now();                                 \
        elapsed = sec(t2 - t1).count();                                             \
        single_cout(std::setw(30) << std::left << (NAME " " #EXPR) << "\telapsed\t" \
                                  << std::setw(10) << elapsed);                     \
    }

namespace secrecy::benchmarking::utils {

static void print_bin(const int& num1, const int& num2, bool add_line) {
    std::bitset<32> x(num1);
    std::bitset<32> y(num2);
    std::cout << x << "\t\t" << y << "\t\t";

    if (add_line) {
        std::cout << '\n';
    }
}

template <typename... T>
static void timeTest(void(func)(T...), std::string name, int batch_nums, int size, T... args) {
    struct timeval begin, end;
    long seconds, micro;
    double elapsed;

    // start timer
    gettimeofday(&begin, 0);
    for (int i = 0; i < batch_nums; ++i) {
        func(args...);
    }

    // stop timer
    gettimeofday(&end, 0);
    seconds = end.tv_sec - begin.tv_sec;
    micro = end.tv_usec - begin.tv_usec;
    elapsed = seconds + micro * 1e-6;

    printf("\n%s \tpassed in:\t\t%f\t\t\tThroughput:\t\t%dk\n", name.c_str(), elapsed,
           int(size * batch_nums / elapsed / 1000));
}

template <typename T>
static int duration_to_ms(std::chrono::time_point<T> a, std::chrono::time_point<T> b) {
    return duration_cast<std::chrono::milliseconds>(b - a).count();
}

}  // namespace secrecy::benchmarking::utils

#endif  // SECRECY_BENCHMARKING_UTILS_H