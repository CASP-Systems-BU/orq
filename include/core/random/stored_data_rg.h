#ifndef GAVEL_STORED_DATA_RG_H
#define GAVEL_STORED_DATA_RG_H

#include "random_generator.h"

namespace secrecy {

    template<typename Share, typename ShareVector>
    class StoredDataRG : public RandomGenerator {
        std::vector<ShareVector> shares;
        std::vector<int> start_indices;

        template<typename... T>
        void addShares(ShareVector _shares_1, T... _shares_2) {
            addShares(_shares_1);
            addShares(_shares_2...);
        }

        void addShares(ShareVector _shares_1) {
            shares.push_back(_shares_1);
            start_indices.push_back(0);
        }

    public:

        StoredDataRG() : RandomGenerator(-1) {}

        template<typename... T>
        StoredDataRG(T... _shares) : RandomGenerator(-1) {
            addShares(_shares...);
        }

        Share getNext(RGChannelID id) {
            auto res = shares[id][start_indices[id]];
            start_indices[id]++;
            return res;
        }

        Share getRandom() {
            return 0;
        }

        ShareVector getMultipleNext(RGChannelID id, RGSize size) {
            auto res = shares[id].simple_subset(start_indices[id], size);
            start_indices[id] += size;
            return res;
        }

        ShareVector getMultipleRandom(RGSize size) {
            return ShareVector(size);
        }
    };
} // namespace gavel

#endif //GAVEL_STORED_DATA_RG_H
