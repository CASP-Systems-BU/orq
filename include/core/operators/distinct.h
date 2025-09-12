#pragma once

#include "common.h"
#include "core/containers/a_shared_vector.h"
#include "core/containers/b_shared_vector.h"

namespace orq::operators {
/**
 * @brief Obliviously marks distinct rows based on keys (adjacent rows only).
 *
 * Only adjacent rows are considered, so vectors should be sorted
 * before calling for global uniqueness.
 *
 * @tparam Share The underlying data type of the shared vectors.
 * @tparam EVector Share container type.
 * @param keys List of keys to consider for uniqueness.
 * @param res Vector to place the result in.
 */
template <typename Share, typename EVector>
static void distinct(std::vector<BSharedVector<Share, EVector> *> &keys,
                     BSharedVector<Share, EVector> *res) {
    assert(keys.size() > 0);
    // clear out the result vector
    res->zero();

    // always mark the first index unique
    BSharedVector<Share, EVector> first_index = res->slice(0, 1);
    Vector<Share> one(1, 1);
    // enforce single-bit boolean share (all higher bits are 0)
    first_index = orq::service::runTime->public_share<EVector::replicationNumber>(one);

    BSharedVector<Share, EVector> rest = res->slice(1);

    for (int i = 0; i < keys.size(); ++i) {
        // v[0..n-1]
        BSharedVector<Share, EVector> a = keys[i]->slice(0, keys[i]->size() - 1);
        // v[1..n]
        BSharedVector<Share, EVector> b = keys[i]->slice(1);

        rest |= a != b;
    }

    // no return; `rest` is a reference into the result vector so
    // automatically updates.
}

/**
 * @brief Marks distinct rows based on keys (vector reference overload).
 *
 * @tparam Share The underlying data type of the shared vectors.
 * @tparam EVector Share container type.
 * @param keys Vector of keys to consider for uniqueness.
 * @param res Vector to place the result in.
 */
template <typename Share, typename EVector>
static void distinct(std::vector<BSharedVector<Share, EVector>> &keys,
                     BSharedVector<Share, EVector> &res) {
    std::vector<BSharedVector<Share, EVector> *> keys_ptr;

    for (int i = 0; i < keys.size(); ++i) {
        keys_ptr.push_back(&keys[i]);
    }

    distinct(keys_ptr, &res);
}
}  // namespace orq::operators
