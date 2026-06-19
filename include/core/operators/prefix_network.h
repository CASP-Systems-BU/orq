#pragma once

#include "../containers/a_shared_vector.h"
#include "../containers/b_shared_vector.h"
#include "../containers/shared_vector.h"
#include "common.h"

namespace orq::aggregators {

/**
 * @brief Parent class for Prefix Networks. Automatically handles type conversions for
 * ASharedVector / BSharedVector.
 *
 * Subclasses must implement three functions:
 * - `_level(i, input)`, which produces a view of `input` for level $i$;
 * - `level_size(i)`, which returns the size of the view at level $i$; and
 * - `depth()`, which returns the depth of the current network.
 *
 * @tparam T
 * @tparam E
 */
template <typename T, typename E>
class PrefixNetwork {
   private:
    using _S_t = SharedVector<T, E>;

   protected:
    size_t n;
    bool reversed;

    /**
     * @brief Return a pair of shared vectors representing the two sides of the comparison at level
     * i. Both should be of length `level_size(i)`
     *
     * @param i the level of the prefix network
     * @param input the input (full-span) vector
     * @return std::pair<_S_t, _S_t>
     */
    virtual std::pair<_S_t, _S_t> _level(int i, _S_t& input) const = 0;

    /**
     * @brief Produce a view of `input` which corresponds to the prefix network
     * at level `i`. Calls down into subclasses' `_level()` function and handles
     * casting back to the appropriate type.
     *
     * @tparam ShVec_t either ASharedVector or BSharedVector
     * @param i
     * @param input
     * @return std::pair<ShVec_t, ShVec_t>
     */
    template <typename ShVec_t>
    std::pair<ShVec_t, ShVec_t> level(int i, ShVec_t& input) const {
        assert(0 <= i && i < depth());

        // Switch direction if this is reversed
        auto inr = this->reversed ? input.reversed_view() : input;

        auto [a, b] = _level(i, inr);
        return {ShVec_t(a.vector), ShVec_t(b.vector)};
    }

    /**
     * @brief How many elements participate in this level.
     *
     * @param i
     * @return size_t
     */
    virtual size_t level_size(int i) const = 0;

    /**
     * @brief The depth of this network.
     *
     * @return int
     */
    virtual int depth() const = 0;

    /**
     * @brief Helper struct to handle iterating through a prefix network
     *
     */
    struct LevelFn {
        const PrefixNetwork* parent;
        int i;

        template <typename ShVec_t>
        auto operator()(ShVec_t& v) const {
            return parent->level(i, v);
        }
    };

    struct Level {
        size_t size;
        LevelFn f;
    };

    /**
     * @brief Prefix network iterator struct
     *
     * (This allows us to implement C++ range loops over prefix network)
     *
     */
    struct it {
        int idx;
        const PrefixNetwork* parent;

        bool operator!=(const it& other) const { return idx != other.idx; }

        void operator++() { idx++; }

        Level operator*() const { return {parent->level_size(idx), LevelFn{parent, idx}}; }
    };

   public:
    /**
     * @brief Construct a new Prefix Network
     *
     * @param dir whether this is a forward (down) or reverse (up) aggregation
     * @param _n the size of the network
     */
    PrefixNetwork(enum Direction dir, size_t _n) : reversed(dir == Direction::Reverse), n(_n) {
        if (!is_power_of_two(n)) {
            std::cerr << "prefix network: assertion failed: " << n << " not power of two\n";
            assert(false);
        }
    }

    it begin() const { return {0, this}; }
    it end() const { return {depth(), this}; }
};

/**
 * @brief The Hillis-Steele / Kogge-Stone network. Optimal depth \f$\log_2 n\f$ and highly
 * parallelizable, but not work efficient (\f$n \log n\f$ operations).
 *
 * Example network on 32 elements (each `O` is a gate):
 * ```
 * OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO.
 * OOOOOOOOOOOOOOOOOOOOOOOOOOOOOO..
 * OOOOOOOOOOOOOOOOOOOOOOOOOOOO....
 * OOOOOOOOOOOOOOOOOOOOOOOO........
 * OOOOOOOOOOOOOOOO................
 * ```
 *
 * In this example, we use 80 gates but only 5 rounds.
 *
 * @tparam T
 * @tparam E
 */
template <typename T, typename E>
class HillisSteele : public PrefixNetwork<T, E> {
    using _S_t = SharedVector<T, E>;

    std::pair<_S_t, _S_t> _level(int i, _S_t& input) const {
        auto s = level_size(i);

        return {input.slice(0, s), input.slice(this->n - s)};
    }

    size_t level_size(int i) const {
        auto gap = this->reversed ? (i + 1) : (depth() - i);

        return this->n - (this->n / (1 << gap));
    }

    int depth() const { return std::bit_width(this->n - 1); }

   public:
    HillisSteele(enum Direction dir, size_t _n) : PrefixNetwork<T, E>(dir, _n) {}
};

/**
 * @brief The Brent-Kung network. Conceptually similar to a Blelloch prefix network.
 *
 * Asymptotically optimal depth \f$2 \log_2 n\f$ and work \f$2 n\f$. Not as parallelizable due to a
 * sparser network structure.
 *
 *
 * Example network on 32 elements (each `O` is a gate):
 * ```
 * O.O.O.O.O.O.O.O.O.O.O.O.O.O.O.O.
 * O...O...O...O...O...O...O...O...
 * O.......O.......O.......O.......
 * O...............O...............
 * O...............................  "reduce" phase
 * ........O.......................
 * ....O.......O.......O...........
 * ..O...O...O...O...O...O...O.....
 * .O.O.O.O.O.O.O.O.O.O.O.O.O.O.O..  "distribute" phase
 * ```
 *
 * Here we use only 57 gates, but 9 rounds.
 *
 */
template <typename T, typename E>
class BrentKung : public PrefixNetwork<T, E> {
    using _S_t = SharedVector<T, E>;

    int half_depth() const { return std::bit_width(this->n - 1); }

    std::pair<_S_t, _S_t> _level(int i, _S_t& input) const {
        if (i < half_depth()) {
            // 0 .. hd - 1
            size_t gap = 1 << i;
            auto a = input.subset(1 * gap - 1, gap * 2);
            auto b = input.subset(2 * gap - 1, gap * 2);
            // a might be larger than b -- resize
            a.resize(b.size());

            return {a, b};
        } else {
            // hd - 2 .. 0
            i = 2 * half_depth() - 2 - i;
            size_t gap = 1 << i;
            auto a = input.subset(2 * gap - 1, gap * 2);
            auto b = input.subset(3 * gap - 1, gap * 2);
            a.resize(b.size());

            return {a, b};
        }
    }

    size_t level_size(int i) const {
        if (i < half_depth()) {
            size_t gap = 1 << i;
            return (this->n - 2 * gap) / (2 * gap) + 1;
        } else {
            i = 2 * half_depth() - 2 - i;
            size_t gap = 1 << i;
            return (this->n - 3 * gap) / (2 * gap) + 1;
        }
    }

    int depth() const { return 2 * half_depth() - 1; }

   public:
    BrentKung(enum Direction dir, size_t _n) : PrefixNetwork<T, E>(dir, _n) {}
};

}  // namespace orq::aggregators