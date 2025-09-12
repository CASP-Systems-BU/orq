/**
 * @file micro_batching.cpp
 * @brief Find the optimal batching paramater for this network setting.
 *
 */

#include "orq.h"

using namespace orq::debug;
using namespace orq::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#define _STRING(x) #x
#define STRING(x) _STRING(x)

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();

    const size_t TOTAL_SIZE = 1 << 20;

    float best_time = -1;
    size_t best_batch = -1;

    const int iters = 16;

    single_cout("# Check batching: " << iters << " iters of length-" << TOTAL_SIZE << " mult");

    ASharedVector<int> a(TOTAL_SIZE), b(TOTAL_SIZE);

    for (size_t x = 1024; x <= TOTAL_SIZE; x <<= 1) {
        runTime->setBatchSize(x);

        stopwatch::get_elapsed();
        for (int i = 0; i < iters; i++) {
            auto z = a * b;
        }

        auto r = stopwatch::get_elapsed() * (1'000'000'000.0 / TOTAL_SIZE);

        single_cout("# bs=" << std::setw(10) << x << ": " << std::fixed << std::setprecision(2) << r
                            << " ns / gate");

        if (best_time < 0 || r < best_time) {
            best_time = r;
            best_batch = x;
        }
    }

    single_cout("# -- " << best_batch << " @ " << best_time);

    if (pID == 0) {
        for (int p = 1; p < runTime->getNumParties(); p++) {
            runTime->comm0()->sendShare((int64_t)best_batch, p);
        }
    } else {
        runTime->comm0()->receiveShare((int64_t&)best_batch, -pID);
    }

    int steps = 16;
    double step_multiplier = pow(4, 1.0 / steps);

    const int multiple = 256;

    size_t loop_start = best_batch / 2;
    size_t loop_end = best_batch * 2;

    for (size_t x = loop_start; x <= loop_end; x *= step_multiplier) {
        // round to the nearest multiple
        size_t xr = round((double)x / multiple) * multiple;

        runTime->setBatchSize(xr);

        stopwatch::get_elapsed();
        for (int i = 0; i < iters; i++) {
            auto z = a * b;
        }
        auto r = stopwatch::get_elapsed() * (1'000'000'000.0 / TOTAL_SIZE);

        single_cout("# bs=" << std::setw(10) << xr << ": " << std::fixed << std::setprecision(2)
                            << r << " ns / gate");

        if (best_time < 0 || r < best_time) {
            best_time = r;
            best_batch = xr;
        }
    }

    single_cout("# -- " << best_batch << " @ " << best_time);

    single_cout("BATCHSIZE: " << best_batch);

    if (pID == 0) {
        std::ofstream outFile("auto_batchsize.txt");
        if (outFile.is_open()) {
            auto t = std::time(nullptr);
            auto tm = *std::localtime(&t);
            outFile << "# timestamp: " << std::put_time(&tm, "%Y-%m-%d %H:%M") << "\n";
            outFile << "# protocol: " << STRING(COMPILED_MPC_PROTOCOL_NAMESPACE) << " ("
                    << runTime->getNumParties() << "PC)\n";
            outFile << "# threads: " << runTime->get_num_threads() << "\n";
            outFile << "BATCHSIZE: " << best_batch << "\n";
            outFile.close();
        } else {
            single_cout("Error: Unable to open file.");
        }
    }
}