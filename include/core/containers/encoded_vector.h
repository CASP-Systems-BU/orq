#ifndef SECRECY_ENCODED_VECTOR_H
#define SECRECY_ENCODED_VECTOR_H

#include <memory>

#include "../../debug/debug.h"
#include "../../service/common/runtime.h"
#include "encoding.h"

#define shares_define_pointer_reference_operator(op)                      \
    template <typename T>                                                 \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x, const T & y) { \
        auto& x_ = *x.get();                                            \
        assert(x_.size() == y.size());                                    \
        return x_ op y;                                                   \
    }

#define shares_define_reference_pointer_operator(op)                      \
    template <typename T>                                                 \
    std::unique_ptr<T> operator op(const T & x, std::unique_ptr<T>&& y) { \
        auto& y_ = y.get()[0];                                            \
        assert(x.size() == y_.size());                                    \
        return x op y_;                                                   \
    }

// An assignment operator between two unique pointers. Returns the left
// (modified) argument.
#define define_assignment_ptr_operator(op)                                           \
    template <typename T>                                                            \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x, std::unique_ptr<T>&& y) { \
        auto& x_ = *x.get();                                                         \
        auto& y_ = *y.get();                                                         \
        assert(x_.size() == y_.size());                                              \
        x_ &= y_;                                                                    \
        return x;                                                                    \
    }

#define shares_define_pointers_operator(op)                                          \
    template <typename T>                                                            \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x, std::unique_ptr<T>&& y) { \
        auto& x_ = *x.get();                                                       \
        auto& y_ = *y.get();                                                       \
        assert(x_.size() == y_.size());                                              \
        return x_ op y_;                                                             \
    }

#define shares_define_pointer_operator(op)                   \
    template <typename T>                                    \
    std::unique_ptr<T> operator op(std::unique_ptr<T>&& x) { \
        auto& x_ = *x.get();                               \
        return op x_;                                        \
    }

#define shares_define_pointer_reference_operator_no_move(op)             \
    template <typename T>                                                \
    std::unique_ptr<T> operator op(std::unique_ptr<T>& x, const T & y) { \
        auto& x_ = *x.get();                                           \
        assert(x_.size() == y.size());                                   \
        return x_ op y;                                                  \
    }

#define shares_define_reference_pointer_operator_no_move(op)             \
    template <typename T>                                                \
    std::unique_ptr<T> operator op(const T & x, std::unique_ptr<T>& y) { \
        auto& y_ = *y.get();                                           \
        assert(x.size() == y_.size());                                   \
        return x op y_;                                                  \
    }

#define shares_define_pointers_operator_no_move(op)                                \
    template <typename T>                                                          \
    std::unique_ptr<T> operator op(std::unique_ptr<T>& x, std::unique_ptr<T>& y) { \
        auto& x_ = *x.get();                                                       \
        auto& y_ = *y.get();                                                       \
        assert(x_.size() == y_.size());                                            \
        return x_ op y_;                                                           \
    }

#define shares_define_pointer_operator_no_move(op)          \
    template <typename T>                                   \
    std::unique_ptr<T> operator op(std::unique_ptr<T>& x) { \
        auto& x_ = *x.get();                              \
        return op x_;                                       \
    }

#define binary_op(_op_, type, func, this, y)                        \
    std::unique_ptr<type> operator _op_(const type & y) const {     \
        auto& x_vector = this->vector;                              \
        auto& y_vector = reinterpret_cast<const type*>(&y)->vector; \
        assert(x_vector.size() == y_vector.size());                 \
        auto res = std::make_unique<type>(x_vector.size());         \
        service::runTime->func(x_vector, y_vector, res->vector);    \
        return res;                                                 \
    }

#define binary_element_op(_op_, func, T, S)                                                 \
    std::unique_ptr<T> operator _op_(S y) const {                                           \
        Vector<S> yv({y});                                                                  \
        auto ypv = secrecy::service::runTime->public_share<EVector::replicationNumber>(yv); \
        return *this _op_ ypv.repeated_subset_reference(this->size());                      \
    }

// TODO: actually reimplement as above, rather than just calling, so we can
// avoid creating a new vec.
#define compound_assignment_op(_op_, func, T)                    \
    inline T operator _op_(const T& y) {                         \
        auto& x_vector = this->vector;                           \
        auto& y_vector = reinterpret_cast<const T*>(&y)->vector; \
        assert(x_vector.size() == y_vector.size());              \
        service::runTime->func(x_vector, y_vector, x_vector);    \
        return *this;                                            \
    }

