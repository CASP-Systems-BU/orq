#ifndef SECRECY_OPERATORS_RELATIONAL_H
#define SECRECY_OPERATORS_RELATIONAL_H

#include "../containers/a_shared_vector.h"
#include "../containers/b_shared_vector.h"

#include "common.h"

namespace secrecy
{
    namespace operators
    {
        /**
         * @brief obliviously mark distinct rows, given a list of keys. note that
         * only adjacent rows are considered, so for global uniqueness vectors
         * should be sorted before calling `distinct`.
         *
         * @tparam Share
         * @tparam EVector
         * @param keys a list of keys to consider for uniqueness.
         * @param res a vector to place the result in
         */
        template <typename Share, typename EVector>
        static void distinct(std::vector<BSharedVector<Share, EVector> *> &keys,
                             BSharedVector<Share, EVector> *res)
        {
            assert(keys.size() > 0);
            // clear out the result vector
            res->zero();

            // always mark the first index unique
            BSharedVector<Share, EVector> first_index = res->slice(0, 1);
            Vector<Share> one(1, 1);
            // enforce single-bit boolean share (all higher bits are 0)
            first_index = secrecy::service::runTime->public_share<EVector::replicationNumber>(one);

            BSharedVector<Share, EVector> rest = res->slice(1);

            for (int i = 0; i < keys.size(); ++i)
            {
                // v[0..n-1]
                BSharedVector<Share, EVector> a = keys[i]->slice(0, keys[i]->size() - 1);
                // v[1..n]
                BSharedVector<Share, EVector> b = keys[i]->slice(1);

                rest |= a != b;
            }

            // no return; `rest` is a reference into the result vector so
            // automatically updates.
        }

        template <typename Share, typename EVector>
        static void distinct(std::vector<BSharedVector<Share, EVector>> &keys,
                             BSharedVector<Share, EVector> &res)
        {
            std::vector<BSharedVector<Share, EVector> *> keys_ptr;

            for (int i = 0; i < keys.size(); ++i)
            {
                keys_ptr.push_back(&keys[i]);
            }

            distinct(keys_ptr, &res);
        }
    }
}

#endif // SECRECY_OPERATORS_RELATIONAL_H