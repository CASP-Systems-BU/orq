#include <iostream>
#include <set>

#include "orq.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::random;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

const int num_parties = PROTOCOL_NUM;

// **************************************** //
//        Test LocalPRG Correctness         //
// **************************************** //
template <typename T>
void test_local_prg_randomness() {
    auto rank = runTime->getPartyID();
    int test_size = 1000;

    auto localPRG = runTime->rand0()->localPRG;

    // test vectorized queries to getNext
    Vector<T> randomness(test_size);
    localPRG->getNext(randomness);

    // convert to plain std::vector
    std::vector<T> r;
    for (int i = 0; i < randomness.size(); i++) {
        r.push_back(randomness[i]);
    }

    // check that there aren't too many duplicates
    // we could do an equality check, but this still could fail with low-ish
    // probability. Instead, just set some arbitrarily high threshold that we
    // are unlikely to fail with greater than negligible probability
    //
    // convert vector to a set (which removes duplicates) and check same size
    std::set<T> s(r.begin(), r.end());
    assert(s.size() >= test_size * 0.99);

    return;
}

// ***************************************** //
//        Test XChaCha20 Correctness         //
// ***************************************** //
template <typename T>
void test_xchacha20_randomness() {
    int test_size = 1000;
    std::vector<unsigned char> seed(crypto_stream_xchacha20_KEYBYTES);
    XChaCha20PRGAlgorithm::xchacha20KeyGen(seed);
    auto prg_algorithm = std::make_unique<XChaCha20PRGAlgorithm>(seed);

    // test vectorized queries to getNext
    Vector<T> randomness(test_size);
    prg_algorithm->getNext(randomness);

    // convert to plain std::vector
    std::vector<T> r = randomness.as_std_vector();

    // check that there aren't too many duplicates
    // we could do an equality check, but this still could fail with low-ish
    // probability. Instead, just set some arbitrarily high threshold that we
    // are unlikely to fail with greater than negligible probability
    //
    // convert vector to a set (which removes duplicates) and check same size
    std::set<T> s(r.begin(), r.end());
    assert(s.size() >= test_size * 0.99);

    return;
}

// **************************************** //
//           Test Group Generation          //
// **************************************** //
// helper function to check if the group generation for a given parameter set matches our
// expectation
void helper_check_group_generation_correct(int num_parties, int group_size,
                                           std::vector<std::set<int>> correct) {
    std::set<int> parties;
    for (int i = 0; i < num_parties; i++) {
        parties.insert(i);
    }
    std::set<int> partial_combination;
    std::vector<std::set<int>> groups;
    orq::ProtocolBase::generateAllCombinations(parties, partial_combination, group_size, groups);

    for (int i = 0; i < correct.size(); i++) {
        assert(groups[i] == correct[i]);
    }
}

// brute force test over all possible 3pc and 4pc inputs
void test_group_generation() {
    // 3 parties, groups of size 1
    std::vector<std::set<int>> correct_3_1 = {{0}, {1}, {2}};
    helper_check_group_generation_correct(3, 1, correct_3_1);

    // 3 parties, groups of size 2
    std::vector<std::set<int>> correct_3_2 = {{0, 1}, {0, 2}, {1, 2}};
    helper_check_group_generation_correct(3, 2, correct_3_2);

    // 4 parties, groups of size 1
    std::vector<std::set<int>> correct_4_1 = {{0}, {1}, {2}, {3}};
    helper_check_group_generation_correct(4, 1, correct_3_1);

    // 4 parties, groups of size 2
    std::vector<std::set<int>> correct_4_2 = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
    helper_check_group_generation_correct(4, 2, correct_4_2);

    // 4 parties, groups of size 3
    std::vector<std::set<int>> correct_4_3 = {{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}};
    helper_check_group_generation_correct(4, 3, correct_4_3);
}

// **************************************** //
//     Test CommonPRG Correctness (3PC)     //
// **************************************** //
template <typename T>
void test_3pc_common_prg_correctness() {
    auto rank = runTime->getPartyID();
    int test_size = orq::random::AESPRGAlgorithm::MAX_AES_QUERY_BYTES;

    std::vector<int> relative_ranks = {-1, +1};
    std::vector<Vector<T>*> common_randomness;

    for (int relative_rank : relative_ranks) {
        // generate randomness from CommonPRG
        auto common_prg = runTime->rand0()->commonPRGManager->get(relative_rank);

        // TODO randomness vector has MAX_AES_QUERY_BYTES size, which is a bit
        // misleading because T can be a multi-byte type like int32. So using
        // MAX_AES_QUERY_BYTES as the size just forces getNext to have multiple
        // batches. Think about how to communicate that better.
        Vector<T> randomness(test_size);

        common_prg->getNext(randomness);
        common_randomness.push_back(new Vector<T>(randomness));

        // check that the values are not zero
        bool all_zero = true;
        for (int j = 0; j < test_size; j++) {
            if (randomness[j] != 0) {
                all_zero = false;
            }
        }
        assert(!all_zero);
    }

    // now check that the generated randomness is correctly shared between parties
    Vector<T> shared_with_prev = *common_randomness[0];
    Vector<T> shared_with_next = *common_randomness[1];

    Vector<T> remote(test_size);
    runTime->comm0()->exchangeShares(shared_with_next, remote, +1, -1, test_size);

    // check correctness
    for (int i = 0; i < test_size; i++) {
        assert(shared_with_prev[i] == remote[i]);
    }

    return;
}

