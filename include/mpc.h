#ifndef GAVEL_MPC_H
#define GAVEL_MPC_H

// Core - Containers
#include "core/containers/e_vector.h"
#include "core/containers/vector.h"
#include "core/protocols/protocol.h"
#include "service/common/runtime.h"

// Core - Communication
#include "core/communication/communicator.h"
#include "core/communication/communicator_factory.h"
#include "core/communication/mpi_communicator.h"
#include "core/communication/no_copy_communicator/no_copy_communicator.h"
#include "core/communication/no_copy_communicator/no_copy_communicator_factory.h"
#include "core/communication/null_communicator.h"

// Core - Random
#include "core/random/manager.h"
#include "core/random/permutations/hm_sharded_permutation_generator.h"
#include "core/random/permutations/zero_permutation_generator.h"
#include "core/random/zero_rg.h"
#include "core/random/dummy_auth_triple_generator.h"
#include "core/random/dummy_auth_random_generator.h"

#ifdef MPC_PROTOCOL_BEAVER_TWO
#include "core/random/beaver_triple_generator.h"
#include "core/random/dummy_ole.h"
#include "core/random/zero_ole.h"
#include "core/random/silent_ole.h"
#include "core/random/silent_ot.h"
#ifdef USE_LIBOTE
#include "core/random/oprf.h"
#endif
#endif

// #include "core/random/stored_data_rg.h"
#include "core/random/common_prg.h"
#include "core/random/pooled/pooled_generator.h"

#include "core/protocols/protocol_factory.h"

// Core - Protocols
#include "core/protocols/dummy_0pc.h"
#include "core/protocols/plaintext_1pc.h"
#include "core/protocols/replicated_3pc.h"

#ifdef USE_DALSKOV_FANTASTIC_FOUR
#include "core/protocols/dalskov_4pc.h"
#else
#include "core/protocols/custom_4pc.h"
#endif

#ifdef MPC_PROTOCOL_BEAVER_TWO
#include "core/protocols/beaver_2pc.h"
#endif

// Core - Shares
#include "core/containers/a_shared_vector.h"
#include "core/containers/b_shared_vector.h"
#include "core/containers/encoded_vector.h"
#include "core/containers/permutation.h"
#include "core/containers/shared_vector.h"
#include "core/operators/operators.h"

// Relational
#include "relational/database/encoded_table.h"
#include "relational/database/shared_column.h"

// Macros Section
#define init_mpc_types(_Element_, _Vector_, _ReplicatedShare_, _EVector_, _Replication_)          \
    template <typename T>                                                                         \
    using ReplicatedShare = _ReplicatedShare_<T>;                                                 \
                                                                                                  \
    template <typename T>                                                                         \
    using Vector = _Vector_<T>;                                                                   \
                                                                                                  \
    template <typename T>                                                                         \
    using DataTable = std::vector<_Vector_<T>>;                                                   \
                                                                                                  \
    template <typename T>                                                                         \
    using EVector = _EVector_<T, _Replication_>;                                                  \
                                                                                                  \
    template <typename T>                                                                         \
    using ASharedVector = secrecy::ASharedVector<T, _EVector_<T, _Replication_>>;                 \
                                                                                                  \
    template <typename T>                                                                         \
    using BSharedVector = secrecy::BSharedVector<T, _EVector_<T, _Replication_>>;                 \
                                                                                                  \
    using EncodedColumn = relational::EncodedColumn;                                              \
                                                                                                  \
    template <typename T>                                                                         \
    using SharedColumn = relational::SharedColumn<T, _EVector_<T, _Replication_>>;                \
                                                                                                  \
    template <typename T>                                                                         \
    using EncodedTable =                                                                          \
        secrecy::relational::EncodedTable<T, SharedColumn<T>, ASharedVector<T>, BSharedVector<T>, \
                                          secrecy::EncodedVector, DataTable<T>>;

