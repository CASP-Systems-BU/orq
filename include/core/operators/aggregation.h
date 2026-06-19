#pragma once

#include <bit>
#include <optional>
#include <tuple>

#include "../tests/util.h"
#include "common.h"
#include "core/containers/a_shared_vector.h"
#include "core/containers/b_shared_vector.h"
#include "debug/orq_debug.h"
#include "prefix_network.h"

using namespace orq::operators;
using namespace orq::debug;

namespace orq::aggregators {

/**
 * @brief Arithmetic sum aggregation.
 *
 * @tparam A Arithmetic shared vector type.
 *
 * @param group Grouping bits indicating which elements to aggregate.
 * @param a Accumulator vector (modified in place).
 * @param b Input vector to be aggregated.
 */
template <typename A>
void sum(const A& group, A& a, const A& b) {
    a = a + group * b;
}

/**
 * @brief Boolean OR aggregation.
 *
 * @tparam B Boolean shared vector type.
 *
 * @param group Grouping bits indicating which elements to aggregate.
 * @param a Accumulator vector (modified in place).
 * @param b Input vector to be aggregated.
 */
template <typename B>
void bit_or(const B& group, B& a, const B& b) {
    B ext(group.size());
    ext.extend_lsb(group);
    a = a | (ext & b);
}

/**
 * @brief Count aggregation (delegates to sum).
 *
 * @tparam A Arithmetic shared vector type.
 *
 * @param group Grouping bits indicating which elements to count.
 * @param a Accumulator vector (modified in place).
 * @param b Input vector (typically all ones for counting).
 */
template <typename A>
void count(const A& group, A& a, const A& b) {
    sum(group, a, b);
}

/**
 * @brief Internal helper for min/max aggregation.
 *
 * @tparam B Boolean shared vector type.
 *
 * @param group Grouping bits indicating which elements to aggregate.
 * @param a Accumulator vector (modified in place).
 * @param b Input vector to be aggregated.
 * @param minimum If true, computes minimum; if false, computes maximum.
 */
template <typename B>
void _min_max_agg(const B& group, B& a, const B& b, const bool& minimum = false) {
    B b_greater = b > a;

    if (minimum) {
        // well, now it's technically a_less_than_or_equal
        b_greater = !b_greater;
    }

    a = multiplex(group, a, multiplex(b_greater, a, b));
}

/**
 * @brief Maximum aggregation.
 *
 * @tparam B Boolean shared vector type.
 *
 * @param group Grouping bits indicating which elements to aggregate.
 * @param a Accumulator vector (modified in place).
 * @param b Input vector to be aggregated.
 */
template <typename B>
void max(const B& group, B& a, const B& b) {
    _min_max_agg(group, a, b, false);
}

/**
 * @brief Minimum aggregation.
 *
 * @tparam B Boolean shared vector type.
 *
 * @param group Grouping bits indicating which elements to aggregate.
 * @param a Accumulator vector (modified in place).
 * @param b Input vector to be aggregated.
 */
template <typename B>
void min(const B& group, B& a, const B& b) {
    _min_max_agg(group, a, b, true);
}

/**
 * @brief Identity "aggregation", used for non-aggregated joins.
 * Templated to accept either arithmetic or boolean types. Copies rows from
 * left to the right.
 *
 * @param group grouping bits.
 * @param a first vector, which will be updated.
 * @param b second vector.
 */
template <typename T>
void copy(const T& group, T& a, const T& b) {
    a = multiplex(group, a, b);
}

/**
 * @brief validity aggregation. For internal use while updating valid
 * column.
 *
 * @tparam B Boolean shared vector type.
 *
 * @param group Grouping bits.
 * @param a Accumulator vector (modified in place).
 * @param b Input vector.
 */
template <typename B>
void valid(const B& group, B& a, const B& b) {
    B agg = a & b;
    a = multiplex(group, a, agg);
}

template <typename Share, typename EVector>
using A_ = ASharedVector<Share, EVector>;

template <typename Share, typename EVector>
using B_ = BSharedVector<Share, EVector>;

/**
 * @brief Sorting-network based agregation. Assumes all vectors are the same
 * size.
 *
 * @tparam S underlying data type of vectors
 * @tparam E Share container type.
 *
 * @param keys vector of keys to join and aggregate on
 * @param agg_spec_b boolean aggregations
 * @param agg_spec_a arithmetic aggregations
 * @param dir which direction to run the aggregation
 * @param sel_b selection column (for table operations, usually table ID)
 */
template <typename S, typename E>
void aggregate(
    std::vector<B_<S, E>>& keys,
    const std::vector<
        std::tuple<B_<S, E>, B_<S, E>, void (*)(const B_<S, E>&, B_<S, E>&, const B_<S, E>&)>>&
        agg_spec_b,
    const std::vector<
        std::tuple<A_<S, E>, A_<S, E>, void (*)(const A_<S, E>&, A_<S, E>&, const A_<S, E>&)>>&
        agg_spec_a,
    const enum Direction dir = Direction::Forward, std::optional<B_<S, E>> sel_b = {}) {
    // figure out size of the aggregation
    size_t total_size;
    if (keys.size() > 0) {
        total_size = keys[0].size();
    } else if (agg_spec_b.size() > 0) {
        total_size = std::get<0>(agg_spec_b[0]).size();
    } else if (agg_spec_a.size() > 0) {
        total_size = std::get<0>(agg_spec_a[0]).size();
    } else {
        throw std::runtime_error("Empty aggregation!");
    }

    bool have_b_aggs = (agg_spec_b.size() > 0);
    bool have_a_aggs = (agg_spec_a.size() > 0);

    bool a_any_copy = false;
    bool a_any_noncopy = false;
    bool b_any_copy = false;
    bool b_any_noncopy = false;

    ASSERT_POWER_OF_TWO(total_size);

    // TODO: check that in multiplex and just avoid the step
    //  - Normally, multiplexing using secret shares uses a secure and.
    //  - However, if we are multiplexing using public one, we can use plaintext.

    /* Preprocessing step: check if we need secret shared one; transform
     * count into sum.
     */

    Vector<S> one(1, 1);
    B_<S, E> shared_one_b = orq::service::runTime->public_share<E::replicationNumber>(one);
    A_<S, E> shared_one_a = orq::service::runTime->public_share<E::replicationNumber>(one);

    for (auto s : agg_spec_a) {
        auto [in, out, func] = s;

        // If this is a count aggregation...
        if (func == &count<A_<S, E>>) {
            if (sel_b.has_value()) {
                // use sel_b (table id) within joins: don't count rows on
                // left
                // TODO: technically only need if no A copies, but this is
                // only a single invocation per call
                out = sel_b->b2a_bit();
            } else {
                // ...use vector of all 1s as input
                out = shared_one_a.repeated_subset_reference(total_size);
            }
        } else /*if (in.vector != out.vector)*/
        {
            // ...otherwise, copy input to output
            // TODO: only do this if vectors different
            out = in;
        }

        if (func == &copy<A_<S, E>>) {
            a_any_copy = true;
        } else {
            a_any_noncopy = true;
        }
    }

    for (auto s : agg_spec_b) {
        auto [in, out, func] = s;
        out = in;

        if (func == &copy<B_<S, E>>) {
            b_any_copy = true;
        } else {
            b_any_noncopy = true;
        }
    }

    auto aggNet = BrentKung<S, E>(dir, total_size);

    for (auto [sz, level_func] : aggNet) {
        B_<S, E> group_bits_b(sz);
        B_<S, E> join_group_bits_b(sz);

        A_<S, E> group_bits_a(group_bits_b.size());
        A_<S, E> join_group_bits_a(group_bits_b.size());

        if (keys.size() == 0) {
            group_bits_b = shared_one_b.repeated_subset_reference(sz);
            group_bits_a = shared_one_a.repeated_subset_reference(sz);
        } else {
            auto [first_vector, second_vector] = level_func(keys[0]);
            group_bits_b = first_vector == second_vector;

            // for remaining columns
            for (int j = 1; j < keys.size(); ++j) {
                auto [first_vector, second_vector] = level_func(keys[j]);
                group_bits_b &= first_vector == second_vector;
            }
        }

        join_group_bits_b = group_bits_b;
        join_group_bits_a = group_bits_a;
        if (sel_b.has_value() && (a_any_noncopy || b_any_noncopy)) {
            auto [sa, sb] = level_func(*sel_b);
            group_bits_b &= ~(sa ^ sb);
        }

        group_bits_b.mask(1);

        // If we're using a `copy` aggregation, use the `join_group_bits`. Otherwise, use the
        // regular `group_bits`. Use type deduction so we only have to declare this lambda once.
        auto which_group_bits = [&](const auto& func, const auto& group_bits,
                                    const auto& join_group_bits) {
            using FuncType = std::decay_t<decltype(func)>;
            if (func == static_cast<FuncType>(&copy<std::decay_t<decltype(group_bits)>>)) {
                return join_group_bits;
            } else {
                return group_bits;
            }
        };

        // Iterate through the aggregations: boolean...
        for (auto s : agg_spec_b) {
            auto [_in, out, func] = s;

            auto [a, b] = level_func(out);
            auto g = which_group_bits(func, group_bits_b, join_group_bits_b);

            func(g, b, a);
        }

        // (only perform b2a if needed)
        if (keys.size() > 0 && have_a_aggs) {
            if (a_any_noncopy) {
                group_bits_a = group_bits_b.b2a_bit();
            }
            if (a_any_copy) {
                join_group_bits_a = join_group_bits_b.b2a_bit();
            }
        }

        // ...and arithmetic
        for (auto s : agg_spec_a) {
            auto [_in, out, func] = s;

            auto [a, b] = level_func(out);
            auto g = which_group_bits(func, group_bits_a, join_group_bits_a);

            func(g, b, a);
        }
    }  // end odd-even aggregation loop
}

/**
 * @brief Aggregation, with additional selection (window) argument.
 * Performs reverse aggregation.
 *
 * @tparam S
 * @tparam E
 * @param keys
 * @param agg_spec_b
 * @param agg_spec_a
 * @param sel_b
 */
template <typename S, typename E>
void aggregate(
    std::vector<B_<S, E>>& keys,
    const std::vector<
        std::tuple<B_<S, E>, B_<S, E>, void (*)(const B_<S, E>&, B_<S, E>&, const B_<S, E>&)>>&
        agg_spec_b,
    const std::vector<
        std::tuple<A_<S, E>, A_<S, E>, void (*)(const A_<S, E>&, A_<S, E>&, const A_<S, E>&)>>&
        agg_spec_a,
    B_<S, E> sel_b) {
    aggregate(keys, agg_spec_b, agg_spec_a, Direction::Reverse, std::make_optional(sel_b));
}

/**
 * @brief Compute a prefix sum over a vector using a log-depth aggregation
 * tree. Based on sorting network aggregation but entirely local
 * computation, with the simplification that all entries belong to the same
 * group.
 *
 * TODO: extend this to any user-supplied associative operation
 *
 * @tparam T vector type (shared or plaintext)
 * @param v input vector
 * @param reverse whether to compute a suffix sum instead
 */
template <typename T>
void tree_prefix_sum(T v, const bool& reverse = false) {
    size_t size = v.size();
    ASSERT_POWER_OF_TWO(size);

    auto y = reverse ? v.directed_subset_reference(-1) : v;

    const int log_size = std::bit_width(size) - 1;

    for (int i = 1; i <= log_size; i++) {
        int d = size / (1 << i);

        int d_rest = size - d;

        T a(y.slice(0, d_rest));
        T b(y.slice(d));

        // This cannot be compound assignment +=, because it operates on
        // itself.
        b = b + a;
    }
}
}  // namespace orq::aggregators
