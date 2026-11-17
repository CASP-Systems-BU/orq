#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "backend/common/runtime.h"
#include "debug/orq_debug.h"
#include "encoding.h"

namespace orq {
template <typename T>
inline bool vector_owns_storage(const T& t) {
    if constexpr (requires { t.owns_storage(); }) {
        return t.owns_storage();
    }
    return true;
}
}  // namespace orq

#define shares_define_pointer_reference_operator(op)                      \
    template <typename T>                                                 \
        requires requires(const T& t) { t.size(); }                       \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x, const T & y) { \
        auto& x_ = *x;                                                    \
        assert(x_.size() == y.size());                                    \
        return x_ op y;                                                   \
    }

#define shares_define_reference_pointer_operator(op)                      \
    template <typename T>                                                 \
        requires requires(const T& t) { t.size(); }                       \
    std::unique_ptr<T> operator op(const T & x, std::unique_ptr<T>&& y) { \
        auto& y_ = *y;                                                    \
        assert(x.size() == y_.size());                                    \
        return x op y_;                                                   \
    }

#define define_assignment_ptr_operator(op)                                           \
    template <typename T>                                                            \
        requires requires(const T& t) { t.size(); }                                  \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x, std::unique_ptr<T>&& y) { \
        auto& x_ = *x;                                                               \
        auto& y_ = *y;                                                               \
        assert(x_.size() == y_.size());                                              \
        x_ op y_;                                                                    \
        return std::move(x);                                                         \
    }

#define define_assignment_ptr_operator_no_move(op)                                  \
    template <typename T>                                                           \
        requires requires(const T& t) { t.size(); }                                 \
    std::unique_ptr<T>& operator op(std::unique_ptr<T>& x, std::unique_ptr<T>& y) { \
        auto& x_ = *x;                                                              \
        auto& y_ = *y;                                                              \
        assert(x_.size() == y_.size());                                             \
        x_ op y_;                                                                   \
        return x;                                                                   \
    }

#define shares_define_pointers_operator(op)                                          \
    template <typename T>                                                            \
        requires requires(const T& t) { t.size(); }                                  \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x, std::unique_ptr<T>&& y) { \
        auto& x_ = *x;                                                               \
        auto& y_ = *y;                                                               \
        assert(x_.size() == y_.size());                                              \
        return x_ op y_;                                                             \
    }

#define shares_define_pointer_operator(op)                   \
    template <typename T>                                    \
        requires requires(const T& t) { t.size(); }          \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x) { \
        return op(std::move(*x));                            \
    }

#define shares_define_pointer_reference_operator_no_move(op)             \
    template <typename T>                                                \
        requires requires(const T& t) { t.size(); }                      \
    std::unique_ptr<T> operator op(std::unique_ptr<T>& x, const T & y) { \
        auto& x_ = *x;                                                   \
        assert(x_.size() == y.size());                                   \
        return x_ op y;                                                  \
    }

#define shares_define_reference_pointer_operator_no_move(op)             \
    template <typename T>                                                \
        requires requires(const T& t) { t.size(); }                      \
    std::unique_ptr<T> operator op(const T & x, std::unique_ptr<T>& y) { \
        auto& y_ = *y;                                                   \
        assert(x.size() == y_.size());                                   \
        return x op y_;                                                  \
    }

#define shares_define_pointers_operator_no_move(op)                                \
    template <typename T>                                                          \
        requires requires(const T& t) { t.size(); }                                \
    std::unique_ptr<T> operator op(std::unique_ptr<T>& x, std::unique_ptr<T>& y) { \
        auto& x_ = *x;                                                             \
        auto& y_ = *y;                                                             \
        assert(x_.size() == y_.size());                                            \
        return x_ op y_;                                                           \
    }

#define shares_define_pointer_operator_no_move(op)          \
    template <typename T>                                   \
        requires requires(const T& t) { t.size(); }         \
    std::unique_ptr<T> operator op(std::unique_ptr<T>& x) { \
        return op(*x);                                      \
    }

#define shares_define_pointer_scalar_operator(op)                \
    template <typename T, typename S>                            \
        requires requires(const T& t) { t.size(); } &&           \
                 (!std::is_same_v<std::remove_cvref_t<S>, T>)    \
    std::unique_ptr<T> operator op(std::unique_ptr<T>& x, S y) { \
        return (*x)op y;                                         \
    }

#define shares_define_pointer_scalar_rvalue(op)                   \
    template <typename T, typename S>                             \
        requires requires(const T& t) { t.size(); } &&            \
                 (!std::is_same_v<std::remove_cvref_t<S>, T>)     \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x, S y) { \
        return (*x)op y;                                          \
    }

