#ifndef SECRECY_POOLED_GENERATOR_H
#define SECRECY_POOLED_GENERATOR_H

#include "../correlation_generator.h"
#include "../../../benchmark/stopwatch.h"

using namespace secrecy::benchmarking;

#include <deque>

namespace secrecy::random {

    template <typename Generator, typename... Ts>
    // compile-time check that the template parameter is a derived class of CorrelationGenerator
    requires std::derived_from<Generator, CorrelationGenerator>
    class PooledGenerator : public CorrelationGenerator {

        std::shared_ptr<Generator> generator;
        
        // the main pool of generated randomness
        std::tuple<std::deque<Ts>...> queueTuple;
        
        /**
         * addToQueueImpl - private implementation method for adding a generated batch to the object's queue
         * batch - The generated batch of randomness
         */
        template<std::size_t... I>
        void addToQueueImpl(std::tuple<Vector<Ts>...>& batch, std::index_sequence<I...>) {
            // use fold expression to iterate and add each vector to its corresponding queue
            ((
                std::get<I>(queueTuple).insert(std::get<I>(queueTuple).end(), std::get<I>(batch).begin(), std::get<I>(batch).end()),
                void()
            ), ...);
        }

        /**
         * addToQueueImpl - private implementation method for adding a generated batch to the object's queue
         * batch - The generated batch of randomness
         */
        template<std::size_t... I>
        void addToQueueImpl(std::tuple<std::vector<Ts>...>& batch, std::index_sequence<I...>) {
            // use fold expression to iterate and add each vector to its corresponding queue
            ((
                std::get<I>(queueTuple).insert(std::get<I>(queueTuple).end(), std::get<I>(batch).begin(), std::get<I>(batch).end()),
                void()
            ), ...);
        }

        /**
         * addToQueue - private method to allow reserve() to add randomness to the pool
         * batch - The generated batch of randomness
         */
        void addToQueue(std::tuple<Vector<Ts>...>& batch) {
            addToQueueImpl(batch, std::make_index_sequence<sizeof...(Ts)>{});
        }
        
        /**
         * addToQueue - private method to allow reserve() to add randomness to the pool
         * batch - The generated batch of randomness
         */
        void addToQueue(std::tuple<std::vector<Ts>...>& batch) {
            addToQueueImpl(batch, std::make_index_sequence<sizeof...(Ts)>{});
        }

        /**
         * extractElements - private helper function to get elements from a specific deque in the tuple of deques
         * count - The number of elements to extract
         */
        template<std::size_t I, template<typename...> class VectorType = Vector>
        VectorType<typename std::tuple_element<I, std::tuple<Ts...>>::type> extractElements(std::size_t count) {
            using ElementType = typename std::tuple_element<I, std::tuple<Ts...>>::type;
            std::vector<ElementType> result(count);

            auto& deque = std::get<I>(queueTuple);
            
            // insert the range of elements from deque to result vector
            std::copy(deque.begin(), deque.begin() + count, result.begin());
            
            // remove the extracted elements from the deque
            deque.erase(deque.begin(), deque.begin() + count);

            if constexpr (std::is_same_v<VectorType<ElementType>, Vector<ElementType>>) {
                // convert to secrecy::Vector
                return Vector(result);
            } else {
                // return as std::vector
                return result;
            }
        }

        /**
         * getNextImpl - private implementation function to get the next <count> elements from the pool
         * count - The number of elements to get
         */
        template<std::size_t... I, template<typename...> class VectorType = Vector>
        std::tuple<VectorType<Ts>...> getNextImpl(std::size_t count, std::index_sequence<I...>) {
            // create a tuple of vectors for the result
            return std::make_tuple(
                extractElements<I, VectorType>(count)... // extract elements from each deque
            );
        }
        
        /**
         * queueSizeImpl - private implementation function to get the size of the pool
         * count - The number of elements to get
         */
        template<std::size_t... I>
        std::size_t sizeImpl(std::index_sequence<I...>) const {
            std::size_t sizes[] = {std::get<I>(queueTuple).size()...};
            std::size_t first_size = sizes[0];

            // check that all sizes are the same
            for (std::size_t size : sizes) {
                if (size != first_size) {
                    throw std::runtime_error("Queue sizes do not match");
                }
            }

            return first_size;
        }

    public:

        /**
         * PooledGenerator constructor
         * pass a default seed to the RandomGenerator since we don't need to generate a real seed
         */
        PooledGenerator(std::shared_ptr<Generator> _generator)
            : CorrelationGenerator(_generator->getRank()), generator(_generator) {}

        void reserve(size_t count) {
            auto batch = generator->getNext(count);
            addToQueue(batch);
        }

        /**
         * queueSize - get the size of the pool of randomness
         */
        std::size_t size() const {
            // use a helper function with index sequence internally
            return sizeImpl(std::make_index_sequence<sizeof...(Ts)>{});
        }

        /**
         * assertCorrelated - check that the batch is correlated with the generator
         * batch - The batch to check
         */
        template <typename T>
        void assertCorrelated(T& batch) {
            generator->assertCorrelated(batch);
        }

        /**
         * getNext - get elements from the pool
         * count - The number of elements to get
         */
        auto getNext(size_t count) {
            size_t currentSize = size();

            // if not enought elements, generate more
            if (currentSize < count) {
                return generator->getNext(count);
            }
            
            // call a helper function to actually get the elements
            return getNextImpl(count, std::make_index_sequence<sizeof...(Ts)>{});
        }
    };

    /**
     * @brief Internal function to actually create a pooled object.
     * 
     * @tparam Tuple the inferred tuple type of the correlation's output
     * @tparam Generator the generator itself
     * @tparam Is an index sequence corresponding to the number of tuple elements
     * in the correlation
     * @param generator the actual generation object which should be pooled.
     * Pass in a shared pointer.
     * @return requires 
     */
    template <typename Tuple, typename Generator, std::size_t... Is>
    auto _make_pooled_impl(std::shared_ptr<Generator> generator, std::index_sequence<Is...>)
    {
        // use variadic expansion of template args
        // calling with tuple type <Vector<T>, Vector<Q>, Vector<R>> will
        // instantiate pooled generator with <Generator, T, Q, R>
        //
        // basically implementing "for each element in the tuple, get the
        // value_type of the underlying vector", but at compile time
        return std::make_shared<
            PooledGenerator<
                Generator,
                typename std::tuple_element<
                    Is, std::decay_t<Tuple>>::type::value_type...>>(generator);
    }

    /**
     * make_pooled - Create a new PooledGenerator object.
     * @param args - The arguments to pass to the Generator constructor.
     * @return A shared pointer to the new PooledGenerator object.
     */
    template <typename Generator, typename... Args>
        requires std::derived_from<Generator, CorrelationGenerator>
    auto make_pooled(Args &&...args)
    {
        // create a Generator
        auto generator = std::make_shared<Generator>(std::forward<Args>(args)...);

        // infer the type of the correlation from the generator
        using correlation_t = decltype(generator->getNext(std::declval<size_t>()));

        // correlation consists of tuples of this size
        constexpr size_t tuple_size = std::tuple_size<std::decay_t<correlation_t>>::value;

        // template hack to create the pooled generator
        return _make_pooled_impl<correlation_t>(generator, std::make_index_sequence<tuple_size>{});
    }

} // namespace secrecy::random
#endif