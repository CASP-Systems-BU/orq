#ifndef SECRECY_SERVICE_BENCHMARK_H
#define SECRECY_SERVICE_BENCHMARK_H

#include "../../benchmark/benchmarking.h"
#include "runtime.h"

namespace secrecy{ namespace service { namespace benchmarking {

    template<typename Data, typename DataVector,
            typename Share, typename ShareVector,
            typename ReplicatedShare, typename ReplicatedShareVector,
            typename AShares, typename BShares,
            typename ShareTable, typename DataTable,
            typename Communicator, typename RG, typename Protocol>
    class MPCBenchmarking {
    public:
        static void timedExecute(int batchSize, int batchesNum, std::string name,
                                 void (testMPC)(ShareTable *shareTable),
                                 void (plainFunc)(DataTable *dataTable),
                                 std::string tableName, std::vector<std::string> columns, int rows,
                                 int ra_size, int rb_size) {

            struct timeval begin, end;
            long seconds, micro;
            double elapsed;

            // start timer
            gettimeofday(&begin, 0);
            for (int i = 0; i < batchesNum; ++i) {
                execute(batchSize,
                        testMPC, plainFunc,
                        tableName, columns, rows,
                        ra_size, rb_size);
            }

            // stop timer
            gettimeofday(&end, 0);
            seconds = end.tv_sec - begin.tv_sec;
            micro = end.tv_usec - begin.tv_usec;
            elapsed = seconds + micro * 1e-6;

            if (runTime->getPartyID() == 0) {
                printf("%s \t\tpassed in:\t\t%f\t\t\tThroughput:\t\t%dk\n", name.c_str(), elapsed,
                       int(batchSize * batchesNum / elapsed / 1000));
            }

        }

        static void execute(int batchSize,
                            void (testMPC)(ShareTable *shareTable),
                            void (plainFunc)(DataTable *dataTable),
                            std::string tableName, std::vector<std::string> columns, int rows,
                            int ra_size, int rb_size) {

            // Generate Random Table [ColumnName][Replication][Index]
            ShareTable shareTable(tableName, columns, batchSize);
            srand(time(NULL) + runTime->getPartyID());

#if defined(MPC_GENERATE_DATA)
            for (auto &column : columns) {
                auto &shareVector = shareTable[column];
                ReplicatedShareVector col (batchSize);
//                for (int i = 0; i < AShares::protocol->replicationNumber; ++i) {
                    for (int ind = 0; ind < batchSize; ++ind) {
#if defined(MPC_USE_RANDOM_GENERATOR_DATA)
                        col(0)[ind] = rand() % MPC_RANDOM_DATA_RANGE;
#else
                        col(0)[ind] = ind % MPC_RANDOM_DATA_RANGE;
#endif
                    }
//                }
                if (runTime->getReplicationNumber() == 2) {
                    runTime->comm0()->exchangeShares(col(0), col(1), 1, 2, batchSize);
                }
                
                if(runTime->getReplicationNumber() == 3){
                    runTime->comm0()->exchangeShares(col(0), col(1), +3, +1, batchSize);
                    runTime->comm0()->exchangeShares(col(0), col(2), +2, +2, batchSize);
                }

                if(ShareTable::isBShared(column)){
                    shareVector = BShares(col);
                }else{
                    shareVector = AShares(col);
                }

            }
#endif

            testMPC(&shareTable);
#if defined(MPC_CHECK_CORRECTNESS) || defined(MPC_PRINT_RESULT)
            // Calculate Results
            auto res_open_Table = shareTable.openTable();
            auto data_Table = shareTable.openTable();

#if defined(MPC_EVALUATE_CORRECT_OUTPUT)
            plainFunc(&data_Table);
#endif

#ifdef MPC_CHECK_CORRECTNESS
            for (int k = 0; k < res_open_Table.size(); ++k) {
                for (int i = 0; i < batchSize; ++i) {
                    assert(data_Table[k][i] == res_open_Table[k][i]);
                }
            }
#endif
#ifdef MPC_PRINT_RESULT
            if (AShares::protocol->partyID == 0) {
                std::cout << "\nResult\n";
                for (unsigned int i = 0; i < batchSize; ++i) {
                    for (unsigned int k = 0; k < data_Table.size(); ++k) {
                        secrecy::benchmarking::utils::print_bin(data_Table[k][i], res_open_Table[k][i], false);
                        if (k == (data_Table.size() - 2) || k == (data_Table.size() - 1)) {
                            printf("%d\t%d\t\t\n", data_Table[k][i], res_open_Table[k][i]);
                        } else {
                            printf("%d\t\t", res_open_Table[k][i]);
                        }
                        assert(data_Table[k][i] == res_open_Table[k][i]);
                    }
                }
            }
#endif
#endif
        }
    };

} } } // namespace secrecy::service::benchmarking



#endif //SECRECY_SERVICE_BENCHMARK_H