#define shares_define_pointer_reference_reuse(op, assign_op, trait, enc)             \
    template <typename T>                                                            \
        requires requires(const T& t) { t.size(); }                                  \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x, const T & y) {            \
        assert(x->size() == y.size());                                               \
        if constexpr (requires { T::trait; }) {                                      \
            if constexpr (T::trait) {                                                \
                if (orq::vector_owns_storage(*x)) {                                  \
                    *x assign_op y;                                                  \
                    return std::move(x);                                             \
                }                                                                    \
            }                                                                        \
        } else if constexpr (requires {                                              \
                                 x->encoding;                                        \
                                 (*x) assign_op y;                                   \
                             }) {                                                    \
            if (x->encoding == orq::Encoding::enc && orq::vector_owns_storage(*x)) { \
                *x assign_op y;                                                      \
                return std::move(x);                                                 \
            }                                                                        \
        }                                                                            \
        return *x op y;                                                              \
    }

#define shares_define_pointers_reuse(op, assign_op, trait, enc)                      \
    template <typename T>                                                            \
        requires requires(const T& t) { t.size(); }                                  \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x, std::unique_ptr<T>&& y) { \
        assert(x->size() == y->size());                                              \
        if constexpr (requires { T::trait; }) {                                      \
            if constexpr (T::trait) {                                                \
                if (orq::vector_owns_storage(*x)) {                                  \
                    *x assign_op* y;                                                 \
                    return std::move(x);                                             \
                }                                                                    \
            }                                                                        \
        } else if constexpr (requires {                                              \
                                 x->encoding;                                        \
                                 (*x) assign_op * y;                                 \
                             }) {                                                    \
            if (x->encoding == orq::Encoding::enc && orq::vector_owns_storage(*x)) { \
                *x assign_op* y;                                                     \
                return std::move(x);                                                 \
            }                                                                        \
        }                                                                            \
        return *x op * y;                                                            \
    }

#define shares_define_pointer_scalar_reuse(op, assign_op, trait, enc)                \
    template <typename T, typename S>                                                \
        requires requires(const T& t) { t.size(); } &&                               \
                 (!std::is_same_v<std::remove_cvref_t<S>, T>)                        \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x, S y) {                    \
        if constexpr (requires { T::trait; }) {                                      \
            if constexpr (T::trait) {                                                \
                if (orq::vector_owns_storage(*x)) {                                  \
                    *x assign_op y;                                                  \
                    return std::move(x);                                             \
                }                                                                    \
            }                                                                        \
        } else if constexpr (requires {                                              \
                                 x->encoding;                                        \
                                 (*x) assign_op y;                                   \
                             }) {                                                    \
            if (x->encoding == orq::Encoding::enc && orq::vector_owns_storage(*x)) { \
                *x assign_op y;                                                      \
                return std::move(x);                                                 \
            }                                                                        \
        }                                                                            \
        return (*x)op y;                                                             \
    }

#define ORQ_UNIQUE_PTR_CMP_OPS(macro) \
    macro(==);                        \
    macro(!=);                        \
    macro(<);                         \
    macro(>);                         \
    macro(<=);                        \
    macro(>=);

#define ORQ_UNIQUE_PTR_ARITH_OPS(macro) \
    macro(+);                           \
    macro(-);                           \
    macro(*);                           \
    macro(/);                           \
    macro(&);                           \
    macro(|);                           \
    macro(^);

#define ORQ_INSTANTIATE_UNIQUE_PTR_REUSE()                              \
    shares_define_pointer_reference_reuse(+, +=, inplace_add, AShared); \
    shares_define_pointer_reference_reuse(-, -=, inplace_sub, AShared); \
    shares_define_pointer_reference_reuse(*, *=, inplace_mul, AShared); \
    shares_define_pointer_reference_reuse(&, &=, inplace_and, BShared); \
    shares_define_pointer_reference_reuse(|, |=, inplace_or, BShared);  \
    shares_define_pointer_reference_reuse(^, ^=, inplace_xor, BShared); \
    shares_define_pointers_reuse(+, +=, inplace_add, AShared);          \
    shares_define_pointers_reuse(-, -=, inplace_sub, AShared);          \
    shares_define_pointers_reuse(*, *=, inplace_mul, AShared);          \
    shares_define_pointers_reuse(&, &=, inplace_and, BShared);          \
    shares_define_pointers_reuse(|, |=, inplace_or, BShared);           \
    shares_define_pointers_reuse(^, ^=, inplace_xor, BShared);          \
    shares_define_pointer_scalar_reuse(+, +=, inplace_add, AShared);    \
    shares_define_pointer_scalar_reuse(-, -=, inplace_sub, AShared);    \
    shares_define_pointer_scalar_reuse(*, *=, inplace_mul, AShared);    \
    shares_define_pointer_scalar_reuse(&, &=, inplace_and, BShared);    \
    shares_define_pointer_scalar_reuse(|, |=, inplace_or, BShared);     \
    shares_define_pointer_scalar_reuse(^, ^=, inplace_xor, BShared);    \
    shares_define_pointer_scalar_reuse(<<, <<=, inplace_and, BShared);  \
    shares_define_pointer_scalar_reuse(>>, >>=, inplace_and, BShared);

