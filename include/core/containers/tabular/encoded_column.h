#pragma once

#include "debug/orq_debug.h"
#include "encoded_table.h"

/**
 * @brief Binary operation on columns
 *
 */
#define binary_op_downcast(_op_, eType, vType)                                        \
    std::unique_ptr<EncodedColumn> operator _op_(const EncodedColumn & other) const { \
        assert(encoding == Encoding::eType && encoding == other.encoding);            \
        auto v1 = static_cast<const vType<Share, EVector> *>(contents.get());         \
        auto v2 = static_cast<const vType<Share, EVector> *>(other.contents.get());   \
        return std::make_unique<SharedColumn>(*v1 _op_ * v2);                         \
    }

/**
 * @brief Binary assignment operation on columns
 *
 */
#define binary_assignment_downcast(_op_, eType, vType)                              \
    EncodedColumn &operator _op_(const EncodedColumn & other) {                     \
        assert(encoding == Encoding::eType && encoding == other.encoding);          \
        auto v1 = static_cast<vType<Share, EVector> *>(contents.get());             \
        auto v2 = static_cast<const vType<Share, EVector> *>(other.contents.get()); \
        *v1 _op_ *v2;                                                               \
        return *this;                                                               \
    }

/**
 * @brief Binary assignment operation on pointer to column
 *
 */
#define binary_assignment_ptr(_op_, eType, vType)                                    \
    EncodedColumn &operator _op_(const std::unique_ptr<EncodedColumn> &&other) {     \
        assert(encoding == Encoding::eType && encoding == other->encoding);          \
        auto v1 = static_cast<vType<Share, EVector> *>(contents.get());              \
        auto v2 = static_cast<const vType<Share, EVector> *>(other->contents.get()); \
        *v1 _op_ *v2;                                                                \
        return *this;                                                                \
    }

/**
 * @brief Unary operation on column
 *
 */
#define unary_op_downcast(_op_, eType, vType)                                \
    std::unique_ptr<EncodedColumn> operator _op_() const {                   \
        assert(encoding == Encoding::eType);                                 \
        auto v = static_cast<const vType<Share, EVector> *>(contents.get()); \
        return std::make_unique<SharedColumn>(_op_(*v));                     \
    }

/**
 * @brief Binary operation which applies to either AShared or BShared columns (resolved at runtime)
 *
 */
#define binary_op_either_type(_op_)                                                             \
    std::unique_ptr<EncodedColumn> operator _op_(const EncodedColumn & other) const {           \
        assert(encoding == other.encoding);                                                     \
        std::unique_ptr<SharedColumn> s;                                                        \
        if (encoding == AShared) {                                                              \
            auto v1 = static_cast<const ASharedVector<Share, EVector> *>(contents.get());       \
            auto v2 = static_cast<const ASharedVector<Share, EVector> *>(other.contents.get()); \
            s = std::make_unique<SharedColumn>(*v1 _op_ * v2);                                  \
        } else {                                                                                \
            auto v1 = static_cast<const BSharedVector<Share, EVector> *>(contents.get());       \
            auto v2 = static_cast<const BSharedVector<Share, EVector> *>(other.contents.get()); \
            s = std::make_unique<SharedColumn>(*v1 _op_ * v2);                                  \
        }                                                                                       \
        s->encoding = encoding;                                                                 \
        return s;                                                                               \
    }

/**
 * @brief Column-constant operations, where we need to convert the constant into a (temporary,
 * public-shared) column.
 *
 * TODO: Replace this once we implement constant ops
 *
 */
#define binary_op_element(_op_, eType, vType)                                                     \
    std::unique_ptr<EncodedColumn> operator _op_(const int64_t & other) const {                   \
        assert(encoding == Encoding::eType);                                                      \
        auto v1 = static_cast<const vType<Share, EVector> *>(contents.get());                     \
        Vector<Share> v2_(1, (Share)other);                                                       \
        vType<Share, EVector> v2 =                                                                \
            orq::service::runTime->public_share<EVector::replicationNumber>(v2_);                 \
        auto s = std::make_unique<SharedColumn>(*v1 _op_ v2.cyclic_subset_reference(v1->size())); \
        s->encoding = encoding;                                                                   \
        return s;                                                                                 \
    }

/**
 * @brief Binary operation with constant which applies to either type (resolved at runtime).
 *
 */
#define binary_op_element_either_type(_op_)                                                    \
    std::unique_ptr<EncodedColumn> operator _op_(const int64_t & other) const {                \
        std::unique_ptr<SharedColumn> s;                                                       \
        Vector<Share> k_(1, (Share)other);                                                     \
        if (encoding == AShared) {                                                             \
            auto v = static_cast<const ASharedVector<Share, EVector> *>(contents.get());       \
            ASharedVector<Share, EVector> ka =                                                 \
                orq::service::runTime->public_share<EVector::replicationNumber>(k_);           \
            s = std::make_unique<SharedColumn>(*v _op_ ka.cyclic_subset_reference(v->size())); \
        } else {                                                                               \
            auto v = static_cast<const BSharedVector<Share, EVector> *>(contents.get());       \
            BSharedVector<Share, EVector> kb =                                                 \
                orq::service::runTime->public_share<EVector::replicationNumber>(k_);           \
            s = std::make_unique<SharedColumn>(*v _op_ kb.cyclic_subset_reference(v->size())); \
        }                                                                                      \
        s->encoding = encoding;                                                                \
        return s;                                                                              \
    }

