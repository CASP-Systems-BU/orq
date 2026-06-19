#include "core/math/primes.h"
#include "orq.h"
#include "profiling/stopwatch.h"

using namespace orq::random;
using namespace orq::math::primes;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    orq_init(argc, argv);
#ifndef MPC_PROTOCOL_BEAVER_TWO
    single_cout("Skipping SHPRG for non-2PC");
#else

    // seed length
    int seed_length = 2048;

    stopwatch::timepoint("Start");

    auto shprg = SeedHomomorphicPRG<__int128_t, int32_t>(runtime, seed_length, seed_length * 2);

    stopwatch::timepoint("Sample Public Parameter");

    auto seed = shprg.sample_seed(runtime);
    auto binary_seed = shprg.sample_binary_seed(runtime);
    auto compressed_binary_seed = shprg.compress_binary_seed(binary_seed);

    stopwatch::timepoint("Sample Seeds");

    auto expanded = shprg.expand(seed);

    stopwatch::timepoint("Expand Full-Domain Seed");

    auto expanded_binary = shprg.expand_binary_seed(binary_seed);

    stopwatch::timepoint("Expand Binary Seed");

    auto expanded_compressed_binary = shprg.expand_compressed_binary_seed(compressed_binary_seed);

    stopwatch::timepoint("Expand Compressed Binary Seed");

    stopwatch::done();

#endif
    return 0;
}