#define init_mpc_system(_Communicator_, _RG_, _Protocol_, _ProtocolFactory_)                 \
    typedef _Communicator_ Communicator;                                                     \
    typedef _RG_ RG;                                                                         \
    typedef _Protocol_<int8_t, ReplicatedShare<int8_t>, Vector<int8_t>, EVector<int8_t>>     \
        Protocol_8;                                                                          \
    typedef _Protocol_<int16_t, ReplicatedShare<int16_t>, Vector<int16_t>, EVector<int16_t>> \
        Protocol_16;                                                                         \
    typedef _Protocol_<int32_t, ReplicatedShare<int32_t>, Vector<int32_t>, EVector<int32_t>> \
        Protocol_32;                                                                         \
    typedef _Protocol_<int64_t, ReplicatedShare<int64_t>, Vector<int64_t>, EVector<int64_t>> \
        Protocol_64;                                                                         \
    typedef _Protocol_<__int128_t, ReplicatedShare<__int128_t>, Vector<__int128_t>,          \
                       EVector<__int128_t>>                                                  \
        Protocol_128;                                                                        \
    typedef _ProtocolFactory_<ReplicatedShare, Vector, EVector> ProtocolFactory;

#define init_mpc_functions(_Replication_)                                                          \
    template <typename T, typename... T2>                                                          \
    static EVector<T> secret_share_a(const secrecy::Vector<T>& data, const T2&... args) {          \
        return runTime->secret_share_a<_Replication_>(data, args...);                              \
    }                                                                                              \
                                                                                                   \
    template <typename T, typename... T2>                                                          \
    static EVector<T> secret_share_b(const secrecy::Vector<T>& data, const T2&... args) {          \
        return runTime->secret_share_b<_Replication_>(data, args...);                              \
    }                                                                                              \
                                                                                                   \
    template <typename T>                                                                          \
    static std::vector<std::shared_ptr<EncodedColumn>> secret_share(                               \
        const DataTable<T>& data_table, const std::vector<std::string>& schema,                    \
        const int& _party_id = 0) {                                                                \
        return EncodedTable<T>::template secret_share<T, _Replication_>(data_table, schema,        \
                                                                        _party_id);                \
    }                                                                                              \
                                                                                                   \
    template <typename T>                                                                          \
    static EVector<T> public_share(const secrecy::Vector<T>& data) {                               \
        return runTime->public_share<_Replication_>(data);                                         \
    }                                                                                              \
                                                                                                   \
    template <typename T>                                                                          \
    static BSharedVector<T> compare_rows(const std::vector<BSharedVector<T>*>& x_vec,              \
                                         const std::vector<BSharedVector<T>*>& y_vec,              \
                                         const std::vector<bool>& inverse) {                       \
        return secrecy::compare_rows(x_vec, y_vec, inverse);                                       \
    }                                                                                              \
                                                                                                   \
    template <typename T>                                                                          \
    static void swap(std::vector<BSharedVector<T>*>& x_vec, std::vector<BSharedVector<T>*>& y_vec, \
                     const BSharedVector<T>& bits) {                                               \
        secrecy::swap(x_vec, y_vec, bits);                                                         \
    }                                                                                              \
                                                                                                   \
    template <typename T>                                                                          \
    static Vector<T> compare_rows(const std::vector<Vector<T>*>& x_vec,                            \
                                  const std::vector<Vector<T>*>& y_vec,                            \
                                  const std::vector<bool>& inverse) {                              \
        return secrecy::compare_rows(x_vec, y_vec, inverse);                                       \
    }                                                                                              \
                                                                                                   \
    template <typename T>                                                                          \
    static void swap(std::vector<Vector<T>*>& x_vec, std::vector<Vector<T>*>& y_vec,               \
                     const std::vector<bool>& bits) {                                              \
        secrecy::swap(x_vec, y_vec, bits);                                                         \
    }                                                                                              \
                                                                                                   \
    template <typename T, typename V>                                                              \
    static void bitonic_sort(std::vector<BSharedVector<T>*> _columns,                              \
                             const std::vector<bool>& desc) {                                      \
        operators::bitonic_sort(_columns, desc);                                                   \
    }

#endif  // GAVEL_MPC_H