/**
 * @brief True column-constant operations, like shifts and public division
 *
 */
#define binary_op_fixed_element(_op_, eType, vType)                            \
    std::unique_ptr<EncodedColumn> operator _op_(const int64_t & y) const {    \
        assert(encoding == Encoding::eType);                                   \
        using T = vType<Share, EVector>;                                       \
        auto v = static_cast<const T *>(contents.get());                       \
        return std::make_unique<SharedColumn>(std::make_unique<T>(*v _op_ y)); \
    }

/**
 * @brief Binary operation with pointer-pointer to column
 *
 */
#define binary_op_ptr_element(_op_)                                                \
    std::unique_ptr<EncodedColumn> operator _op_(std::unique_ptr<EncodedColumn> x, \
                                                 const int64_t & y) {              \
        return *x _op_ y;                                                          \
    }

namespace orq::relational {

/**
 * @brief An encoded table column that supports vectorized secure operations. Internally stores an
 * encoded vector, and passes all operations down to that vector.
 *
 */
class EncodedColumn {
    template <typename S, typename C, typename A, typename B, typename V, typename D>
    friend class EncodedTable;

   protected:
    virtual void tail(size_t) = 0;
    virtual void resize(size_t) = 0;

   public:
    /**
     * The column's encoding (e.g., A-shared, B-shared, etc.)
     */
    Encoding encoding;

    /**
     * The column's contents (i.e., an encoded vector).
     */
    std::unique_ptr<EncodedVector> contents;

    /**
     * The column's name
     */
    std::string name;

    virtual ~EncodedColumn() {};

    /**
     * @return The column's size in number of elements.
     */
    virtual size_t size() const = 0;

    virtual void zero() = 0;

    virtual std::unique_ptr<EncodedColumn> deepcopy() = 0;
    virtual std::unique_ptr<EncodedColumn> ltz() = 0;

