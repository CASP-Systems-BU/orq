#include "../include/secrecy.h"

using namespace secrecy::debug;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char **argv) {
    secrecy_init(argc, argv);

    // Testing Vector elements operations

    // Testing Vector Patterns
    secrecy::Vector<int> vec_pattern_1 = {0, 1, 2, 3, 4, 5, 6, 7};

    secrecy::Vector<int> vec_pattern_2 = vec_pattern_1.simple_subset_reference(0);
    assert(vec_pattern_2.same_as(secrecy::Vector<int>({0, 1, 2, 3, 4, 5, 6, 7})));

    secrecy::Vector<int> vec_pattern_3 = vec_pattern_1.simple_subset_reference(0, 2);
    assert(vec_pattern_3.same_as(secrecy::Vector<int>({0, 2, 4, 6})));

    secrecy::Vector<int> vec_pattern_4 = vec_pattern_1.alternating_subset_reference(2,2)
            .simple_subset_reference(0, 2, 2);
    assert(vec_pattern_4.same_as(secrecy::Vector<int>({0, 4})));

    secrecy::Vector<int> vec_pattern_5 = vec_pattern_1.simple_subset_reference(0, 1, 3);
    assert(vec_pattern_5.same_as(secrecy::Vector<int>({0, 1, 2, 3})));

    secrecy::Vector<int> vec_pattern_6 = vec_pattern_1.alternating_subset_reference(2, 2);
    assert(vec_pattern_6.same_as(secrecy::Vector<int>({0, 1, 4, 5})));

    secrecy::Vector<int> vec_pattern_7 = vec_pattern_1.reversed_alternating_subset_reference(2,2);
    assert(vec_pattern_7.same_as(secrecy::Vector<int>({1, 0, 5, 4})));

    secrecy::Vector<int> vec_pattern_8 = vec_pattern_1.simple_subset_reference(2,1)
            .reversed_alternating_subset_reference(2,2);
    assert(vec_pattern_8.same_as(secrecy::Vector<int>({3, 2, 7, 6})));

    secrecy::Vector<int> vec_pattern_9 = vec_pattern_1.simple_subset_reference(3,1)
            .reversed_alternating_subset_reference(3,1);
    assert(vec_pattern_9.same_as(secrecy::Vector<int>({5, 4, 3, 7})));

    secrecy::Vector<int> vec_pattern_10 = vec_pattern_1.directed_subset_reference(-1);
    assert(vec_pattern_10.same_as(secrecy::Vector<int>({7, 6, 5, 4, 3, 2, 1, 0})));

    secrecy::Vector<int> vec_pattern_11 = vec_pattern_1.simple_subset_reference(0, 2)
            .repeated_subset_reference(2);
    assert(vec_pattern_11.same_as(secrecy::Vector<int>({0, 0, 2, 2, 4, 4, 6, 6})));

    secrecy::Vector<int> vec_pattern_12 = vec_pattern_1.cyclic_subset_reference(2);
    assert(vec_pattern_12.same_as(secrecy::Vector<int>({0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7})));

    secrecy::Vector<int> vec_pattern_13 = vec_pattern_1.simple_subset_reference(1,1)
            .reversed_alternating_subset_reference(2,4)
            .repeated_subset_reference(3)
            .cyclic_subset_reference(2);
    assert(vec_pattern_13.same_as(secrecy::Vector<int>({2, 2, 2, 1, 1, 1, 7, 7, 7, 2, 2, 2, 1, 1, 1, 7, 7, 7})));

    secrecy::Vector<int> vec_pattern_14 = vec_pattern_1.simple_subset_reference(1,1)
            .reversed_alternating_subset_reference(2,4)
            .repeated_subset_reference(3)
            .cyclic_subset_reference(2)
            .directed_subset_reference(-1);
    assert(vec_pattern_14.same_as(secrecy::Vector<int>({7, 7, 7, 1, 1, 1, 2, 2, 2, 7, 7, 7, 1, 1, 1, 2, 2, 2})));

    {
        auto v1 = vec_pattern_1.included_reference({0, 1, 1, 0, 0, 1, 1, 1});
        assert(v1.same_as({1, 2, 5, 6, 7}));
        auto v2 = vec_pattern_1.included_reference({1, 0, 1, 0, 1});
        assert(v2.same_as({0, 2, 4}));
        auto empty = vec_pattern_1.included_reference({});
        assert(empty.size() == 0);
    }

    {
        std::vector<size_t> map = {1, 4, 6};
        auto v1 = vec_pattern_1.mapping_reference(map);
        assert(v1.same_as({1, 4, 6}));

        secrecy::Vector<int> map2 = {0, 3, 5};
        auto v2 = vec_pattern_1.mapping_reference(map2);
        assert(v2.same_as({0, 3, 5}));

        auto repeated = vec_pattern_1.repeated_subset_reference(10);
        // apparent size should change
        assert(repeated.size() == 10 * vec_pattern_1.size());
        // but internal size does not
        assert(repeated._get_internal_data().size() == vec_pattern_1.size());
        assert(repeated.has_mapping());
        auto mat = repeated.materialize();
        // after materializing, size agrees, and no more mapping
        assert(mat.size() == repeated.size());
        assert(mat._get_internal_data().size() == repeated.size());
        assert(! mat.has_mapping());

        secrecy::Vector<int> v3(vec_pattern_1.size());
        v3 = vec_pattern_1;
        
        v3.apply_mapping({0, 2, 4, 1, 3, 5, 7, 6});
        assert(! v3.same_as(vec_pattern_1, false));
        // inverse map.
        v3.apply_mapping({0, 3, 1, 4, 2, 5, 7, 6});
        assert(v3.same_as(vec_pattern_1));
    }

    single_cout("Vector-Patterns...ok");

    // Basic ranges
    assert(std::ranges::equal(vec_pattern_1, std::vector({0, 1, 2, 3, 4, 5, 6, 7})));

    // All random-access iterator methods on a mapped Vector
    auto iter14 = vec_pattern_14.begin();
    assert(*iter14 == 7);
    assert(*(iter14 + 3) == 1);
    iter14++;
    ++iter14;
    assert(iter14[4] == 2);
    iter14--;
    --iter14;
    assert(vec_pattern_14.end() - iter14 == 18);

    // Ranges over mapped Vector
    assert(std::ranges::equal(vec_pattern_14, std::vector({7, 7, 7, 1, 1, 1, 2, 2, 2, 7, 7, 7, 1, 1, 1, 2, 2, 2})));

    single_cout("Vector-iterators...ok");

    std::vector<int> v = vec_pattern_14.as_std_vector();
    assert(std::ranges::equal(v, std::vector({7, 7, 7, 1, 1, 1, 2, 2, 2, 7, 7, 7, 1, 1, 1, 2, 2, 2})));
    assert(secrecy::Vector<int>(0).as_std_vector().size() == 0);

    single_cout("Vector-conversion...ok");

    // Testing bit manipulation
    secrecy::Vector<int> vec_bit_01 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    secrecy::Vector<int> vec_bit_02 = vec_bit_01.simple_bit_compress(0, 1, 0, 1);
    secrecy::Vector<int> vec_bit_03 = vec_bit_01.simple_bit_compress(0, 1, 0, 2);
    secrecy::Vector<int> vec_bit_04 = vec_bit_01.simple_bit_compress(1, 1, 1, 1);
    secrecy::Vector<int> vec_bit_05 = vec_bit_01.simple_bit_compress(1, 1, 1, 2);
    secrecy::Vector<int> vec_bit_06 = vec_bit_01.simple_bit_compress(0, 1, 3, 1);
    secrecy::Vector<int> vec_bit_07 = vec_bit_01.simple_bit_compress(0, 1, 3, 2);
    secrecy::Vector<int> vec_bit_08 = vec_bit_01.simple_bit_compress(0, 2, 3, 1);
    secrecy::Vector<int> vec_bit_09 = vec_bit_01.simple_bit_compress(0, 2, 3, 2);

    secrecy::Vector<int> vec_bit_11 = {0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f};

    secrecy::Vector<int> vec_bit_12 = vec_bit_11.alternating_bit_compress(0, 1, 2, 30, 1);
    assert(vec_bit_12.same_as(secrecy::Vector<int>({0x000000ff})));

    secrecy::Vector<int> vec_bit_13 = vec_bit_11.alternating_bit_compress(0, 1, 4, 4, 1);
    assert(vec_bit_13.same_as(secrecy::Vector<int>({(int) 0xffffffff, (int) 0xffffffff})));

    secrecy::Vector<int> vec_bit_14 = vec_bit_11.alternating_bit_compress(0, 8, 8, 8, 1);
    assert(vec_bit_14.same_as(secrecy::Vector<int>({0x000000ff})));

    secrecy::Vector<int> vec_bit_15 = vec_bit_11.alternating_bit_compress(0, 8, 8, 8, -1);
    assert(vec_bit_15.same_as(secrecy::Vector<int>({0x00000000})));


    // if (secrecy::service::runTime->getPartyID() == 0) {
    //     print_binary(vec_bit_01, 0);
    //     print_binary(vec_bit_02, 0);
    //     print_binary(vec_bit_03, 0);
    //     print_binary(vec_bit_04, 0);
    //     print_binary(vec_bit_05, 0);
    //     print_binary(vec_bit_06, 0);
    //     print_binary(vec_bit_07, 0);
    //     print_binary(vec_bit_08, 0);
    //     print_binary(vec_bit_09, 0);
    //
    //     print_binary(vec_bit_11, 0);
    //     print_binary(vec_bit_12, 0);
    //     print_binary(vec_bit_13, 0);
    //     print_binary(vec_bit_14, 0);
    //     print_binary(vec_bit_15, 0);
    // }

    single_cout("Vector-bit...ok");

#if defined(MPC_USE_MPI_COMMUNICATOR)
    MPI_Finalize();
#endif

    return 0;
}
