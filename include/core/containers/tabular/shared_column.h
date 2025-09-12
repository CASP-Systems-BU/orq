#pragma once

#include "core/containers/a_shared_vector.h"
#include "core/containers/b_shared_vector.h"
#include "core/containers/e_vector.h"
#include "encoded_column.h"

namespace orq::relational {

/**
 * A secret-shared column with share and container types.
 *
 * @tparam Share Share data type.
 * @tparam EVector Container data type.
 */
template <typename Share, typename EVector>
class SharedColumn : public EncodedColumn {
    using SVector = SharedVector<Share, EVector>;

    template <typename S, typename C, typename A, typename B, typename V, typename D>
    friend class EncodedTable;

   protected:
    void resize(size_t n) { static_cast<SVector*>(contents.get())->resize(n); }

    void tail(size_t n) { static_cast<SVector*>(contents.get())->tail(n); }

   public:
    static const int replicationNumber = EVector::replicationNumber;

    /**
     * Allocates a shared column with the given encoding and initializes it with zeros.
     * @param _size The column's size in number of elements.
     * @param eType The column's encoding (e.g., A-Shared, B-Shared, etc.).
     */
    explicit SharedColumn(const size_t& _size, const Encoding& eType) {
        switch (eType) {
            case Encoding::AShared: {
                contents = std::make_unique<ASharedVector<Share, EVector>>(_size);
                break;
            }
            case Encoding::BShared: {
                contents = std::make_unique<BSharedVector<Share, EVector>>(_size);
                break;
            }
        }
        encoding = eType;
    }

    /**
     * Move constructor from an encoded vector.
     * @param _shares The encoded vector whose contents will be moved to the new column.
     * @param col_name new name for the column
     */
    explicit SharedColumn(std::unique_ptr<EncodedVector>&& _shares,
                          const std::string& col_name = "") {
        auto v = _shares.release();
        encoding = v->encoding;
        contents = std::unique_ptr<EncodedVector>(v);
        name = col_name;
    }

    /**
     * Move constructor from an encoded column.
     * @param other The encoded column whose contents will be moved to the new column.
     */
    SharedColumn(std::unique_ptr<EncodedColumn>&& other) {
        auto& v = *other.get();
        encoding = v.encoding;
        contents = v.contents;
    }

    /**
     * Move assignment operator (shallow move) from an encoded column.
     * @param other The encoded column whose contents will be moved to the new column.
     */
    SharedColumn& operator=(std::unique_ptr<EncodedColumn>&& other) {
        auto& c = *other.get();
        this->encoding = c.encoding;
        this->contents.reset(c.contents.release());
        return *this;
    }

    /**
     * @brief Zero out this column
     *
     */
    void zero() {
        auto v = (SVector*)contents.get();
        v->zero();
    }

    /**
     * @brief
     *
     */
    virtual ~SharedColumn() {};

    /**
     * @return The column's size in number of elements.
     */
    virtual size_t size() const { return this->contents.get()->size(); }

    /**
     * @brief Make a deep copy of this column. Deep-copies the underlying vector
     *
     * @return std::unique_ptr<EncodedColumn>
     */
    std::unique_ptr<EncodedColumn> deepcopy() {
        auto v = static_cast<SVector*>(contents.get());
        auto s = std::make_unique<SharedColumn>(v->deepcopy());
        s->encoding = encoding;
        return s;
    }

    /**
     * @brief Elementwise less-than-zero of a BShared column. Will cause an assertion error if
     * called on an AShared column
     *
     * @return std::unique_ptr<EncodedColumn>
     */
    std::unique_ptr<EncodedColumn> ltz() {
        assert(encoding == Encoding::BShared);
        auto v = static_cast<const BSharedVector<Share, EVector>*>(contents.get());
        return std::make_unique<SharedColumn>(v->ltz());
    }

    // **************************************** //
    //   Either-type operators                  //
    // **************************************** //

    /**
     * @brief Boolean adder or Arithmetic add/subtract
     *
     */
    binary_op_either_type(+);
    binary_op_either_type(-);

    /**
     * @brief Boolean adder or Arithmetic add/subtract with constant
     *
     */
    binary_op_element_either_type(+);
    binary_op_element_either_type(-);

