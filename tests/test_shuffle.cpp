#include "../include/secrecy.h"
#include "../include/core/random/permutations/permutation_manager.h"

#include <iostream>

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

using Group = std::set<int>;
using LocalPermutation = std::vector<int>;


// **************************************** //
//          Test Shuffle GenPerm            //
// **************************************** //
// this test checks that the output is actually a sorting permutation of a single bit
void test_shuffle_gen_perm(int test_size) {
    auto rank = runTime->getPartyID();

    // generate a permutation for each group
    for (std::set<int> group : runTime->getGroups()) {
        if (!group.contains(rank))
            continue;

        auto common_prg = runTime->rand0()->commonPRGManager->get(group);

        // generate the permutation
        std::vector<int> permutation = secrecy::operators::gen_perm(test_size, common_prg);

        int lowestRank = *group.begin();
        if (rank == lowestRank) {
            // lowest rank receives every other party's vector
            for (int otherRank : group) {
                if (rank == otherRank)
                    continue;
                int relative_rank = otherRank - rank;
                Vector<int> remote(test_size);
                runTime->comm0()->exchangeShares(permutation, remote, relative_rank, relative_rank, test_size);

                // check correctness
                for (int j = 0; j < test_size; j++) {
                    assert(permutation[j] == remote[j]);
                }
            }
        } else {
            // just exchange with lowest rank, check equality
            int relative_rank = lowestRank - rank;
            Vector<int> remote(test_size);
            runTime->comm0()->exchangeShares(permutation, remote, relative_rank, relative_rank, test_size);
            
            // check correctness
            for (int j = 0; j < test_size; j++) {
                assert(permutation[j] == remote[j]);
            }
        }
    }
}

// **************************************** //
//      Test Shuffle Local ApplyPerm        //
// **************************************** //
// this test checks that the local permutation passed is correctly applied
void test_shuffle_local_apply_perm(int test_size) {
    auto rank = runTime->getPartyID();

    // generate a test vector
    secrecy::Vector<int> x(test_size);
    for (int i = 0; i < test_size; i++) {
        x[i] = i;
    }

    // generate and apply a permutation for each group
    for (std::set<int> group : runTime->getGroups()) {
        BSharedVector<int> b = secret_share_b(x, 0);
        BSharedVector<int> b_inv = secret_share_b(x, 0);
        std::vector<int> permutation;
        if (group.contains(rank)) {
            auto common_prg = runTime->rand0()->commonPRGManager->get(group);
            
            // generate the permutation
            permutation = secrecy::operators::gen_perm(test_size, common_prg);

            // apply the permutation
            secrecy::operators::local_apply_perm(b, permutation);

            // apply the inverse permutation
            secrecy::operators::local_apply_perm(b_inv, permutation);
            secrecy::operators::local_apply_inverse_perm(b_inv, permutation);

            b_inv.vector.materialize_inplace();
            b.vector.materialize_inplace();
        }
        // reshare
        secrecy::service::runTime->reshare(b.vector, group, true);
        secrecy::service::runTime->reshare(b_inv.vector, group, true);

        secrecy::Vector<int> y_b = b.open();
        secrecy::Vector<int> y_b_inv = b_inv.open();
        
        if (group.contains(rank)) {
            // check that the permutation has been applied correctly
            for (int i = 0; i < test_size; i++) {
                assert(y_b[permutation[i]] == i);
                assert(y_b_inv[i] == i);
            }
        }
    }
}

// **************************************** //
//    Test Oblivious Apply Sharded Perm     //
// **************************************** //
void test_shuffle_oblivious_apply_sharded_perm(int test_size) {
    auto rank = runTime->getPartyID();

    // generate a test vector
    secrecy::Vector<int> x(test_size);
    for (int i = 0; i < test_size; i++) {
        x[i] = i;
    }
    ASharedVector<int> a = secret_share_a(x, 0);
    BSharedVector<int> b = secret_share_b(x, 0);

    auto groups = secrecy::service::runTime->getGroups();

    std::shared_ptr<secrecy::random::ShardedPermutation> perm_a = secrecy::random::PermutationManager::get()->getNext<int>(test_size, secrecy::Encoding::AShared);
    std::shared_ptr<secrecy::random::ShardedPermutation> perm_b = secrecy::random::PermutationManager::get()->getNext<int>(test_size, secrecy::Encoding::BShared);

    // apply the permutation
    oblivious_apply_sharded_perm(a, perm_a);
    oblivious_apply_sharded_perm(b, perm_b);
    // check that the vector was actually permuted by checking that at least 90% of the values are different
    auto permuted_a = a.open();
    auto permuted_b = b.open();
    int count_identical_a = 0;
    int count_identical_b = 0;
    int threshold_identical = test_size / 10;
    for (int i = 0; i < test_size; i++) {
        if (permuted_a[i] == i) {
            count_identical_a++;
        }
        if (permuted_b[i] == i) {
            count_identical_b++;
        }
    }
    assert(count_identical_a <= threshold_identical);
    assert(count_identical_b <= threshold_identical);

    auto opened_a = a.open();
    auto opened_b = b.open();
    for (int i = 0; i < test_size; i++) {
        assert(opened_a[i] < test_size);
        assert(opened_b[i] < test_size);
    }

    // apply the inverse permutation
    oblivious_apply_inverse_sharded_perm(a, perm_a);
    oblivious_apply_inverse_sharded_perm(b, perm_b);
    // check that the result is the original vector
    auto unpermuted_a = a.open();
    auto unpermuted_b = b.open();
    for (int i = 0; i < test_size; i++) {
        assert(unpermuted_a[i] == i);
        assert(unpermuted_b[i] == i);
    }
}