// **************************************** //
//     Test CommonPRG Correctness (4PC)     //
// **************************************** //
template <typename T>
void test_4pc_common_prg_correctness() {
    auto rank = runTime->getPartyID();
    int test_size = orq::random::AESPRGAlgorithm::MAX_AES_QUERY_BYTES;

    std::vector<Vector<T>*> common_randomness;

    for (int relative_rank = 1; relative_rank < num_parties; relative_rank++) {
        // generate randomness from CommonPRG
        auto common_prg = runTime->rand0()->commonPRGManager->get(relative_rank);

        Vector<T> randomness(test_size);

        common_prg->getNext(randomness);
        common_randomness.push_back(new Vector<T>(randomness));

        // check that the values are not zero
        bool all_zero = true;
        for (int j = 0; j < test_size; j++) {
            if (randomness[j] != 0) {
                all_zero = false;
            }
        }
        assert(!all_zero);
    }

    for (int i = 0; i < 4; i++) {
        if (i == rank) continue;

        int relative_rank = (4 + i - rank) % 4;
        Vector<T> common_vec = *common_randomness[relative_rank - 1];

        Vector<T> remote1(test_size);
        Vector<T> remote2(test_size);

        if (relative_rank == 1) {
            runTime->comm0()->exchangeShares(common_vec, remote1, -1, -1, test_size);
            runTime->comm0()->exchangeShares(common_vec, remote2, -2, -2, test_size);
        } else if (relative_rank == 2) {
            runTime->comm0()->exchangeShares(common_vec, remote1, -1, -1, test_size);
            runTime->comm0()->exchangeShares(common_vec, remote2, +1, +1, test_size);
        } else if (relative_rank == 3) {
            runTime->comm0()->exchangeShares(common_vec, remote1, +1, +1, test_size);
            runTime->comm0()->exchangeShares(common_vec, remote2, +2, +2, test_size);
        }

        // check correctness
        for (int j = 0; j < test_size; j++) {
            assert(common_vec[j] == remote1[j]);
            assert(common_vec[j] == remote2[j]);
        }
    }

    return;
}

// **************************************** //
//    Test CommonPRG Correctness (Groups)   //
// **************************************** //
template <typename T>
void test_common_prg_correctness_groups() {
    auto rank = runTime->getPartyID();
    int test_size = orq::random::AESPRGAlgorithm::MAX_AES_QUERY_BYTES;

    for (std::set<int> group : runTime->getGroups()) {
        if (!group.contains(rank)) continue;

        Vector<T> randomness(test_size);
        auto common_prg = runTime->rand0()->commonPRGManager->get(group);
        common_prg->getNext(randomness);

        int lowestRank = *group.begin();
        if (rank == lowestRank) {
            // lowest rank receives every other party's vector
            for (int otherRank : group) {
                if (rank == otherRank) continue;
                int relative_rank = otherRank - rank;
                Vector<T> remote(test_size);
                runTime->comm0()->exchangeShares(randomness, remote, relative_rank, relative_rank,
                                                 test_size);

                // check correctness
                bool all_zero = true;
                for (int j = 0; j < test_size; j++) {
                    assert(randomness[j] == remote[j]);
                    if (randomness[j] != 0) {
                        all_zero = false;
                    }
                }
                assert(!all_zero);
            }
        } else {
            // just exchange with lowest rank, check equality
            int relative_rank = lowestRank - rank;
            Vector<T> remote(test_size);
            runTime->comm0()->exchangeShares(randomness, remote, relative_rank, relative_rank,
                                             test_size);

            // check correctness
            bool all_zero = true;
            for (int j = 0; j < test_size; j++) {
                assert(randomness[j] == remote[j]);
                if (randomness[j] != 0) {
                    all_zero = false;
                }
            }
            assert(!all_zero);
        }
    }
}

