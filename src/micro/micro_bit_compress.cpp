#include "../../include/secrecy.h"

#include <iomanip>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
//#pragma GCC diagnostic ignored "-Wcompound-token-split-by-macro"

using namespace secrecy::debug;
using namespace secrecy::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

// #define CHECK_ALTERNATIVES

#define BIT_LOOP(EXPR) {                            \
    for (int i = 0; i < MAX_BITS_NUMBER; i++) {     \
        (EXPR);                                     \
    }                                               \
}                                                   \

#define LOG_BIT_LOOP(EXPR) {                                 \
    for (int i = 1; i <= std::log2(MAX_BITS_NUMBER); i++) {  \
        (EXPR);                                              \
    }                                                        \
}                                                   

void profile_vector() {
    auto pID = runTime->getPartyID();
    
    int test_size = 1 << 22;

    // Set DataType here:
    using T = int32_t;
    const int MAX_BITS_NUMBER = std::numeric_limits<std::make_unsigned_t<T>>::digits;

    const int compressed_size = test_size / MAX_BITS_NUMBER;

    // Load `a` with random data
    secrecy::Vector<T> a(test_size), b(test_size);
    runTime->randomnessManagers[0]->localPRG->getNext(a);

    Vector<T> c(compressed_size), d(compressed_size);
    
    single_cout("Vector " << test_size << " x " << MAX_BITS_NUMBER << "b");

#ifdef CHECK_ALTERNATIVES
    single_cout("Check SBC Correctness...");
    for (int i = 0; i < MAX_BITS_NUMBER; i++) {
        c = a.simple_bit_compress(i, 1, i, 1);
        a.sbc_hw(d, i);
        assert(c.same_as(d));
        // Make sure not trivially correct and all zero!
        assert(c[0] != 0);
    }
    single_cout("  ...OK");

    // Randomize
    runTime->randomnessManagers[0]->localPRG->getNext(a);
    b = a;

    single_cout("Check SBD Correctness...");
    for (int i = 0; i < MAX_BITS_NUMBER; i++) {
    runTime->randomnessManagers[0]->localPRG->getNext(c);
        a.simple_bit_decompress(c, i);
        b.sbd_hw(c, 1L << i);
        assert(a.same_as(b));
    }
    single_cout("  ...OK");

    runTime->randomnessManagers[0]->localPRG->getNext(a);

    single_cout("Check BLS Correctness...");
    for (int i = 1; i <= std::log2(MAX_BITS_NUMBER); i++) {
        c = a.bit_level_shift(1 << i);
        d = a.bls_bitwise(i);
        assert(c.same_as(d));
        // Make sure not trivially correct and all zero!
        assert(c[0] != 0);
    }
    single_cout("  ...OK");
#endif

    double elapsed;

    PROFILE(BIT_LOOP(c = a.simple_bit_compress(i, 1, i, 1)), "SBC4");
    auto baseline = elapsed;
    PROFILE(BIT_LOOP(c.pack_from(a, i)), "SBC2 (pack_from)");
#ifdef CHECK_ALTERNATIVES
    PROFILE(BIT_LOOP(a.sbc_hw(c, i)), "HW  ");
    auto hw_elapsed = elapsed;
    single_cout("SBC HW Speedup: " << baseline / hw_elapsed << "x\n");
#endif

    PROFILE(BIT_LOOP(a.unpack_from(c, i)), "SBD (unpack_from) ");
    baseline = elapsed;

#ifdef CHECK_ALTERNATIVES
    PROFILE(BIT_LOOP(a.sbd_hw(c, 1L << i)), "HW  ");
    hw_elapsed = elapsed;
    single_cout("SBD HW Speedup: " << baseline / hw_elapsed << "x\n");
#endif

    PROFILE(LOG_BIT_LOOP(c = a.bit_level_shift(i)), "BLS ");
    baseline = elapsed;

#ifdef CHECK_ALTERNATIVES
    PROFILE(LOG_BIT_LOOP(c = a.bls_bitwise(i)), "BW  ");
    hw_elapsed = elapsed;
    single_cout("BLS HW Speedup: " << baseline / hw_elapsed << "x\n");
#endif

}

int main(int argc, char** argv) {
    // Initialize Secrecy runtime [executable - threads_num - p_factor - batch_size]
    secrecy_init(argc, argv);

    profile_vector();

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}

#pragma GCC diagnostic pop
