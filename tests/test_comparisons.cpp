#include "orq.h"

using namespace orq::debug;
using namespace orq::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char** argv) {
    orq_init(argc, argv);
    // The party's unique id
    auto pID = runTime->getPartyID();

    const int MAX_BITS_NUMBER = std::numeric_limits<int>::digits;

    // Input plaintext data for testing secure comparisons
    orq::Vector<int> data_a = {111, -4, -17, 2345, 999, 0, -28922, 1231241, 0, -23437};
    orq::Vector<int> data_b = {0, -4, -5, INT_MIN, 999, 70, -243242, INT_MAX, 0, 78};

    // TODO (john): Add tests with different data types, e.g., unsigned, and check corner cases
    //    orq::Vector<int> data_a = {1 << (MAX_BITS_NUMBER-1)};
    //    orq::Vector<int> data_b = {0};

    assert(data_a.size() == data_b.size());

    // **************************************** //
    //           Comparison primitives          //
    // **************************************** //

    // Secret-share original vectors using boolean sharing
    BSharedVector<int> b_v1 = secret_share_b(data_a, 0);
    BSharedVector<int> b_v2 = secret_share_b(data_b, 0);

    // Apply elementwise secure equality
    BSharedVector<int> c_eq = (b_v1 == b_v2);
    // Open equality bits
    auto c_eq_open = c_eq.open();
    // Get ground truth
    orq::Vector<int> eq_bits = (data_a == data_b);

    // Compare equality bits with ground truth
    assert(c_eq_open.same_as(eq_bits));

    if (pID == 0) std::cout << "Equality...OK" << std::endl;

    // Apply elementwise secure greater-than
    BSharedVector<int> c_gr = (b_v1 > b_v2);
    // Open greater-than bits
    auto c_gr_open = c_gr.open();
    // Get ground truth
    orq::Vector<int> gr_bits = (data_a > data_b);

    // Compare greater-than bits with ground truth
    assert(c_gr_open.same_as(gr_bits));

    if (pID == 0) std::cout << "Greater-than...OK" << std::endl;

    // Apply elementwise secure greater-or-equal
    BSharedVector<int> c_geq = (b_v1 >= b_v2);
    // Open greater-or-equal bits
    auto c_geq_open = c_geq.open();
    // Get ground truth
    orq::Vector<int> geq_bits = (data_a >= data_b);

    // Compare greater-or_equal bits with ground truth
    assert(c_geq_open.same_as(geq_bits));

    if (pID == 0) std::cout << "Greater-or-equal...OK" << std::endl;

    // Apply elementwise secure less-than
    BSharedVector<int> c_lt = (b_v1 < b_v2);
    // Open less-than bits
    auto c_lt_open = c_lt.open();
    // Get ground truth
    orq::Vector<int> lt_bits = (data_a < data_b);

    // Compare less-than bits with ground truth
    assert(c_lt_open.same_as(lt_bits));

    if (pID == 0) std::cout << "Less-than...OK" << std::endl;

    // Apply elementwise secure less-or-equal
    BSharedVector<int> c_leq = (b_v1 <= b_v2);
    // Open less-or-equal bits
    auto c_leq_open = c_leq.open();
    // Get ground truth
    orq::Vector<int> leq_bits = (data_a <= data_b);

    // Compare less-or-equal bits with ground truth
    assert(c_leq_open.same_as(leq_bits));

    if (pID == 0) std::cout << "Less-or-equal...OK" << std::endl;

    // Apply elementwise secure less-than-zero
    BSharedVector<int> ltz = b_v1.ltz();
    // Open less-than-zero bits
    auto ltz_open = ltz.open();
    // Get ground truth
    orq::Vector<int> zeros(data_a.size());
    // TODO (john): Add support for elementwise comparisons with constant, i.e., "data_a < 0"
    orq::Vector<int> ltz_bits = (data_a < zeros);
    // Compare less-than-zero bits with ground truth
    assert(ltz_open.same_as(ltz_bits));

    if (pID == 0) std::cout << "Less-than-zero...OK" << std::endl;

    // **************************************** //
    //             compare function             //
    // **************************************** //

    single_cout_nonl("Çompare (combo =/>) function... ");
    {
        orq::Vector<int> x = {30670,  0,      -14730, 25788, -50441, INT_MAX, -19964, INT_MAX,
                              -13582, -64520, 27218,  0,     34184,  INT_MIN, -65282, INT_MIN};
        orq::Vector<int> y = {-15260, 0,      -32847,  30024,  -6530,  INT_MAX, -1536, 29620,
                              -26019, -64520, INT_MIN, -15059, -34111, -7765,   56837, INT_MIN};

        BSharedVector<int> sx = secret_share_b(x, 0);
        BSharedVector<int> sy = secret_share_b(y, 0);

        BSharedVector<int> eq(sx.size());
        BSharedVector<int> gt(sx.size());

        auto eq_truth = (x == y);
        auto gt_truth = (x > y);

        sx._compare(sy, eq, gt);

        auto eq_open = eq.open();
        auto gt_open = gt.open();

        assert(eq_open.same_as(eq_truth));
        assert(gt_open.same_as(gt_truth));
    }
    single_cout("OK");

    // Tear down communication

    return 0;
}