// **************************************** //
//   Test ZeroSharingGenerator Correctness  //
// **************************************** //
template <typename T>
void test_zero_sharing_generator_correctness() {
    int test_size = orq::random::AESPRGAlgorithm::MAX_AES_QUERY_BYTES;

    // generate the values
    auto zeroSharingGenerator = runTime->rand0()->zeroSharingGenerator;
    Vector<T> my_shares_arithmetic(test_size);
    Vector<T> my_shares_binary(test_size);
    zeroSharingGenerator->getNextArithmetic(my_shares_arithmetic);
    zeroSharingGenerator->getNextBinary(my_shares_binary);

    // check that the values are not zero
    if (num_parties > 1) {
        bool all_zero_a = true, all_zero_b = true;
        for (int i = 0; i < test_size; i++) {
            if (my_shares_arithmetic[i] != 0) {
                all_zero_a = false;
            }
            if (my_shares_binary[i] != 0) {
                all_zero_b = false;
            }
        }
        assert(!all_zero_a && !all_zero_b);
    }

    // get all shares
    std::vector<Vector<T>> other_shares_arithmetic;
    std::vector<Vector<T>> other_shares_binary;
    for (int i = 1; i < num_parties; i++) {
        Vector<T> other_shares_a(test_size);
        Vector<T> other_shares_b(test_size);
        int send_party = i;
        int recv_party = num_parties - i;
        runTime->comm0()->exchangeShares(my_shares_arithmetic, other_shares_a, send_party,
                                         recv_party, test_size);
        runTime->comm0()->exchangeShares(my_shares_binary, other_shares_b, send_party, recv_party,
                                         test_size);
        other_shares_arithmetic.push_back(other_shares_a);
        other_shares_binary.push_back(other_shares_b);
    }

    // check that the values sum/xor to zero
    for (int i = 0; i < test_size; i++) {
        int sum_shares = my_shares_arithmetic[i];
        int xor_shares = my_shares_binary[i];
        for (int j = 0; j < num_parties - 1; j++) {
            sum_shares += other_shares_arithmetic[j][i];
            xor_shares ^= other_shares_binary[j][i];
        }

        assert(sum_shares == 0);
        assert(xor_shares == 0);
    }
}

// **************************************** //
//    Test ZeroSharingGenerator (Groups)    //
// **************************************** //
template <typename T>
void test_zero_sharing_generator_groups() {
    auto rank = runTime->getPartyID();
    int test_size = orq::random::AESPRGAlgorithm::MAX_AES_QUERY_BYTES;

    auto zeroSharingGenerator = runTime->rand0()->zeroSharingGenerator;

    for (std::set<int> group : runTime->getGroups()) {
        if (!group.contains(rank)) continue;

        std::vector<Vector<T>> randomness_arithmetic;
        std::vector<Vector<T>> randomness_binary;
        for (int i = 0; i < num_parties; i++) {
            randomness_arithmetic.push_back(Vector<T>(test_size));
            randomness_binary.push_back(Vector<T>(test_size));
        }

        zeroSharingGenerator->groupGetNextArithmetic(randomness_arithmetic, group);
        zeroSharingGenerator->groupGetNextBinary(randomness_binary, group);

        // check that the values are not zero
        bool all_zero_a = true, all_zero_b = true;
        if (num_parties > 1) {
            for (int i = 0; i < num_parties; i++) {
                for (int j = 0; j < test_size; j++) {
                    if (randomness_arithmetic[i][j] != 0) {
                        all_zero_a = false;
                    }
                    if (randomness_binary[i][j] != 0) {
                        all_zero_b = false;
                    }
                }
            }
            assert(!all_zero_a && !all_zero_b);
        }

        // check that the values sum/xor to zero
        Vector<T> sum_shares(test_size);
        Vector<T> xor_shares(test_size);
        for (int i = 0; i < num_parties; i++) {
            sum_shares += randomness_arithmetic[i];
            xor_shares ^= randomness_binary[i];
        }
        for (int i = 0; i < test_size; i++) {
            assert(sum_shares[i] == 0);
            assert(xor_shares[i] == 0);
        }
    }
}

int main(int argc, char** argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();

    // test local randomness
    // unsigned
    test_local_prg_randomness<uint32_t>();
    test_local_prg_randomness<uint64_t>();
    // signed
    test_local_prg_randomness<int32_t>();
    test_local_prg_randomness<int64_t>();
    if (pID == 0) std::cout << "Local Randomness...OK" << std::endl;

    // test XChaCha20
    // unsigned
    test_xchacha20_randomness<uint32_t>();
    test_xchacha20_randomness<uint64_t>();
    // signed
    test_xchacha20_randomness<int32_t>();
    test_xchacha20_randomness<int64_t>();
    if (pID == 0) std::cout << "XChaCha20 randomness...OK" << std::endl;

    // test automatic group generation
    test_group_generation();
    if (pID == 0) std::cout << "Automatic Group Generation...OK" << std::endl;

    // test common randomness
    if (num_parties == 3) {
        test_3pc_common_prg_correctness<int32_t>();
        test_3pc_common_prg_correctness<int64_t>();
    } else if (num_parties == 4) {
        test_4pc_common_prg_correctness<int32_t>();
        test_4pc_common_prg_correctness<int64_t>();
    }
    if (pID == 0) std::cout << "Common Randomness...OK" << std::endl;

    // test common group randomness
    test_common_prg_correctness_groups<int32_t>();
    test_common_prg_correctness_groups<int64_t>();
    if (pID == 0) std::cout << "Common Group Randomness...OK" << std::endl;

    // test random zero sharing
    test_zero_sharing_generator_correctness<int32_t>();
    test_zero_sharing_generator_correctness<int64_t>();
    if (pID == 0) std::cout << "Zero Sharings...OK" << std::endl;

    // test group random zero sharing
    test_zero_sharing_generator_groups<int32_t>();
    test_zero_sharing_generator_groups<int64_t>();
    if (pID == 0) std::cout << "Group Zero Sharings...OK" << std::endl;

    // Tear down communication

    return 0;
}