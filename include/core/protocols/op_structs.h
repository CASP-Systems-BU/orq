#pragma once

namespace orq {
/**
 * @brief Operator functors for generic binary operations.
 *
 * These structs define the operators used in binary operations like MULT and AND,
 * allowing the control flow to be templated and shared between implementations.
 */

struct ArithmeticOps {
    template <typename T>
    void accumulateOp(T& a, const T& b) const {
        a += b;
    }

    template <typename T>
    T multiplyOp(const T& a, const T& b) const {
        return a * b;
    }

    template <typename T>
    T addOp(const T& a, const T& b) const {
        return a + b;
    }

    // Whether to call truncate() after computation
    static constexpr bool do_truncate = true;
};

struct DotProductOps {
    size_t agg = 0;

    DotProductOps(size_t agg = 0) : agg(agg) {}

    template <typename T>
    void accumulateOp(T& a, const T& b) const {
        a += b;
    }

    template <typename T>
    T multiplyOp(const T& a, const T& b) const {
        return a.dot_product(b, agg);
    }

    template <typename T>
    T addOp(const T& a, const T& b) const {
        return a + b;
    }

    // Whether to call truncate() after computation
    static constexpr bool do_truncate = true;
};

struct BooleanOps {
    // Equivalent of addition
    template <typename T>
    void accumulateOp(T& a, const T& b) const {
        a ^= b;
    }

    // Equivalent of multiplication
    template <typename T>
    T multiplyOp(const T& a, const T& b) const {
        return a & b;
    }

    template <typename T>
    T addOp(const T& a, const T& b) const {
        return a ^ b;
    }

    // No fixed point ops for boolean
    static constexpr bool do_truncate = false;
};
}  // namespace orq