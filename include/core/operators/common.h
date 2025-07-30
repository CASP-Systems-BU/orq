#ifndef SECRECY_OPERATORS_COMMON_H
#define SECRECY_OPERATORS_COMMON_H

#include "../containers/a_shared_vector.h"
#include "../containers/b_shared_vector.h"

namespace secrecy::operators {

template <typename Share, typename EVector>
static BSharedVector<Share, EVector> multiplex(const BSharedVector<Share, EVector>& sel,
                                               const BSharedVector<Share, EVector>& a,
                                               const BSharedVector<Share, EVector>& b) {
    BSharedVector<Share, EVector> sel_extended(sel.size());
    sel_extended.extend_lsb(sel);
    BSharedVector<Share, EVector> res = a ^ (sel_extended & (b ^ a));
    return res;
}

template <typename Share, typename EVector>
static ASharedVector<Share, EVector> multiplex(const ASharedVector<Share, EVector>& sel,
                                               const ASharedVector<Share, EVector>& a,
                                               const ASharedVector<Share, EVector>& b) {
    ASharedVector<Share, EVector> res = a + sel * (b - a);
    return res;
}

template <typename Share, typename EVector>
static void auto_conversion(BSharedVector<Share, EVector>& x, BSharedVector<Share, EVector>& res) {
    res = x;
}

template <typename Share, typename EVector>
static void auto_conversion(ASharedVector<Share, EVector>& x, ASharedVector<Share, EVector>& res) {
    res = x;
}

// TODO: differentiate between "b2a_bit" and just "b2a"
template <typename Share, typename EVector>
static void auto_conversion(BSharedVector<Share, EVector>& x, ASharedVector<Share, EVector>& res) {
    res = x.b2a_bit();
}

template <typename Share, typename EVector>
static void auto_conversion(ASharedVector<Share, EVector>& x, BSharedVector<Share, EVector>& res) {
    res = x.a2b();
}

template <typename Share, typename EVector>
static void secret_share_vec(const Vector<Share>& data, BSharedVector<Share, EVector>& out,
                             const PartyID& data_party = 0) {
    out = secrecy::service::runTime->secret_share_b<EVector::replicationNumber>(data, data_party);
}

template <typename Share, typename EVector>
static void secret_share_vec(const Vector<Share>& data, ASharedVector<Share, EVector>& out,
                             const PartyID& data_party = 0) {
    out = secrecy::service::runTime->secret_share_a<EVector::replicationNumber>(data, data_party);
}
}  // namespace secrecy::operators

#endif  // SECRECY_OPERATORS_COMMON_H
