#include "orq.h"
#include "profiling/memory.h"

using namespace orq::debug;
using namespace orq::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();

    auto test_size = runTime->getArg<size_t>("test-size", "r", 1000);
    orq::benchmarking::memory::mempoint("Start");

    stopwatch::timepoint("Start");
    for (int i = 0; i < test_size; i++) {
        float f = 16.0f + (rand() / (float)RAND_MAX) * 16.0f;
        size_t n = std::pow(2, f);

        auto a = std::chrono::steady_clock::now();
        Vector<int> v(n);
        v[n / 2] = 1;
        auto b = std::chrono::steady_clock::now();
        double delta = std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
        single_cout(i << " " << n << "," << delta / n);

        orq::benchmarking::output(std::to_string(n), delta / n);
    }

    stopwatch::done();

    return 0;
}