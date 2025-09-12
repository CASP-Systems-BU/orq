#include "orq.h"

using namespace orq::debug;
using namespace orq::service;
using namespace orq::aggregators;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

template <typename T>
auto bench_prefix_sum_internal(T x) {
    auto n = x.size();
    T w(n), y(n), z(n);

    w = x;
    y = x;
    z = x;

    // T yi(1), yp(1);

    stopwatch::timepoint("--");
    w.prefix_sum();
    stopwatch::timepoint("Direct EVector");

    runTime->worker0->proto_32->mark_statistics();
    for (int i = 1; i < y.size(); i++) {
        // currently xi
        auto yi = y.slice(i, i + 1);
        auto yp = y.slice(i - 1, i);
        yi += yp;
    }
    stopwatch::timepoint("AP Linear");
    runTime->print_statistics();

    runTime->worker0->proto_32->mark_statistics();
    tree_prefix_sum(z);
    stopwatch::timepoint("Tree");
    runTime->print_statistics();

    Vector<int> z_(x.size());
    if constexpr (std::is_base_of<orq::EncodedVector, T>::value) {
        z_ = z.open();
        assert(z_.same_as(y.open()));
        assert(z_.same_as(w.open()));
    } else {
        z_ = z;
        assert(z.same_as(y));
        assert(z.same_as(w));
    }

    return z_;
}

void bench_prefix_sum(int i) {
    auto N = 1 << i;
    single_cout("Benchmark size 2^" << i << " (" << N << ")");
    auto pid = runTime->getPartyID();

    Vector<int> x(N);
    runTime->populateLocalRandom(x);
    Vector<int> y(N);
    y = x;

    single_cout("== Plaintext ==");

    // Plaintext
    if (pid == 0) {
        bench_prefix_sum_internal(x);
    }

    single_cout("== AShared ==");

    ASharedVector<int> ash_x = secret_share_a(x, 0);
    ASharedVector<int> pf(ash_x.size());
    ASharedVector<int> sum(1);

    stopwatch::timepoint("Begin");

    for (int j = 0; j < ash_x.vector.replicationNumber; j++) {
        for (int i = 0; i < ash_x.size(); i++) {
            sum.vector(j)[0] += ash_x.vector(j)[i];
            pf.vector(j)[i] = sum.vector(j)[0];
        }
    }

    stopwatch::timepoint("Manual");

    bench_prefix_sum_internal(ash_x);

    // single_cout("== BShared ==");

    // BSharedVector<int> bsh_x = secret_share_b(x, 0);
    // bench_prefix_sum_internal(bsh_x);
}

int main(int argc, char** argv) {
    orq_init(argc, argv);

    bench_prefix_sum(20);
}