#define compound_assignment_element_op(_op_, func, T, S)                                    \
    inline void operator _op_(S y) {                                                        \
        Vector<S> yv({y});                                                                  \
        auto ypv = secrecy::service::runTime->public_share<EVector::replicationNumber>(yv); \
        *this _op_ ypv.cyclic_subset_reference(this->size());                               \
    }

#define compound_assignment_ptr_op(_op_, func, T)  \
    T& operator _op_(const std::unique_ptr<T> y) { \
        auto& x_ = this->vector;                   \
        auto& y_ = y->vector;                      \
        assert(x_.size() == y_.size());            \
        service::runTime->func(x_, y_, x_);        \
        return *this;                              \
    }

#define unary_op(_op_, type, func, this)                   \
    std::unique_ptr<type> operator _op_() const {          \
        auto res = std::make_unique<type>(this->size());   \
        service::runTime->func(this->vector, res->vector); \
        return res;                                        \
    }

#define fn_no_input(func, type, this)                      \
    std::unique_ptr<type> func() const {                   \
        auto res = std::make_unique<type>(this->size());   \
        service::runTime->func(this->vector, res->vector); \
        return res;                                        \
    }

#define fn_input_no_return(func, arg, this)                   \
    template <typename T>                                     \
    std::unique_ptr<type> func(T arg) {                       \
        auto res = service::runTime->func(this->vector, arg); \
    }

#define svector_reference(TYPE, REF)            \
    template <typename... T>                    \
    TYPE REF(T... args) const {                 \
        return TYPE(this->vector.REF(args...)); \
    }

namespace secrecy {
/**
 * A EncodedVector is the main programming abstraction in Secrecy. All secure operators are applied
 to encoded
 * vectors, i.e., they take one or more encoded vectors as input and generate one or more encoded
 vectors as output.
 *
 * Secrecy computing parties perform secure operations on their encoded vectors as if they were
 performing plaintext
 * operations on the original (secret) vectors. For example, given two secret vectors v1 and v2,
 Secrecy parties
 * can execute the elementwise addition of the secret vectors by simply running v1 + v2 using their
 local encoded
 * vectors. This way, the Secrecy program looks identical to the respective plaintext program, as
 shown below:
 *
 * **Plaintext program** (not secure)
 \verbatim
   // Executed by a trusted party
   auto w = v1 + v2;   // These are C++ vectors and '+' is the elementwise plaintext addition
 \endverbatim
 * **Secrecy program** (secure)
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
     * @param eType - The vector's encoding (e.g., A-shared, B-shared, etc.)
     */
    EncodedVector(const Encoding& eType) : encoding(eType) {};

    // Destructor
    virtual ~EncodedVector() {};

    virtual VectorSizeType size() const = 0;

    // TODO: how to generalize this "int"?
    // virtual inline secrecy::Vector<int>& operator()(const int& index) = 0;

    // Friend class
    friend class EncodedColumn;
};

// **************************************** //
//           Overloaded operators           //
//         (for std::unique_pointer)        //
// **************************************** //

// Binary operators between T (left) and std::unique_pointer<T> (right)
shares_define_reference_pointer_operator(+);
shares_define_reference_pointer_operator(-);
shares_define_reference_pointer_operator(*);
shares_define_reference_pointer_operator(/);
shares_define_reference_pointer_operator(&);
shares_define_reference_pointer_operator(|);
shares_define_reference_pointer_operator(^);

// Binary operators between std::unique_pointer<T> (left) and T (right)
shares_define_pointer_reference_operator(+);
shares_define_pointer_reference_operator(-);
shares_define_pointer_reference_operator(*);
shares_define_pointer_reference_operator(/);
shares_define_pointer_reference_operator(&);
shares_define_pointer_reference_operator(|);
shares_define_pointer_reference_operator(^);

// Binary operators where both operands are std::unique_pointer<T>
shares_define_pointers_operator(+);
shares_define_pointers_operator(-);
shares_define_pointers_operator(*);
shares_define_pointers_operator(/);
shares_define_pointers_operator(&);
shares_define_pointers_operator(|);
shares_define_pointers_operator(^);

define_assignment_ptr_operator(+=);
define_assignment_ptr_operator(-=);
define_assignment_ptr_operator(*=);
define_assignment_ptr_operator(/=);
define_assignment_ptr_operator(&=);
define_assignment_ptr_operator(|=);
define_assignment_ptr_operator(^=);

// Unary operators on std::unique_pointer<T>
shares_define_pointer_operator(-);
shares_define_pointer_operator(~);
shares_define_pointer_operator(!);
}  // namespace secrecy

#endif  // SECRECY_ENCODED_VECTOR_H
