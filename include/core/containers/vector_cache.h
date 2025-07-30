#ifndef SECRECY_VECTOR_CACHE_H
#define SECRECY_VECTOR_CACHE_H

#include <vector>
#include <queue>
#include <map>
#include <memory>

namespace secrecy{
    namespace {

        template<typename T>
        class VectorCache {
            std::map<int, std::queue<std::vector<T> *> > cache;

        public:
            VectorCache() {}

            ~VectorCache() {
                // iterate over cache which is a map.
                // The value inside cache is a queue of vector points.
                // Free the vector pointers.
                for (auto &kv : cache) {
                    while (!kv.second.empty()) {
                        delete kv.second.front();
                        kv.second.pop();
                    }
                }
            }

            std::vector<T> *AllocateVector(const int &size) {
                if (cache[size].size() > 0) {
                    auto v = cache[size].front();
                    cache[size].pop();
                    return v;
                } else {
                    return new std::vector<T>(size);
                }
            }

            void deallocateVector(std::vector<T> *vecPtr) {
                cache[vecPtr->size()].push(vecPtr);
            }
        };

    }
}

#endif //SECRECY_VECTOR_CACHE_H
