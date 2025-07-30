#ifndef SECRECY_BENCHMARKING_BENCHMARKING_H
#define SECRECY_BENCHMARKING_BENCHMARKING_H

// Assisting
#include "../debug/debug.h"
#include "data.h"
#include "utils.h"

// Unit Benchmarking
#include "../experiments/experiments.h"

#define init_mpc_benchmarking(service_benchmark);                                                           \
typedef service_benchmark::MPCBenchmarking< Data, DataVector,                                               \
                                            Share, ShareVector,                                             \
                                            ReplicatedShare, ReplicatedShareVector,                         \
                                            AShares, BShares,                                               \
                                            ShareTable, DataTable,                                          \
                                            Communicator, RG, Protocol> MPCBenchmarking;                    \
typedef secrecy::benchmarking::primitives<ShareTable, DataTable, MPCBenchmarking > PrimitivesBenchmarking;    \

#endif //SECRECY_BENCHMARKING_BENCHMARKING_H
