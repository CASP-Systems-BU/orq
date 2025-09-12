#pragma once

#include "core/containers/a_shared_vector.h"
#include "core/containers/b_shared_vector.h"

namespace orq::operators {

/**
 * @brief Conditional selection between two boolean shared vectors.
 *
 * Performs oblivious selection: returns a if sel is false, b if sel is true.
 *
 * @tparam Share The underlying data type of the shared vectors.
 *
 * @param sel Boolean selector vector (extended to full bit width).
 * @param a First input vector (returned when sel is false).
 * @param b Second input vector (returned when sel is true).
 * @return The multiplexed result vector.
 */
template <typename Share, typename EVector>
static BSharedVector<Share, EVector> multiplex(const BSharedVector<Share, EVector>& sel,
                                               const BSharedVector<Share, EVector>& a,
                                               const BSharedVector<Share, EVector>& b) {
    BSharedVector<Share, EVector> sel_extended(sel.size());
    sel_extended.extend_lsb(sel);
    BSharedVector<Share, EVector> res = a ^ (sel_extended & (b ^ a));
    return res;
}

/**
 * @brief Conditional selection between two arithmetic shared vectors.
 *
 * Performs oblivious selection: returns a if sel is 0, b if sel is 1.
 * Uses linear combination for arithmetic shares.
 *
 * @tparam Share The underlying data type of the shared vectors.
 *
 * @param sel Arithmetic selector vector.
 * @param a First input vector (returned when sel is 0).
 * @param b Second input vector (returned when sel is 1).
 * @return The multiplexed result vector.
 */
template <typename Share, typename EVector>
static ASharedVector<Share, EVector> multiplex(const ASharedVector<Share, EVector>& sel,
                                               const ASharedVector<Share, EVector>& a,
                                               const ASharedVector<Share, EVector>& b) {
    ASharedVector<Share, EVector> res = a + sel * (b - a);
    return res;
}

/**
 * @brief Identity conversion for boolean shared vectors.
 * @tparam Share The underlying data type of the shared vectors.
 * @param x Input boolean shared vector.
 * @param res Output boolean shared vector (copy of input).
 */
template <typename Share, typename EVector>
static void auto_conversion(BSharedVector<Share, EVector>& x, BSharedVector<Share, EVector>& res) {
    res = x;
}

/**
 * @brief Identity conversion for arithmetic shared vectors.
 * @tparam Share The underlying data type of the shared vectors.
 * @param x Input arithmetic shared vector.
 * @param res Output arithmetic shared vector (copy of input).
 */
template <typename Share, typename EVector>
static void auto_conversion(ASharedVector<Share, EVector>& x, ASharedVector<Share, EVector>& res) {
    res = x;
}

/**
 * @brief Converts boolean shared vector to arithmetic shared vector.
 * @tparam Share The underlying data type of the shared vectors.
 * @param x Input boolean shared vector.
 * @param res Output arithmetic shared vector.
 */
// TODO: differentiate between "b2a_bit" and just "b2a"
template <typename Share, typename EVector>
static void auto_conversion(BSharedVector<Share, EVector>& x, ASharedVector<Share, EVector>& res) {
    res = x.b2a_bit();
}

/**
 * @brief Converts arithmetic shared vector to boolean shared vector.
 * @tparam Share The underlying data type of the shared vectors.
 * @param x Input arithmetic shared vector.
 * @param res Output boolean shared vector.
 */
template <typename Share, typename EVector>
static void auto_conversion(ASharedVector<Share, EVector>& x, BSharedVector<Share, EVector>& res) {
    res = x.a2b();
}

/**
 * @brief Creates boolean shared vector from plaintext data.
 * @tparam Share The underlying data type of the shared vectors.
 * @param data Plaintext vector to be secret-shared.
 * @param out Output boolean shared vector.
 * @param data_party The party that provides the input data.
 */
template <typename Share, typename EVector>
static void secret_share_vec(const Vector<Share>& data, BSharedVector<Share, EVector>& out,
                             const PartyID data_party = 0) {
    out = orq::service::runTime->secret_share_b<EVector::replicationNumber>(data, data_party);
}

/**
 * @brief Creates arithmetic shared vector from plaintext data.
 * @tparam Share The underlying data type of the shared vectors.
 * @param data Plaintext vector to be secret-shared.
 * @param out Output arithmetic shared vector.
 * @param data_party The party that provides the input data.
 */
template <typename Share, typename EVector>
static void secret_share_vec(const Vector<Share>& data, ASharedVector<Share, EVector>& out,
                             const PartyID data_party = 0) {
    out = orq::service::runTime->secret_share_a<EVector::replicationNumber>(data, data_party);
}
}  // namespace orq::operators
