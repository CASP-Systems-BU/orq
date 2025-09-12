#include <cmath>  // For log and ceil

#include "orq.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// command
// mpirun -np 3 ./micro_sort_preprocessing 1 1 8192 $ROWS

int main(int argc, char** argv) {
    orq_init(argc, argv);
#ifndef MPC_PROTOCOL_BEAVER_TWO
    single_cout("Skipping test_correlated for non-2PC");
#else

    auto pID = runTime->getPartyID();
    int test_size = 1000000;
    if (argc >= 5) {
        test_size = atoi(argv[4]);
    }

    int bitwidth = 64;

    // radix sort

    // Ln multiplications + Ln b2a_bit = 2Ln multiplications (32 bits, perms only)
    int num_mul_triples = 2 * bitwidth * test_size;

    // quicksort

    // scaling constant
    double quicksort_constant = 2.0;

    double nlogn = test_size * std::log2(test_size);
    int num_comparisons = quicksort_constant * static_cast<int>(std::ceil(nlogn));  // round up
    // handle the case of very small input (n < 2000) by addig an additive buffer
    if (test_size < 2000) {
        num_comparisons += 10000;
    }

    int comparison_bitwidth = bitwidth * 2;
    int ands_per_comparison = std::log2(comparison_bitwidth) + 2;
    int num_ands = num_comparisons * ands_per_comparison;

    // we need to batch to avoid memory issues
    int batch_size = 1 << 30;

    stopwatch::timepoint("Start");

    int num_left = num_mul_triples;
    while (num_left > 0) {
        int amount = batch_size;
        if (num_left < batch_size) {
            amount = num_left;
        }
        runTime->reserve_mul_triples<int32_t>(amount);

        // read the triples to free the memory
        runTime->randomnessManagers[0]
            ->getCorrelation<int32_t, Correlation::BeaverMulTriple>()
            ->getNext(amount);
        num_left -= amount;
    }
    stopwatch::timepoint("Radix-Preprocessing");

    if (bitwidth == 32) {
        num_left = num_ands;
        while (num_left > 0) {
            int amount = batch_size;
            if (num_left < batch_size) {
                amount = num_left;
            }
            runTime->reserve_and_triples<int64_t>(amount);
            // read the triples to free the memory
            runTime->randomnessManagers[0]
                ->getCorrelation<int64_t, Correlation::BeaverAndTriple>()
                ->getNext(amount);
            num_left -= amount;
        }
    } else if (bitwidth == 64) {
        num_left = num_ands;
        while (num_left > 0) {
            int amount = batch_size;
            if (num_left < batch_size) {
                amount = num_left;
            }
            runTime->reserve_and_triples<__int128_t>(amount);
            // read the triples to free the memory
            runTime->randomnessManagers[0]
                ->getCorrelation<int32_t, Correlation::BeaverAndTriple>()
                ->getNext(amount);
            num_left -= amount;
        }
    }

    stopwatch::timepoint("Quick-Preprocessing");

#endif

    return 0;
}