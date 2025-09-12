#pragma once

#include "../pooled/pooled_generator.h"
#include "core/containers/e_vector.h"
#include "correlation_generator.h"
#include "debug/orq_debug.h"
#include "gilboa_ole.h"
#include "ole_generator.h"
#include "profiling/stopwatch.h"
#include "silent_ot.h"
using namespace orq::benchmarking;

#include <stdlib.h>

#include <variant>

const int MAX_TRIPLES_RESERVE_BATCH = 1 << 24;

namespace orq::random {
// Really this should be a SharedVector, but maybe not accessible yet
template <typename T>
using S = EVector<T, 1>;

/**
 * @brief Generates Beaver multiplication triples.
 *
 * @tparam T The data type for the triple elements
 * @tparam E The encoding type (BShared or AShared)
 */
template <typename T, orq::Encoding E>
class BeaverTripleGenerator : public CorrelationGenerator {
    using triple_t = std::tuple<S<T>, S<T>, S<T>>;
    using vec_t = Vector<T>;

    bool pooled;

    OLEGenerator<T, E>* vg;
    std::variant<std::shared_ptr<PooledGenerator<GilboaOLE<T>, T, T>>,
                 std::shared_ptr<PooledGenerator<SilentOT<T>, T, T>>>
        pg;

    std::optional<Communicator*> comm;

    /**
     * A wrapper around getNext that works with either an OLEGenerator or a
     * PooledGenerator.
     * @param n The number of triples to get.
     */
    auto generatorGetNext(size_t n) {
        if (pooled) {
            return std::visit([n](auto& p) { return p->getNext(n); }, pg);
        } else {
            return vg->getNext(n);
        }
    }

   public:
    /**
     * Constructor with OLE generator.
     * @param v The OLE generator to use.
     */
    BeaverTripleGenerator(OLEGenerator<T, E>* v)
        : vg(v), CorrelationGenerator(v->getRank()), comm(v->comm), pooled(false) {}

    /**
     * Constructor that takes a PooledGenerator of an OLEGenerator instead of an OLEGenerator
     * itself.
     * @param p shared_ptr to a PooledGenerator object.
     * @param _comm Optional communicator object for correctness tests.
     */
    template <typename OLEGenerator_t>
    BeaverTripleGenerator(std::shared_ptr<PooledGenerator<OLEGenerator_t, T, T>> p,
                          std::optional<Communicator*> _comm = std::nullopt)
        : pg(p), CorrelationGenerator(p->getRank()), comm(_comm), pooled(true) {}

    /**
     * A function to generate Beaver triples and store them for use later.
     * @param n The number of triples to generate.
     */
    void reserve(size_t n) {
        if (!pooled) {
            if (getRank() == 0) {
                std::cout
                    << "Attempted to pool triples on a triple generator without a pooled generator."
                    << std::endl;
            }
            return;
        }

        size_t remaining = n;
        while (remaining > MAX_TRIPLES_RESERVE_BATCH) {
            std::visit([n](auto& p) { p->reserve(2 * MAX_TRIPLES_RESERVE_BATCH); }, pg);
            remaining -= MAX_TRIPLES_RESERVE_BATCH;
        }

        if (remaining > 0) {
            std::visit([n, remaining](auto& p) { p->reserve(2 * remaining); }, pg);
        }
    }

    /**
     * Generate Beaver triples.
     * @param n The number of triples to generate.
     * @return A tuple of three vectors representing the Beaver triple.
     */
    triple_t getNext(size_t n) {
        auto party0 = getRank() == 0;
        // Get two OLEs.
        // These are tuples of Vector<T>, with the following layout
        //   A   +   B   =   C   *   D
        // P0 #0   P1 #0   P0 #1   P1 #1
        //
        // or, P0: {A, C}
        //     P1: {B, D}
        //
        // However, for efficiency purposes, just get a OLE of twice the
        // length, and then chop it up.
        auto [vAB, vCD] = generatorGetNext(2 * n);
        // Chop up the A (P0) / B (P1)
        auto vAB_left = vAB.slice(0, n);
        auto vAB_right = vAB.slice(n, 2 * n);

        // Chop up the C (P0) / D (P1)
        auto vCD_left = vCD.slice(0, n);
        auto vCD_right = vCD.slice(n, 2 * n);

        // P1 is reversed from P0, so we get cross terms.
        vec_t a_ = (party0 ? vCD_left : vCD_right);
        vec_t b_ = (party0 ? vCD_right : vCD_left);

        // Use a lambda so we don't need to initialize the vector and then
        // copy into it.
        vec_t c_ = [&]() {
            // This will be optimized out at compile type, since it's from
            // the template.
            if constexpr (E == orq::Encoding::BShared) {
                return (a_ & b_) ^ vAB_left ^ vAB_right;
            } else {
                // This is all _local_ multiplication.
                return a_ * b_ + vAB_left + vAB_right;
            }
        }();

        // Cast to EVector on return so we can use SharedVector constructor
        // in user code
        return {std::vector<vec_t>({a_}), std::vector<vec_t>({b_}), std::vector<vec_t>({c_})};
    }

    /**
     * @brief Check the beaver triple is correct.
     *
     * Since `CorrelationGenerator`s don't have access to the runtime, we
     * need to manually "open" the shared vector here.
     *
     * @param bt
     */
    void assertCorrelated(triple_t bt) {
        auto [my_a, my_b, my_c] = bt;

        auto n = my_a.size();

        Vector<T> other_a(n), other_b(n), other_c(n);

        if (!comm.has_value()) {
            if (getRank() == 0) {
                std::cout << "Skipping BT check: communicator not defined\n";
            }
            return;
        }

        auto communicator = comm.value();

        my_a.materialize_inplace();
        my_b.materialize_inplace();
        my_c.materialize_inplace();

        communicator->exchangeShares(my_a(0), other_a, 1, n);
        communicator->exchangeShares(my_b(0), other_b, 1, n);
        communicator->exchangeShares(my_c(0), other_c, 1, n);

        if constexpr (E == orq::Encoding::BShared) {
            auto a = my_a(0) ^ other_a;
            auto b = my_b(0) ^ other_b;
            auto c = my_c(0) ^ other_c;

            assert(!a.same_as(b, false));
            assert(c.same_as(a & b));
        } else {
            auto a = my_a(0) + other_a;
            auto b = my_b(0) + other_b;
            auto c = my_c(0) + other_c;

            assert(!a.same_as(b, false));
            assert(c.same_as(a * b));
        }
    }
};

// Template specialization
template <typename T>
struct CorrelationEnumType<T, Correlation::BeaverMulTriple> {
    using type = BeaverTripleGenerator<T, Encoding::AShared>;
};

template <typename T>
struct CorrelationEnumType<T, Correlation::BeaverAndTriple> {
    using type = BeaverTripleGenerator<T, Encoding::BShared>;
};
}  // namespace orq::random
