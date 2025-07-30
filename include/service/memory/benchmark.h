#ifndef GAVEL_BENCHMARKING_BENCHMARK_H
#define GAVEL_BENCHMARKING_BENCHMARK_H

#include "../../benchmark/benchmarking.h"
#include <thread>


// Generate Data: takes by reference something accessible as []
// Generate Beaver Triples

namespace gavel { namespace benchmarking {

// TODO: generalize for other protocols
    template<typename Data, typename DataVector,
            typename Share, typename ShareVector,
            typename ReplicatedShare, typename ReplicatedShareVector,
            typename AShares, typename BShares,
            typename ShareTable, typename DataTable,
            typename Communicator, typename RG, typename Protocol>
    class MPCBenchmarking {

        static void doSomeComputation(void (testMPC)(ShareTable *shareTable),
                                      int party_num, Communicator communicator, RG rg,
                                      ShareTable *shareTable) {
            auto protocol = Protocol(party_num, &communicator, &rg);
            AShares::protocol = &protocol;
            testMPC(shareTable);
        }


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

            printf("%s \t\tpassed in:\t\t%f\t\t\tThroughput:\t\t%dk\n", name.c_str(), elapsed,
                   int(batchSize * batchesNum / elapsed / 1000));

        }

        static void execute(int batchSize,
                            void (testMPC)(ShareTable *shareTable),
                            void (plainFunc)(DataTable *dataTable),
                            std::string tableName, std::vector<std::string> columns, int rows,
                            int ra_size, int rb_size) {

            // Create communicators & randomNumberGenerators
            auto communicators = Communicator::createCommunicators(Protocol::parties_num);
            std::vector<RG> randomGenerators;
            if (Protocol::parties_num == 2) {
                randomGenerators = secrecy::benchmarking::data::createBeaverTriplesRGs<int, ShareVector, RG>(Protocol::parties_num, ra_size,
                                                                                rb_size);
            } else if (Protocol::parties_num == 3) {
                randomGenerators = secrecy::benchmarking::data::createPRGs<int, ShareVector, RG>(Protocol::parties_num, ra_size,
                                                                    rb_size);
            }

            secrecy::benchmarking::data::BenchmarkingRG<Share, ShareVector> benchmark_rg;
            Protocol benchmark_p(0, nullptr, &benchmark_rg);

            // Generate random data [partyNum][varNum][row]
            std::vector<ShareTable> shareTables;
            for(int i =0; i < Protocol::parties_num; ++i){
                shareTables.push_back(ShareTable(tableName, columns, batchSize));
            }
            std::vector<DataVector> dataTable(columns.size(), batchSize);
            for (unsigned int k = 0; k < columns.size(); ++k) {
                // Create data_table data
                dataTable[k] = benchmark_rg.getMultipleRandom(batchSize);

                std::string column = columns[k];
                if (ShareTable::isBShared(column)) {
                    auto column_shares = benchmark_p.get_shares_b(dataTable[k]);
                    for (int partyInd = 0; partyInd < Protocol::parties_num; ++partyInd) {
                        (*(BShares*)(&shareTables[partyInd][column])) = BShares(column_shares[partyInd]);
                    }
                } else {
                    auto column_shares = benchmark_p.get_shares_a(dataTable[k]);
                    for (int partyInd = 0; partyInd < Protocol::parties_num; ++partyInd) {
                        (*(AShares*)(&shareTables[partyInd][column])) = AShares(column_shares[partyInd]);
                    }
                }
            }


            // Create Computing Parties threads
            std::vector<std::thread> threads;

            for (int i = 0; i < Protocol::parties_num; ++i) {
                threads.push_back(
                        std::thread(doSomeComputation, testMPC, i, communicators[i], randomGenerators[i],
                                    &shareTables[i]));
            }
            for (int i = 0; i < Protocol::parties_num; ++i) {
                threads[i].join();
            }

#if defined(MPC_CHECK_CORRECTNESS) || defined(MPC_PRINT_RESULT)
            // Calculate Results
            plainFunc(&dataTable);

            std::vector<DataVector> res_open_Table(columns.size(), batchSize);
            for (unsigned int k = 0; k < columns.size(); ++k) {
                std::string column = columns[k];
                std::vector<ReplicatedShareVector> column_shares;

                if (ShareTable::isBShared(column)) {
                    for (int partyInd = 0; partyInd < Protocol::parties_num; ++partyInd) {
                        column_shares.push_back((*(BShares *) (&shareTables[partyInd][column])).vector);
                    }
                    res_open_Table[k] = benchmark_p.open_shares_b(column_shares);
                } else {
                    for (int partyInd = 0; partyInd < Protocol::parties_num; ++partyInd) {
                        column_shares.push_back((*(AShares *) (&shareTables[partyInd][column])).vector);
                    }
                    res_open_Table[k] = benchmark_p.open_shares_a(column_shares);
                }
            }
#ifdef MPC_CHECK_CORRECTNESS
            for (unsigned int k = 0; k < dataTable.size(); ++k) {
                for (unsigned int i = 0; i < batchSize; ++i) {
                    assert(dataTable[k][i] == res_open_Table[k][i]);
                }
            }
#endif
#ifdef MPC_PRINT_RESULT
            std::cout << "\nResult\n";
            for (unsigned int i = 0; i < batchSize; ++i) {
                for(unsigned int k = 0; k < dataTable.size(); ++k) {
                    secrecy::benchmarking::utils::print_bin(dataTable[k][i], res_open_Table[k][i], false);
                    if (k == (dataTable.size() - 2) || k == (dataTable.size() - 1)) {
                        printf("%d\t%d\t\t\n", dataTable[k][i], res_open_Table[k][i]);
                    } else {
                        printf("%d\t\t", res_open_Table[k][i]);
                    }
                }
            }
#endif
#endif
        }

    };
}}


#endif //GAVEL_BENCHMARKING_BENCHMARK_H