// **************************************** //
//  Test Oblivious Apply Elementwise Perm   //
// **************************************** //
void test_oblivious_apply_elementwise_perm(int test_size) {
    // generate a test vector
    secrecy::Vector<int> x(test_size);
    for (int i = 0; i < test_size; i++) {
        x[i] = i;
    }

    // apply binary to arithmetic
    ASharedVector<int> a1 = secret_share_a(x, 0);
    ASharedVector<int> a2 = secret_share_a(x, 0);
    secrecy::ElementwisePermutation<secrecy::EVector<int, a1.vector.replicationNumber>> perm1(test_size, secrecy::Encoding::BShared);
    perm1.shuffle();

    // check correctness against local
    Vector<int> local_perm1 = perm1.open();
    for (int i = 0; i < test_size; i++) {
        assert(local_perm1[i] < test_size);
    }

    secrecy::operators::oblivious_apply_elementwise_perm(a1, perm1);
    secrecy::operators::local_apply_perm(a2, local_perm1);

    auto a1_opened = a1.open();
    auto a2_opened = a2.open();

    for (int i = 0; i < a1_opened.size(); i++) {
        assert(a1_opened[i] == a2_opened[i]);
    }
    
    // apply arithmetic to binary
    BSharedVector<int> b1 = secret_share_b(x, 0);
    BSharedVector<int> b2 = secret_share_b(x, 0);
    secrecy::ElementwisePermutation<secrecy::EVector<int, b1.vector.replicationNumber>> perm2(test_size, secrecy::Encoding::AShared);
    perm2.shuffle();
    
    // check correctness against local
    Vector<int> local_perm2 = perm2.open();

    secrecy::operators::oblivious_apply_elementwise_perm(b1, perm2);
    secrecy::operators::local_apply_perm(b2, local_perm2);

    auto b1_opened = b1.open();
    auto b2_opened = b2.open();

    for (int i = 0; i < b1_opened.size(); i++) {
        assert(b1_opened[i] == b2_opened[i]);
    }
}

// **************************************** //
//           Test Reverse Perm              //
// **************************************** //
void test_reverse_elementwise_permutation(int test_size) {
    // generate a test vector
    secrecy::Vector<int> x(test_size);
    for (int i = 0; i < test_size; i++) {
        x[i] = i;
    }
    ASharedVector<int> a = secret_share_a(x, 0);

    // begin test
    secrecy::ElementwisePermutation<secrecy::EVector<int, a.vector.replicationNumber>> e(test_size);
    
    e.shuffle();
    auto opened_before = e.open();

    e.reverse();
    auto opened_after = e.open();

    for (int i = 0; i < test_size; i++) {
        assert((opened_before[i] + opened_after[i]) == (test_size - 1));
    }
}

// **************************************** //
//    Test Elementwise Permutation B2A      //
// **************************************** //
void test_elementwise_permutation_b2a_conversion(int test_size) {
    // generate a test vector
    secrecy::Vector<int> x(test_size);
    for (int i = 0; i < test_size; i++) {
        x[i] = i;
    }
    BSharedVector<int> b = secret_share_b(x, 0);

    secrecy::ElementwisePermutation<secrecy::EVector<int, b.vector.replicationNumber>>
        perm(test_size, secrecy::Encoding::BShared);

    perm.shuffle();

    auto opened_b = perm.open();

    perm.b2a();

    auto opened_a = perm.open();

    for (int i = 0; i < test_size; i++) {
        assert(opened_a[i] == opened_b[i]);
    }
}

