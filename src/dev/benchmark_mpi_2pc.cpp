#include "../../include/secrecy.h"

using namespace secrecy::service::mpi_service::beaver_2pc;
init_mpc_benchmarking(secrecy::service::benchmarking);


int main(int argc, char** argv){
    secrecy_init(argc, argv);

    int batchesNum = 50, batchSize = 1000000;

    // No rounds
    PrimitivesBenchmarking::benchmark_xor_b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_add_a(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_sub_a(batchesNum, batchSize);

    // 1 round
    PrimitivesBenchmarking::benchmark_and_b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_or__b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_mul_a(batchesNum, batchSize);

    // 6 rounds
    PrimitivesBenchmarking::benchmark_eq__b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_neq_b(batchesNum, batchSize);

    // 7 rounds
    PrimitivesBenchmarking::benchmark_gt__b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_lt__b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_gte_b(batchesNum, batchSize);
    PrimitivesBenchmarking::benchmark_lte_b(batchesNum, batchSize);

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif

    return 0;
}