    /**
     * @brief Private division between two columns. The division algorithm only supports BShared
     * inputs, so automatically convert (by calling `a2b`) if any input is AShared. Output will
     * always be BShared.
     *
     * @param other
     * @return std::unique_ptr<EncodedColumn> encoded as a BShared column
     */
    std::unique_ptr<EncodedColumn> operator/(const EncodedColumn& other) const {
        std::unique_ptr<SharedColumn> s;

        // Temporary unique pointers to hold ownership of converted values
        // Without, pointers will go out of scope once we leave if-block.
        // However, can't use unique for non-converted case because there we
        // do not own the inputs to division.
        std::unique_ptr<BSharedVector<Share, EVector>> t1;
        std::unique_ptr<BSharedVector<Share, EVector>> t2;

        const BSharedVector<Share, EVector>* v1;
        const BSharedVector<Share, EVector>* v2;

        if (encoding == AShared) {
            t1 = static_cast<const ASharedVector<Share, EVector>*>(contents.get())->a2b();
            v1 = t1.get();
        } else {
            v1 = static_cast<const BSharedVector<Share, EVector>*>(contents.get());
        }

        if (other.encoding == AShared) {
            t2 = static_cast<const ASharedVector<Share, EVector>*>(other.contents.get())->a2b();
            v2 = t2.get();
        } else {
            v2 = static_cast<const BSharedVector<Share, EVector>*>(other.contents.get());
        }
        s = std::make_unique<SharedColumn>(*v1 / *v2);
        s->encoding = BShared;
        return s;
    }

    // **************************************** //
    //   Arithmetic operators                   //
    // **************************************** //

    /**
     * @brief Column * Column
     *
     */
    binary_op_downcast(*, AShared, ASharedVector);

    /**
     * @brief Column * Constant
     *
     */
    binary_op_element(*, AShared, ASharedVector);

    /**
     * @brief Unary negation
     *
     */
    unary_op_downcast(-, AShared, ASharedVector);

    /**
     * @brief Public division with constant
     *
     */
    binary_op_fixed_element(/, AShared, ASharedVector);

    /**
     * @brief Binary assignment operators over AShared columns
     *
     */
    binary_assignment_downcast(+=, AShared, ASharedVector);
    binary_assignment_downcast(-=, AShared, ASharedVector);
    binary_assignment_downcast(*=, AShared, ASharedVector);
    binary_assignment_ptr(+=, AShared, ASharedVector);
    binary_assignment_ptr(-=, AShared, ASharedVector);
    binary_assignment_ptr(*=, AShared, ASharedVector);

    // **************************************** //
    //   Boolean operators                      //
    // **************************************** //

    /**
     * @brief Binary operators between BShared columns
     *
     */
    binary_op_downcast(&, BShared, BSharedVector);
    binary_op_downcast(|, BShared, BSharedVector);
    binary_op_downcast(^, BShared, BSharedVector);

    /**
     * @brief Assignment operators between BShared columns
     *
     */
    binary_assignment_downcast(&=, BShared, BSharedVector);
    binary_assignment_downcast(|=, BShared, BSharedVector);
    binary_assignment_downcast(^=, BShared, BSharedVector);
    binary_assignment_ptr(&=, BShared, BSharedVector);
    binary_assignment_ptr(|=, BShared, BSharedVector);
    binary_assignment_ptr(^=, BShared, BSharedVector);

    /**
     * @brief Binary operators with an element
     *
     */
    binary_op_element(&, BShared, BSharedVector);
    binary_op_element(^, BShared, BSharedVector);
    binary_op_element(|, BShared, BSharedVector);

    /**
     * @brief Unary operators on BShared columns
     *
     */
    unary_op_downcast(~, BShared, BSharedVector);
    unary_op_downcast(!, BShared, BSharedVector);

    /**
     * @brief Shift operators on BShared columns
     *
     */
    binary_op_fixed_element(>>, BShared, BSharedVector);
    binary_op_fixed_element(<<, BShared, BSharedVector);

    // **************************************** //
    //   Comparison operators                   //
    // **************************************** //

    /**
     * @brief Comparison operators between two columns
     *
     */
    binary_op_downcast(==, BShared, BSharedVector);
    binary_op_downcast(!=, BShared, BSharedVector);
    binary_op_downcast(>, BShared, BSharedVector);
    binary_op_downcast(<, BShared, BSharedVector);
    binary_op_downcast(>=, BShared, BSharedVector);
    binary_op_downcast(<=, BShared, BSharedVector);

    /**
     * @brief Comparison operators between a column and a single element
     *
     */
    binary_op_element(==, BShared, BSharedVector);
    binary_op_element(!=, BShared, BSharedVector);
    binary_op_element(<, BShared, BSharedVector);
    binary_op_element(<=, BShared, BSharedVector);
    binary_op_element(>, BShared, BSharedVector);
    binary_op_element(>=, BShared, BSharedVector);
};

/**
 * @brief Operations with pointers
 *
 */
binary_op_ptr_element(+);
binary_op_ptr_element(/);
binary_op_ptr_element(>>);
binary_op_ptr_element(<<);

binary_op_ptr_element(<);
binary_op_ptr_element(<=);
binary_op_ptr_element(>);
binary_op_ptr_element(>=);
binary_op_ptr_element(==);
binary_op_ptr_element(!=);
}  // namespace orq::relational