#define binary_op(_op_, type, func, this, y)                     \
    std::unique_ptr<type> operator _op_(const type & y) const {  \
        auto& x_vector = this->vector;                           \
        auto& y_vector = y.vector;                               \
        assert(x_vector.size() == y_vector.size());              \
        auto res = std::make_unique<type>(x_vector.size());      \
        service::runTime->func(x_vector, y_vector, res->vector); \
        res->vector.matchPrecision(this->vector);                \
        return res;                                              \
    }

#define binary_element_op(_op_, func, T, S)                                             \
    std::unique_ptr<T> operator _op_(S y) const {                                       \
        Vector<S> yv({y});                                                              \
        auto ypv = orq::service::runTime->public_share<EVector::replicationNumber>(yv); \
        return *this _op_ ypv.repeated_subset_reference(this->size());                  \
    }

#define compound_assignment_op(_op_, func, T)                 \
    inline T& operator _op_(const T & y) {                    \
        auto& x_vector = this->vector;                        \
        auto& y_vector = y.vector;                            \
        assert(x_vector.size() == y_vector.size());           \
        service::runTime->func(x_vector, y_vector, x_vector); \
        return *this;                                         \
    }

#define compound_assignment_element_op(_op_, func, T, S)                                \
    inline T& operator _op_(S y) {                                                      \
        Vector<S> yv({y});                                                              \
        auto ypv = orq::service::runTime->public_share<EVector::replicationNumber>(yv); \
        *this _op_ ypv.cyclic_subset_reference(this->size());                           \
        return *this;                                                                   \
    }

#define compound_assignment_ptr_op(_op_, func, T)   \
    T& operator _op_(const std::unique_ptr<T>& y) { \
        auto& x_ = this->vector;                    \
        auto& y_ = y->vector;                       \
        assert(x_.size() == y_.size());             \
        service::runTime->func(x_, y_, x_);         \
        return *this;                               \
    }

#define unary_op(_op_, type, func, this)                                         \
    std::unique_ptr<type> operator _op_() const& {                               \
        auto res = std::make_unique<type>(this->size());                         \
        service::runTime->func(this->vector, res->vector);                       \
        return res;                                                              \
    }                                                                            \
    std::unique_ptr<type> operator _op_() && {                                   \
        if (this->owns_storage()) {                                              \
            service::runTime->func(this->vector, this->vector);                  \
            return std::make_unique<type>(std::move(*static_cast<type*>(this))); \
        }                                                                        \
        return _op_ static_cast<const type&>(*this);                             \
    }

#define fn_no_input(func, type, this)                      \
    std::unique_ptr<type> func() const {                   \
        auto res = std::make_unique<type>(this->size());   \
        service::runTime->func(this->vector, res->vector); \
        return res;                                        \
    }

#define svector_reference(TYPE, REF)            \
    template <typename... T>                    \
    TYPE REF(T... args) const {                 \
        return TYPE(this->vector.REF(args...)); \
    }

namespace orq {
/**
 * A EncodedVector is the main programming abstraction in ORQ. All secure operators are applied
 * to encoded vectors, i.e., they take one or more encoded vectors as input and generate one or more
 * encoded vectors as output. ORQ computing parties perform secure operations on their encoded
 * vectors as if they were performing plaintext operations on the original (secret) vectors. For
 * example, given two secret vectors v1 and v2, ORQ parties can execute the elementwise addition of
 * the secret vectors by simply running v1 + v2 using their local encoded vectors. This way, the ORQ
 * program looks identical to the respective plaintext program, as shown below:
 *
 * **Plaintext program** (not secure)
 \verbatim
   // Executed by a trusted party
   auto w = v1 + v2;   // These are C++ vectors and '+' is the elementwise plaintext addition
 \endverbatim
 * **ORQ program** (secure)
 \verbatim
   // Executed by an untrusted party
   auto w = v1 + v2;   // These are EncodedVectors and '+' is the elementwise secure addition
 \endverbatim
 */
class EncodedVector {
   public:
    /**
     * The vector's encoding (e.g., A-shared, B-shared, etc.)
     */
    Encoding encoding;

