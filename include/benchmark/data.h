#ifndef SECRECY_BENCHMARKING_RANDOM_GENERATORS_H
#define SECRECY_BENCHMARKING_RANDOM_GENERATORS_H

#include "../debug/debug.h"

namespace secrecy { namespace benchmarking { namespace data {

    template<typename Share, typename ShareVector>
    class BenchmarkingRG {
    public:

        BenchmarkingRG() {}

        inline Share getNext(int id) {
            return 0;
        }

        inline Share getRandom() {
#ifdef MPC_USE_RANDOM_GENERATOR_DATA
            return rand() % MPC_RANDOM_DATA_RANGE;
#else
            return 0;
#endif
        }

        inline ShareVector getMultipleNext(int id, int size) {
            return ShareVector(size);
        }

        inline ShareVector getMultipleRandom(int size) {
            auto res = ShareVector(size);
            for (int i = 0; i < size; ++i) {
                res[i] = getRandom();
            }
            return res;
        }
    };


    // Create Beaver Triples Based RG
    template<typename Share, typename ShareVector, typename RG>
    static std::vector<RG> createBeaverTriplesRGs(const int &parties_num,
                                                  const int &s1,
                                                  const int &s2) {
        // TODO: std::move to reduce memory copying
        BenchmarkingRG<Share, ShareVector> rg;
        // Create beaver triples streams
        std::vector<ShareVector> a_1(1, ShareVector(rg.getMultipleRandom(s1)));
        std::vector<ShareVector> b_1(1, ShareVector(rg.getMultipleRandom(s1)));
        std::vector<ShareVector> c_1(1, ShareVector(a_1[0] * b_1[0]));

        std::vector<ShareVector> a_2(1, ShareVector(rg.getMultipleRandom(s2)));
        std::vector<ShareVector> b_2(1, ShareVector(rg.getMultipleRandom(s2)));
        std::vector<ShareVector> c_2(1, ShareVector(a_2[0] & b_2[0]));

        for (int i = 1; i < parties_num; ++i) {
            a_1.push_back(rg.getMultipleRandom(s1));
            a_1[i - 1] = a_1[i - 1] - a_1[i];
            b_1.push_back(rg.getMultipleRandom(s1));
            b_1[i - 1] = b_1[i - 1] - b_1[i];
            c_1.push_back(rg.getMultipleRandom(s1));
            c_1[i - 1] = c_1[i - 1] - c_1[i];

            a_2.push_back(rg.getMultipleRandom(s2));
            a_2[i - 1] = a_2[i - 1] ^ a_2[i];
            b_2.push_back(rg.getMultipleRandom(s2));
            b_2[i - 1] = b_2[i - 1] ^ b_2[i];
            c_2.push_back(rg.getMultipleRandom(s2));
            c_2[i - 1] = c_2[i - 1] ^ c_2[i];
        }

        std::vector<RG> vec_rg;
        for (int i = 0; i < parties_num; ++i) {
            vec_rg.push_back(RG(a_1[i], b_1[i], c_1[i],
                                a_2[i], b_2[i], c_2[i]));
        }
        return vec_rg;
    }

    // Create PRG
    template<typename Share, typename ShareVector, typename RG>
    static std::vector<RG> createPRGs(const int &parties_num,
                                      const int &s1,
                                      const int &s2) {
        // TODO: std::move to reduce memory copying
        BenchmarkingRG<Share, ShareVector> rg;

        // Create random data streams
        ShareVector r_a_1(rg.getMultipleRandom(s1));
        ShareVector r_a_2(rg.getMultipleRandom(s1));
        ShareVector r_a_3(rg.getMultipleRandom(s1));

        ShareVector r_b_1(rg.getMultipleRandom(s2));
        ShareVector r_b_2(rg.getMultipleRandom(s2));
        ShareVector r_b_3(rg.getMultipleRandom(s2));

        std::vector<RG> vec_rg;
        vec_rg.push_back(RG(r_a_1, r_a_3, r_a_1 - r_a_3,
                            r_b_1, r_b_3, r_b_1 ^ r_b_3));
        vec_rg.push_back(RG(r_a_2, r_a_1, r_a_2 - r_a_1,
                            r_b_2, r_b_1, r_b_2 ^ r_b_1));
        vec_rg.push_back(RG(r_a_3, r_a_2, r_a_3 - r_a_2,
                            r_b_3, r_b_2, r_b_3 ^ r_b_2));
        return vec_rg;
    }

        template<typename ShareVector>
        inline static std::vector<ShareVector> getATriples(int size) {
            auto a_1 = ShareVector(size);
            auto a_2 = ShareVector(size);

            auto b_1 = ShareVector(size);
            auto b_2 = ShareVector(size);

            auto c_1 = ShareVector(size);
            for (int i = 0; i < size; ++i) {
#ifdef MPC_USE_RANDOM_GENERATOR_DATA
                a_1[i] = rand() % MPC_RANDOM_DATA_RANGE;
                a_2[i] = rand() % MPC_RANDOM_DATA_RANGE;

                b_1[i] = rand() % MPC_RANDOM_DATA_RANGE;
                b_2[i] = rand() % MPC_RANDOM_DATA_RANGE;

                c_1[i] = rand() % MPC_RANDOM_DATA_RANGE;
#else
                a_1[i] = 0;
                a_2[i] = 0;

                b_1[i] = 0;
                b_2[i] = 0;

                c_1[i] = 0;
#endif
            }
            auto c_2 = (a_1 + a_2) * (b_1 + b_2) - c_1;

            return {std::move(a_1), std::move(b_1), std::move(c_1), std::move(a_2), std::move(b_2), std::move(c_2)};
        }

            template<typename ShareVector>
            inline static std::vector<ShareVector> getBTriples(int size) {
                auto a_1 = ShareVector(size);
                auto a_2 = ShareVector(size);

                auto b_1 = ShareVector(size);
                auto b_2 = ShareVector(size);

            auto c_1 = ShareVector(size);
            auto c_2 = ShareVector(size);
            for (int i = 0; i < size; ++i) {
#ifdef MPC_USE_RANDOM_GENERATOR_DATA
                a_1[i] = rand() % MPC_RANDOM_DATA_RANGE;
                a_2[i] = rand() % MPC_RANDOM_DATA_RANGE;

                b_1[i] = rand() % MPC_RANDOM_DATA_RANGE;
                b_2[i] = rand() % MPC_RANDOM_DATA_RANGE;

                c_1[i] = rand() % MPC_RANDOM_DATA_RANGE;
#else
                a_1[i] = 0;
                a_2[i] = 0;

                b_1[i] = 0;
                b_2[i] = 0;

                c_1[i] = 0;
#endif
                c_2[i] = ((a_1[i] ^ a_2[i]) & (b_1[i] ^ b_2[i])) ^ c_1[i];
            }

            return {std::move(a_1), std::move(b_1), std::move(c_1), std::move(a_2), std::move(b_2), std::move(c_2)};
        }

} } } // namespace gavel::benchmarking::data

#endif //SECRECY_BENCHMARKING_RANDOM_GENERATORS_H