    /**
     * Elementwise secure arithmetic addition.
     * @param other The second operand of addition.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise additions.
     */
    virtual std::unique_ptr<EncodedColumn> operator+(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator+(const int64_t &other) const = 0;
    virtual EncodedColumn &operator+=(const EncodedColumn &other) = 0;

    /**
     * Elementwise secure arithmetic subtraction.
     * @param other The second operand of subtraction.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise subtractions.
     */
    virtual std::unique_ptr<EncodedColumn> operator-(const EncodedColumn &other) const = 0;
    virtual EncodedColumn &operator-=(const EncodedColumn &other) = 0;

    /**
     * Elementwise secure arithmetic negation.
     * @return A unique pointer to an EncodedColumn with all elements of `this` EncodedColumn
     * negated.
     */
    virtual std::unique_ptr<EncodedColumn> operator-() const = 0;

    /**
     * Elementwise secure arithmetic multiplication.
     * @param other The second operand of multiplication.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise multiplications.
     */
    virtual std::unique_ptr<EncodedColumn> operator*(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator*(const int64_t &other) const = 0;
    virtual EncodedColumn &operator*=(const EncodedColumn &other) = 0;

    /**
     * @brief Elementwise secure schoolbook (binary) division
     *
     * @param other
     * @return std::unique_ptr<EncodedColumn>
     */
    virtual std::unique_ptr<EncodedColumn> operator/(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator/(const int64_t &other) const = 0;

    // **************************************** //
    //             Boolean operators            //
    // **************************************** //

    /**
     * Elementwise secure bitwise XOR.
     * @param other The second operand of XOR.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise XORs.
     */
    virtual std::unique_ptr<EncodedColumn> operator^(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator^(const int64_t &other) const = 0;
    virtual EncodedColumn &operator^=(const EncodedColumn &other) = 0;

    /**
     * Elementwise secure bitwise AND.
     * @param other The second operand of AND.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise ANDs.
     */
    virtual std::unique_ptr<EncodedColumn> operator&(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator&(const int64_t &other) const = 0;
    virtual EncodedColumn &operator&=(const EncodedColumn &other) = 0;

    /**
     * Elementwise secure bitwise OR.
     * @param other The second operand of OR.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise ORs.
     */
    virtual std::unique_ptr<EncodedColumn> operator|(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator|(const int64_t &other) const = 0;
    virtual EncodedColumn &operator|=(const EncodedColumn &other) = 0;

    /**
     * Elementwise secure boolean completion.
     * @return A unique pointer to an EncodedColumn with all elements of `this` EncodedColumn
     * complemented.
     */
    virtual std::unique_ptr<EncodedColumn> operator~() const = 0;

    /**
     * Elementwise secure boolean negation.
     * @return A unique pointer to an EncodedColumn with all results of `this` EncodedColumn
     * negated.
     */
    virtual std::unique_ptr<EncodedColumn> operator!() const = 0;

    /**
     * @brief Elementwise shifts.
     *
     * @param y shift amount
     * @return std::unique_ptr<EncodedColumn>
     */
    virtual std::unique_ptr<EncodedColumn> operator<<(const int64_t &y) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator>>(const int64_t &y) const = 0;

    // **************************************** //
    //           Comparison operators           //
    // **************************************** //

    /**
     * Elementwise secure equality.
     * @param other The second operand of equality.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise equality comparisons.
     */
    virtual std::unique_ptr<EncodedColumn> operator==(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator==(const int64_t &other) const = 0;

    /**
     * Elementwise secure inequality.
     * @param other The second operand of inequality.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise inequality comparisons.
     */
    virtual std::unique_ptr<EncodedColumn> operator!=(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator!=(const int64_t &other) const = 0;

    /**
     * Elementwise secure greater-than.
     * @param other The second operand of greater-than.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise greater-than comparisons.
     */
    virtual std::unique_ptr<EncodedColumn> operator>(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator>(const int64_t &other) const = 0;

    /**
     * Elementwise secure greater-or-equal.
     * @param other The second operand of greater-or-equal.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise greater-or-equal comparisons.
     */
    virtual std::unique_ptr<EncodedColumn> operator>=(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator>=(const int64_t &other) const = 0;

    /**
     * Elementwise secure less-than.
     * @param other The second operand of less-than.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise less-than comparisons.
     */
    virtual std::unique_ptr<EncodedColumn> operator<(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator<(const int64_t &other) const = 0;

    /**
     * Elementwise secure less-or-equal.
     * @param other The second operand of less-or-equal.
     * @return A unique pointer to an EncodedColumn that contains encoded results of
     * the elementwise less-or-equal comparisons.
     */
    virtual std::unique_ptr<EncodedColumn> operator<=(const EncodedColumn &other) const = 0;
    virtual std::unique_ptr<EncodedColumn> operator<=(const int64_t &other) const = 0;

    /**
     * @brief Column assignment
     *
     * @param other The unique pointer to the column whose contents will be moved to `this`
     * column.
     * @return A reference to `this` column after modification.
     */
    virtual EncodedColumn &operator=(std::unique_ptr<EncodedColumn> &&other) = 0;

    // TODO (john): Maybe we also need a copy assignment, e.g., in case we want to copy a column
    // into another
};

// NOTE: Although these operator templates are defined in namespace orq, we must now redefine
// them in orq::relational

// Binary operators between T (left) and std::unique_pointer<T> (right)
shares_define_reference_pointer_operator_no_move(+);
shares_define_reference_pointer_operator_no_move(-);
shares_define_reference_pointer_operator_no_move(*);
shares_define_reference_pointer_operator_no_move(/);
shares_define_reference_pointer_operator_no_move(&);
shares_define_reference_pointer_operator_no_move(|);
shares_define_reference_pointer_operator_no_move(^);

// Binary operators between std::unique_pointer<T> (left) and T (right)
shares_define_pointer_reference_operator_no_move(+);
shares_define_pointer_reference_operator_no_move(-);
shares_define_pointer_reference_operator_no_move(*);
shares_define_pointer_reference_operator_no_move(/);
shares_define_pointer_reference_operator_no_move(&);
shares_define_pointer_reference_operator_no_move(|);
shares_define_pointer_reference_operator_no_move(^);

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

// Binary operators where both operands are std::unique_pointer<T>&&
shares_define_pointers_operator(+);
shares_define_pointers_operator(-);
shares_define_pointers_operator(*);
shares_define_pointers_operator(/);
shares_define_pointers_operator(&);
shares_define_pointers_operator(|);
shares_define_pointers_operator(^);
shares_define_pointers_operator(==);

// Binary operators where both operands are std::unique_pointer<T>
shares_define_pointers_operator_no_move(+);
shares_define_pointers_operator_no_move(-);
shares_define_pointers_operator_no_move(*);
shares_define_pointers_operator_no_move(/);
shares_define_pointers_operator_no_move(&);
shares_define_pointers_operator_no_move(|);
shares_define_pointers_operator_no_move(^);
shares_define_pointers_operator_no_move(==);

define_assignment_ptr_operator(+=);
define_assignment_ptr_operator(-=);
define_assignment_ptr_operator(*=);
define_assignment_ptr_operator(/=);
define_assignment_ptr_operator(&=);
define_assignment_ptr_operator(|=);
define_assignment_ptr_operator(^=);

// Unary operators on std::unique_pointer<T>
shares_define_pointer_operator_no_move(-);
shares_define_pointer_operator_no_move(~);
shares_define_pointer_operator_no_move(!);
}  // namespace orq::relational