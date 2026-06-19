#include "orq.h"
#include "profiling/stopwatch.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

template <typename T>
void reserve_b(int up_to) {
    auto L = sizeof(T) * 8;
    for (int s = 128; s <= up_to; s *= 2) {
        stopwatch::get_elapsed();
        runTime->reserve_and_triples<T>(s);
        auto reserve_time = stopwatch::get_elapsed();

        auto us_per = reserve_time / s * 1e6;

        single_cout("AND " << std::right << std::fixed << std::setprecision(5) << std::setw(3) << L
                           << "b x " << std::setw(10) << s << ": " << std::setw(10) << reserve_time
                           << " s; " << std::setw(10) << us_per << " us / triple = "
                           << std::setw(10) << us_per / L * 1e3 << " ns / bit");

        // use em up
        BSharedVector<T> a(s), b(s);
        a &= b;
    }
    single_cout("--");
}

template <typename T>
void reserve_a(int up_to) {
    auto L = sizeof(T) * 8;
    for (int s = 128; s <= up_to; s *= 2) {
        stopwatch::get_elapsed();
        runTime->reserve_mul_triples<T>(s);
        auto reserve_time = stopwatch::get_elapsed();

        auto us_per = reserve_time / s * 1e6;

        single_cout("MUL " << std::right << std::fixed << std::setprecision(5) << std::setw(3) << L
                           << "b x " << std::setw(10) << s << ": " << std::setw(10) << reserve_time
                           << " s; " << std::setw(10) << us_per << " us / triple = "
                           << std::setw(10) << us_per / L * 1e3 << " ns / bit");

        // use em up
        ASharedVector<T> a(s), b(s);
        a *= b;
    }
    single_cout("--");
}

int main(int argc, char** argv) {
    orq_init(argc, argv);
#ifndef MPC_PROTOCOL_BEAVER_TWO
    single_cout("Skipping micro_triples for non-2PC");
#else

    auto pID = runTime->getPartyID();
    auto test_size = runTime->getArg<size_t>("test-size", "r", 1 << 20);
    auto num_threads = runTime->get_num_threads();

    reserve_b<int8_t>(test_size);
    reserve_b<int16_t>(test_size);
    reserve_b<int32_t>(test_size);
    reserve_b<int64_t>(test_size);
    reserve_b<__int128_t>(test_size);

    reserve_a<int8_t>(test_size);
    reserve_a<int16_t>(test_size);
    reserve_a<int32_t>(test_size);
    reserve_a<int64_t>(test_size);
    reserve_a<__int128_t>(test_size);

    runTime->print_communicator_statistics();

    BSharedVector<T> b1(test_size), b2(test_size);
    ASharedVector<T> a1(test_size), a2(test_size);

    stopwatch::timepoint("Start");

    auto y = b1 & b2;
    stopwatch::timepoint("and - no reserve");
    auto z = a1 * a1;
    stopwatch::timepoint("mult - no reserve");

    runTime->reserve_and_triples<T>(test_size * num_threads);
    stopwatch::timepoint("ReserveAndTriples");

    runTime->reserve_mul_triples<T>(test_size * num_threads);
    stopwatch::timepoint("ReserveMulTriples");

    y = b1 & b2;
    stopwatch::timepoint("and - with reserve");
    z = a1 * a1;
    stopwatch::timepoint("mult - with reserve");

    y = b1 & b2;
    stopwatch::timepoint("and - none left");

    runTime->reserve_and_triples<T>(test_size);
    stopwatch::timepoint("ReserveAndTriples Again");

    for (int i = 0; i < REPEAT; i++) {
        runTime->reserve_and_triples<T>(test_size);
    }
    stopwatch::timepoint("ReserveAndTriples " S_(REPEAT) "x");

    runTime->reserve_and_triples<T>(REPEAT * test_size);
    stopwatch::timepoint("Reserve " S_(REPEAT) "x AndTriples");

    for (int i = 0; i < 20; i++) {
        y = b1 & b2;
        stopwatch::timepoint("and - more reserved " + std::to_string(i));
    }

#endif

    return 0;
}