// **************************************** //
//   Test Invert Elementwise Permutation    //
// **************************************** //
void test_invert_elementwise_permutation(int test_size) {
    // generate a test vector
    secrecy::Vector<int> x(test_size);
    for (int i = 0; i < test_size; i++) {
        x[i] = i;
    }
    BSharedVector<int> b = secret_share_b(x, 0);
    
    secrecy::ElementwisePermutation<secrecy::EVector<int, b.vector.replicationNumber>>
        perm(test_size, secrecy::Encoding::BShared);

    perm.shuffle();

    // apply the permutation to the vector
    secrecy::operators::oblivious_apply_elementwise_perm(b, perm);

    // get the inverse permutation and apply it to the vector
    perm.invert();
    secrecy::operators::oblivious_apply_elementwise_perm(b, perm);

    // check that the permutation and its inverse cancel out to the identity
    auto opened = b.open();
    for (int i = 0; i < test_size; i++) {
        assert(opened[i] == i);
    }
}

// **************************************** //
//             Test Resharing               //
// **************************************** //
void test_resharing(int test_size) {
    auto pID = runTime->getPartyID();

    // generate a test vector
    secrecy::Vector<int> x(test_size);
    for (int i = 0; i < test_size; i++) {
        x[i] = i;
    }
    ASharedVector<int> a = secret_share_a(x, 0);
    BSharedVector<int> b = secret_share_b(x, 0);

    secrecy::Vector<int> old_a(test_size);
    secrecy::Vector<int> old_b(test_size);
    old_a = a.vector(0);
    old_b = b.vector(0);

    for (std::set<int> group : runTime->getGroups()) {
        runTime->reshare(a.vector, group, false);
        runTime->reshare(b.vector, group, true);
    }

    secrecy::Vector<int> new_a(test_size);
    secrecy::Vector<int> new_b(test_size);
    new_a = a.vector(0);
    new_b = b.vector(0);

#ifndef MPC_PROTOCOL_PLAINTEXT_ONE
    // check that randomization actually occurred
    assert(! old_a.same_as(new_a, false));
    assert(! old_b.same_as(new_b, false));
#endif

    // check that the result still opens to the correct values
    auto result_a = a.open();
    auto result_b = b.open();

    assert(result_a.same_as(x));
    assert(result_b.same_as(x));
}

// **************************************** //
//        Test Shuffle Correctness          //
// **************************************** //
template <typename T>
void test_shuffle_correctness(int test_size) {
    // generate a test vector
    secrecy::Vector<T> x(test_size);
    for (int i = 0; i < test_size; i++) {
        x[i] = i;
    }
    ASharedVector<T> a = secret_share_a(x, 0);
    BSharedVector<T> b = secret_share_b(x, 0);

    a.shuffle();
    b.shuffle();

    secrecy::Vector<T> opened_a = a.open();
    secrecy::Vector<T> opened_b = b.open();

    // check that the permutations are shared between parties correctly
    // this is fine for any number of parties because it just makes sure everybody agrees with their successor and predecessor
    //      and if everybody does, then all parties must be in agreement
    secrecy::Vector<T> shared_perm_prev_a(test_size);
    secrecy::Vector<T> shared_perm_next_a(test_size);
    runTime->comm0()->exchangeShares(opened_a, shared_perm_next_a, -1, +1, test_size);
    runTime->comm0()->exchangeShares(opened_a, shared_perm_prev_a, +1, -1, test_size);
    secrecy::Vector<T> shared_perm_prev_b(test_size);
    secrecy::Vector<T> shared_perm_next_b(test_size);
    runTime->comm0()->exchangeShares(opened_b, shared_perm_next_b, -1, +1, test_size);
    runTime->comm0()->exchangeShares(opened_b, shared_perm_prev_b, +1, -1, test_size);
    
    for (int i = 0; i < test_size; i++) {
        assert(opened_a[i] == shared_perm_prev_a[i]);
        assert(opened_a[i] == shared_perm_next_a[i]);
        assert(opened_b[i] == shared_perm_prev_b[i]);
        assert(opened_b[i] == shared_perm_next_b[i]);
    }

    // check that it is still a permutation of [n] after shuffling
    std::vector<T> opened_vec_a(test_size);
    std::vector<T> opened_vec_b(test_size);
    for (int i = 0; i < test_size; i++) {
        opened_vec_a[i] = opened_a[i];
        opened_vec_b[i] = opened_b[i];
    }
    std::sort(opened_vec_a.begin(), opened_vec_a.end());
    std::sort(opened_vec_b.begin(), opened_vec_b.end());
    for (int i = 0; i < test_size; i++) {
        assert(opened_vec_a[i] == i);
        assert(opened_vec_b[i] == i);
    }
}