    /**
     * Constructor
     * @param eType The vector's encoding (e.g., A-shared, B-shared, etc.)
     */
    EncodedVector(const Encoding& eType) : encoding(eType) {};

    // Destructor
    virtual ~EncodedVector() {};

    virtual VectorSizeType size() const = 0;

    virtual bool owns_storage() const { return false; }

    /**
     * Gets the fixed-point precision. Virtual to let subclasses override.
     */
    virtual void setPrecision(const int fixed_point_precision) = 0;

    /**
     * Gets the fixed-point precision. Virtual to let subclasses override.
     */
    virtual size_t getPrecision() const = 0;

    // TODO: how to generalize this "int"?
    // virtual inline orq::Vector<int>& operator()(int index) = 0;

    // Friend class
    friend class EncodedColumn;
};

// **************************************** //
//           Overloaded operators           //
//         (for std::unique_pointer)        //
// **************************************** //

ORQ_UNIQUE_PTR_ARITH_OPS(shares_define_reference_pointer_operator);
ORQ_UNIQUE_PTR_CMP_OPS(shares_define_reference_pointer_operator);
ORQ_UNIQUE_PTR_CMP_OPS(shares_define_pointer_reference_operator);
ORQ_UNIQUE_PTR_CMP_OPS(shares_define_pointers_operator);
shares_define_pointer_reference_operator(/);
shares_define_pointers_operator(/);

ORQ_INSTANTIATE_UNIQUE_PTR_REUSE();

ORQ_UNIQUE_PTR_ARITH_OPS(shares_define_reference_pointer_operator_no_move);
ORQ_UNIQUE_PTR_CMP_OPS(shares_define_reference_pointer_operator_no_move);
ORQ_UNIQUE_PTR_ARITH_OPS(shares_define_pointer_reference_operator_no_move);
ORQ_UNIQUE_PTR_CMP_OPS(shares_define_pointer_reference_operator_no_move);
ORQ_UNIQUE_PTR_ARITH_OPS(shares_define_pointers_operator_no_move);
ORQ_UNIQUE_PTR_CMP_OPS(shares_define_pointers_operator_no_move);

shares_define_pointer_scalar_operator(+);
shares_define_pointer_scalar_operator(-);
shares_define_pointer_scalar_operator(*);
shares_define_pointer_scalar_operator(/);
shares_define_pointer_scalar_operator(&);
shares_define_pointer_scalar_operator(|);
shares_define_pointer_scalar_operator(^);
shares_define_pointer_scalar_operator(<<);
shares_define_pointer_scalar_operator(>>);
shares_define_pointer_scalar_operator(==);
shares_define_pointer_scalar_operator(!=);
shares_define_pointer_scalar_operator(<);
shares_define_pointer_scalar_operator(>);
shares_define_pointer_scalar_operator(<=);
shares_define_pointer_scalar_operator(>=);

shares_define_pointer_scalar_rvalue(/);
shares_define_pointer_scalar_rvalue(==);
shares_define_pointer_scalar_rvalue(!=);
shares_define_pointer_scalar_rvalue(<);
shares_define_pointer_scalar_rvalue(>);
shares_define_pointer_scalar_rvalue(<=);
shares_define_pointer_scalar_rvalue(>=);

define_assignment_ptr_operator(+=);
define_assignment_ptr_operator(-=);
define_assignment_ptr_operator(*=);
define_assignment_ptr_operator(/=);
define_assignment_ptr_operator(&=);
define_assignment_ptr_operator(|=);
define_assignment_ptr_operator(^=);
define_assignment_ptr_operator_no_move(+=);
define_assignment_ptr_operator_no_move(-=);
define_assignment_ptr_operator_no_move(*=);
define_assignment_ptr_operator_no_move(/=);
define_assignment_ptr_operator_no_move(&=);
define_assignment_ptr_operator_no_move(|=);
define_assignment_ptr_operator_no_move(^=);

shares_define_pointer_operator(-);
shares_define_pointer_operator(~);
shares_define_pointer_operator(!);
shares_define_pointer_operator_no_move(-);
shares_define_pointer_operator_no_move(~);
}  // namespace orq