// **************************************** //
//      Test Permutation Composition        //
// **************************************** //
void test_permutation_composition(int test_size) {
    auto pID = runTime->getPartyID();

    // generate a test vector
    secrecy::Vector<int> x(test_size);
    for (int i = 0; i < test_size; i++) {
        x[i] = i;
    }

    BSharedVector<int> v1 = secret_share_b(x, 0);
    BSharedVector<int> v2 = secret_share_b(x, 0);
    BSharedVector<int> v3 = secret_share_b(x, 0);

    // generate the permutation
    secrecy::ElementwisePermutation<secrecy::EVector<int, v1.vector.replicationNumber>>
        sigma(test_size, secrecy::Encoding::BShared);
    secrecy::ElementwisePermutation<secrecy::EVector<int, v1.vector.replicationNumber>>
        rho(test_size, secrecy::Encoding::BShared);
    
    sigma.shuffle();
    rho.shuffle();

    secrecy::Vector<int> sigma_opened = sigma.open();
    secrecy::Vector<int> rho_opened = rho.open();

    // apply the permutations sequentially locally
    secrecy::operators::local_apply_perm(v1, sigma_opened);
    secrecy::operators::local_apply_perm(v1, rho_opened);

    // apply the permutations sequentially obliviously
    secrecy::operators::oblivious_apply_elementwise_perm(v2, sigma);
    secrecy::operators::oblivious_apply_elementwise_perm(v2, rho);

    // check equivalence of local and oblivious sequential composition
    auto v1_opened = v1.open();
    auto v2_opened = v2.open();
    for (int i = 0; i < v1.size(); i++) {
        assert(v1_opened[i] == v2_opened[i]);
    }

    auto composition = secrecy::operators::compose_permutations(sigma, rho);
    secrecy::operators::oblivious_apply_elementwise_perm(v3, composition);

    auto v3_opened = v3.open();
    for (int i = 0; i < v3.size(); i++) {
        assert(v1_opened[i] == v3_opened[i]);
    }
}

// **************************************** //
//           Test Table Shuffle             //
// **************************************** //
void test_table_shuffle(int num_rows, int num_columns) {
    // generate a table
    std::vector<secrecy::Vector<int>> table_data;
    std::vector<std::string> schema;
    for (int i = 0; i < num_columns; i++) {
        table_data.push_back(secrecy::Vector<int>(num_rows));
        // make even columns binary and odd columns arithmetic
        if (i % 2 == 0) {
            schema.push_back("[" + std::to_string(i) + "]");
        } else {
            schema.push_back(std::to_string(i));
        }
        for (int j = 0; j < num_rows; j++) {
            table_data[i][j] = j;
        }
    }
    EncodedTable<int> table = secret_share(table_data, schema);
    
    table.shuffle();

    // open
    std::vector<secrecy::Vector<int>> opened = table.open();
    // make sure all columns have the same value for each row
    for (int i = 0; i < num_rows; i++) {
        int value = opened[0][i];
        for (int j = 1; j < num_columns; j++) {
            assert(opened[j][i] == value);
        }
    }
}


int main(int argc, char** argv) {
    // Initialize Secrecy runtime
    secrecy_init(argc, argv);
    auto pID = runTime->getPartyID();

    int DEFAULT_TEST_SIZE = 100;
    

#ifndef MPC_PROTOCOL_BEAVER_TWO
    DEFAULT_TEST_SIZE = 10000;
    
    test_resharing(DEFAULT_TEST_SIZE);
    single_cout("Resharing...OK");
#endif
    
    // test shuffle permutation generation correctness
    test_shuffle_gen_perm(100);  // non-power of 2
    test_shuffle_gen_perm(1024); // power of 2
    single_cout("Shuffle Permutation Generation...OK");

    test_shuffle_local_apply_perm(DEFAULT_TEST_SIZE);
    single_cout("Local Permutation Application...OK");

    test_shuffle_oblivious_apply_sharded_perm(DEFAULT_TEST_SIZE);
    single_cout("Oblivious Sharded Permutation Application...OK");

    test_oblivious_apply_elementwise_perm(DEFAULT_TEST_SIZE);
    single_cout("Oblivious Elementwise Permutation Application...OK");

    test_reverse_elementwise_permutation(DEFAULT_TEST_SIZE);
    single_cout("Reverse Elementwise Permutation...OK");

    test_elementwise_permutation_b2a_conversion(DEFAULT_TEST_SIZE);
    single_cout("Convert Elementwise Permutation B2A...OK");
    
    test_invert_elementwise_permutation(DEFAULT_TEST_SIZE);
    single_cout("Invert Elementwise Permutation...OK");

    test_shuffle_correctness<int>(DEFAULT_TEST_SIZE);
    test_shuffle_correctness<int64_t>(DEFAULT_TEST_SIZE);
    single_cout("Shuffle Correctness...OK");

    test_permutation_composition(DEFAULT_TEST_SIZE);
    single_cout("Permutation Composition...OK");

    test_table_shuffle(DEFAULT_TEST_SIZE, 4);
    test_table_shuffle(1024, 15);
    single_cout("Table Shuffle...OK");

    // Tear down communication
#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif
    return 0;
}