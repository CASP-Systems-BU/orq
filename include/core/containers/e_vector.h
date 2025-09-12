#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include "encoding.h"
#include "vector.h"  // ORQ's wrapper for std::vector

#define define_apply_return_to_replicated(_func_)       \
    ;                                                   \
    template <typename... T>                            \
    EVector _func_(T... args) const {                   \
        std::vector<Vector<Share>> res;                 \
        for (int i = 0; i < ReplicationNumber; ++i) {   \
            res.push_back(contents[i]._func_(args...)); \
        }                                               \
        EVector result(res, this->precision);           \
        return result;                                  \
    }

#define define_apply_to_replicated(_func_)            \
    template <typename... T>                          \
    void _func_(T... args) {                          \
        for (int i = 0; i < ReplicationNumber; ++i) { \
            contents[i]._func_(args...);              \
        }                                             \
    }

#define define_apply_input_to_replicated(_func_)            \
    template <typename... T>                                \
    void _func_(EVector &other, T... args) {                \
        for (int i = 0; i < ReplicationNumber; ++i) {       \
            contents[i]._func_(other.contents[i], args...); \
        }                                                   \
    }

#define define_apply_const_ref_input_to_replicated(_func_)  \
    template <typename... T>                                \
    void _func_(const EVector &other, const T &...args) {   \
        for (int i = 0; i < ReplicationNumber; ++i) {       \
            contents[i]._func_(other.contents[i], args...); \
        }                                                   \
    }

#define define_apply_input_to_replicated_const(_func_)      \
    template <typename... T>                                \
    void _func_(EVector &other, T... args) const {          \
        for (int i = 0; i < ReplicationNumber; ++i) {       \
            contents[i]._func_(other.contents[i], args...); \
        }                                                   \
    }

#define define_apply_input_return_to_replicated(_func_)                    \
    template <typename... T>                                               \
    EVector _func_(const EVector &other, T... args) const {                \
        std::vector<Vector<Share>> res;                                    \
        for (int i = 0; i < ReplicationNumber; ++i) {                      \
            res.push_back(contents[i]._func_(other.contents[i], args...)); \
        }                                                                  \
        EVector result(res, this->precision);                              \
        return result;                                                     \
    }

#define define_binary_evector_element_op(_op_)                   \
    ;                                                            \
    template <typename OtherType>                                \
    inline EVector operator _op_(const OtherType &other) const { \
        std::vector<Vector<Share>> res;                          \
        for (int i = 0; i < ReplicationNumber; ++i) {            \
            res.push_back(contents[i] _op_ other);               \
        }                                                        \
        EVector result(res, this->precision);                    \
        return result;                                           \
    }

#define define_binary_evector_evector_op(_op_)                 \
    ;                                                          \
    inline EVector operator _op_(const EVector &other) const { \
        std::vector<Vector<Share>> res;                        \
        for (int i = 0; i < ReplicationNumber; ++i) {          \
            res.push_back(contents[i] _op_ other.contents[i]); \
        }                                                      \
        EVector result(res, this->precision);                  \
        return result;                                         \
    }

#define define_unary_evector_op(_op_)                 \
    inline EVector operator _op_() const {            \
        std::vector<Vector<Share>> res;               \
        for (int i = 0; i < ReplicationNumber; ++i) { \
            res.push_back(_op_ contents[i]);          \
        }                                             \
        EVector result(res, this->precision);         \
        return result;                                \
    }

#define define_binary_evector_assignment_op(_op_)          \
    inline EVector &operator _op_(const EVector & other) { \
        for (int i = 0; i < ReplicationNumber; i++) {      \
            contents[i] _op_ other.contents[i];            \
        }                                                  \
        return *this;                                      \
    }

namespace orq {
// Forward declarations
namespace service {
    class RunTime;
    template <typename InputType, typename ReturnType, typename ObjectType>
    class Task_1;
    template <typename InputType, typename ReturnType, typename ObjectType>
    class Task_2;
}  // namespace service

/**
 * @tparam Share Share type.
 * @tparam ReplicationNumber The number of shares that each party sees for each secret value.
 *
 * EVector is an abstraction similar to the EncodedVector, i.e., an "encoded view" of a plaintext
 * vector as seen by an untrusted party. In contrast to EncodedVector, EVector provides access to
 * the underlying encodings of the secret values.
 *
 * While EncodedVector targets end-users, EVector is the abstraction provided to Protocol developers
 * who need access to the underlying encodings in order to define new secure primitives, such as
 * Protocol::add_a(), Protocol::and_b(), etc.
 */
template <typename Share, int ReplicationNumber>
class EVector {
    // The fixed point precision
    size_t precision;

   protected:
    // The EVector contents
    std::vector<Vector<Share>> contents;

    /**
     * Returns the size of EVector, i.e., the total number of secret values in the original
     * (plaintext) vector.
     * @return The size of EVector.
     *
     * NOTE: The output of total_size() cannot be less than the output of size().
     */
    inline size_t total_size() const { return contents[0].total_size(); }

   public:
    using ShareT = Share;

    static const int replicationNumber = ReplicationNumber;

    /**
     * Provides an interface to initialize the data object with existing data.
     * @param contents The vector of vectors to initialize the data object with.
     *
     * NOTE: the copy constructor of Vector<T> is shallow; if this is initialized by another
     * Vector<T> object, both will point to data in the same memory location.
     */
    EVector(std::vector<Vector<Share>> contents)
        : contents(contents), precision(contents[0].getPrecision()) {}

    /**
     * Initializes the EVector with a given vector of vectors and a fixed-point precision.
     * @param contents - The vector of vectors to initialize the data object with.
     * @param fixed_point_precision - The fixed-point fractional bits to set for this EVector.
     */
    EVector(std::vector<Vector<Share>> contents, size_t fixed_point_precision)
        : contents(std::move(contents)), precision(fixed_point_precision) {}

    /**
     * Initializes the EVector with a given initializer list of Vector<Share>.
     * @param l The initializer list to initialize the EVector with.
     * @throws std::invalid_argument if the size of the initializer list doesn't match
     *  ReplicationNumber.
     */
    EVector(std::initializer_list<Vector<Share>> l) {
        if (l.size() != ReplicationNumber) {
            throw std::invalid_argument("Initializer list size must match ReplicationNumber");
        }
        contents.assign(l.begin(), l.end());
        precision = contents[0].getPrecision();
    }

    /**
     *
     * EVector<T,N> constructor that allocates `ReplicationNumber` new Vectors, each one of size
     * `size`.
     * @param size The size of Vector<T> in this EVector<T,N>.
     */
    EVector(const size_t &size) : precision(0) {
        for (int i = 0; i < ReplicationNumber; ++i) {
            // NOTE: it is important to use the push_back and
            // not pass the size in the constructor. Otherwise,
            // std::vector uses the copy constructor and all
            // `Vector`s will point to the same memory location.
            contents.push_back(Vector<Share>(size));
        }
    }

    /**
     * This is a shallow copy constructor (i.e., only copies the vector pointer but not its
     * contents).
     * @param other The other EVector whose contents this vector will point to.
     *
     * WARNING: The new vector will point to the same memory location as used by `other`. To copy
     * the data into a separate memory location, create a new vector first then use the assignment
     * operator.
     */
    EVector(const EVector &other) : contents(other.contents), precision(other.getPrecision()) {}

    /**
     * This is a deep move assignment.
     * Applies the move assignment operator to Vector<T>. Assigns the values of the current batch
     * from the `other` vector to the current batch of this vector. Assumes `other` has the same
     * size and replication factor as this vector.
     * @param other The EVector containing the values that this vector must point to.
     * @return A reference to this EVector after modification.
     */
    EVector &operator=(const EVector &&other) {
        for (int i = 0; i < ReplicationNumber; ++i) {
            contents[i] = other.contents[i];
        }
        precision = other.getPrecision();
        return *this;
    }

    /**
     * Sets the fixed-point precision.
     * @param fixed_point_precision - the number of fixed-point fractional bits.
     */
    void setPrecision(const int fixed_point_precision) { precision = fixed_point_precision; }

    /**
     * Gets the fixed-point precision.
     */
    size_t getPrecision() const { return precision; }

    /**
     * Helper that sets this vector's precision to match another EVector.
     * @param other The EVector whose precision should be copied.
     */
    void matchPrecision(const EVector &other) { precision = other.getPrecision(); }

    /*
     * Writes the secret shares of the EVector to a file.
     * @param _output_file_path - The path to the file to write the secret shares to.
     *  Given file is overwritten if it already exists.
     */
    void output(const std::string &_output_file_path) {
        // Open file for writing
        std::ofstream output_file(_output_file_path, std::ios::out | std::ios::trunc);
        if (!output_file.is_open()) {
            std::cerr << "Error: could not open output file " << _output_file_path << std::endl;
            exit(1);
        }

        // Write the output file
        for (size_t i = 0; i < contents[0].size(); ++i) {
            for (int j = 0; j < ReplicationNumber; ++j) {
                output_file << contents[j][i];
                if (j < ReplicationNumber - 1) {
                    output_file << ",";
                }
            }
            output_file << std::endl;
        }
        output_file.close();
    }

    /**
     * This is a deep copy assignment.
     * Applies the copy assignment operator to Vector<T>. Copies the values of the current batch
     * from the `other` vector to the current batch of this vector. Assumes `other` has the same
     * size and replication factor as this vector.
     * @param other The EVector that contains the values to be copied.
     * @return A reference to this EVector after modification.
     */
    EVector &operator=(const EVector &other) {
        for (int i = 0; i < ReplicationNumber; ++i) {
            contents[i] = other.contents[i];
        }
        precision = other.getPrecision();
        return *this;
    }

    /**
     * @brief Type-conversion assignment
     *
     * @tparam Other type of the other EVector
     * @tparam R replication number
     * @param other
     * @return EVector&
     */
    template <typename Other, int R>
    EVector &operator=(const orq::EVector<Other, R> &other) {
        for (int i = 0; i < ReplicationNumber; ++i) {
            contents[i] = other.contents[i];
        }
        precision = other.getPrecision();
        return *this;
    }

    /**
     * EVector<T,N> constructor that allocates `ReplicationNumber` new Vectors, each one of size
     * `size`, and initializes them with the contents of the file `_input_file_path`. The file
     * contains the secret shares of the EVector. Each line in the file contains `ReplicationNumber`
     * values separated by a space.
     * @param size The size of Vector<T> in this EVector<T,N>.
     * @param _input_file_path The path to the file containing the contents of the EVector.
     */
    EVector(const size_t &size, const std::string &_input_file_path) : precision(0) {
        // First allocate the memory
        for (int i = 0; i < ReplicationNumber; ++i) {
            // NOTE: it is important to use the push_back and
            // not pass the size in the constructor. Otherwise,
            // std::vector uses the copy constructor and all
            // `Vector`s will point to the same memory location.
            contents.push_back(Vector<Share>(size));
        }

        // Open file for reading
        std::ifstream input_file(_input_file_path, std::ios::in);
        if (!input_file.is_open()) {
            std::cerr << "Error: could not open input file " << _input_file_path << std::endl;
            exit(1);
        }

        // Read the input file
        std::string line;
        int i = 0;
        while (std::getline(input_file, line) && i < size) {
            // replace ',' with ' ' to allow for both csv and space separated files
            std::replace(line.begin(), line.end(), ',', ' ');

            // ignore first line if it is alpahnumeric
            if (line.find_first_not_of(" -0123456789") != std::string::npos) {
                continue;
            }

            // each line has ReplicationNumber values
            std::istringstream iss(line);
            for (int j = 0; j < ReplicationNumber; ++j) {
                iss >> contents[j][i];
            }
            ++i;
        }
        input_file.close();
    }

    /**
     * Returns the current batch size, i.e., the number of secret values that are being processed in
     * the current round. The default batch size is the actual vector size.
     * @return The current batch size.
     *
     * NOTE: ORQ parties apply operations on EVectors in batches. Each batch corresponds to a
     * range of elements in the vector.
     */
    inline size_t size() const { return contents[0].size(); }

    inline int getReplicationNumber() const { return ReplicationNumber; }

    /**
     * Creates a new EVector with the same structure as this EVector,
     * but with newly allocated empty vectors of the same size.
     * @return A new EVector with the same structure but empty contents.
     */
    EVector construct_like() const {
        EVector result(this->size());
        return result;
    }

    /* Replicated function calls.
     *
     * These macros call down to Vector operations, definining an EVector
     * operation which trivially applies the corresponding Vector operation
     * to its component replicated Vectors.
     */

    // Functions which take a constant-reference EVector as input
    define_apply_const_ref_input_to_replicated(pack_from);
    define_apply_const_ref_input_to_replicated(unpack_from);

    // Functions which take an EVector as input
    define_apply_input_to_replicated(alternating_bit_decompress);
    define_apply_input_to_replicated(simple_bit_decompress);

    // Functions which return an EVector
    define_apply_return_to_replicated(alternating_bit_compress);
    define_apply_return_to_replicated(alternating_subset_reference);
    define_apply_return_to_replicated(bit_arithmetic_right_shift);
    define_apply_return_to_replicated(bit_left_shift);
    define_apply_return_to_replicated(bit_logical_right_shift);
    define_apply_return_to_replicated(bit_xor);
    define_apply_return_to_replicated(cyclic_subset_reference);
    define_apply_return_to_replicated(directed_subset_reference);
    define_apply_return_to_replicated(extend_lsb);
    define_apply_return_to_replicated(ltz);
    define_apply_return_to_replicated(repeated_subset_reference);
    define_apply_return_to_replicated(reversed_alternating_subset_reference);
    define_apply_return_to_replicated(simple_bit_compress);
    define_apply_return_to_replicated(simple_subset_reference);
    define_apply_return_to_replicated(slice);
    define_apply_return_to_replicated(simple_subset);
    define_apply_return_to_replicated(included_reference);
    define_apply_return_to_replicated(mapping_reference);
    define_apply_return_to_replicated(materialize);
    define_apply_return_to_replicated(chunkedSum);

    // Functions which take an EVector as input and returns an EVector
    define_apply_input_return_to_replicated(dot_product);

    // Functions which operate on this vector
    define_apply_to_replicated(mask);
    define_apply_to_replicated(prefix_sum);
    define_apply_to_replicated(resize);
    define_apply_to_replicated(set_batch);
    define_apply_to_replicated(reset_batch);
    define_apply_to_replicated(set_bits);
    define_apply_to_replicated(tail);
    define_apply_to_replicated(zero);
    define_apply_to_replicated(apply_mapping);
    define_apply_to_replicated(reverse);
    define_apply_to_replicated(materialize_inplace);

    bool has_mapping() const { return contents[0].has_mapping(); }

    /**
     * Returns a mutable reference to a column of the EVector.
     * @param column The column index.
     * @return A mutable reference to a vector of encodings at the given index.
     *
     * EVector is implemented as a column-first 2D vector, where:
     *
     * - row *i* corresponds to *k* >= 1 encodings of the *i*-th secret value, 0 <= *i* < *n*.
     * - column *j* stores the *j*-th encoding, 0 <= *j* < *k*, of each secret value in the vector.
     *
     * The above column-first representation aims to facilitate the implementation of vectorized
     * secure primitives. The number of secret values *n* in EVector is defined by the user but the
     * number of encodings *k* per secret value depends on the Protocol. For example, in
     * Replicated_3PC, each secret value \f$s\f$ is encoded with three shares \f$s_1\f$, \f$s_2\f$,
     * and \f$s_3\f$ and each party receives *k*=2 of the 3 shares, namely:
     *
     * - Party 0 receives shares \f$s_1\f$ and \f$s_2\f$.
     * - Party 1 receives shares \f$s_2\f$ and \f$s_3\f$.
     * - Party 2 receives shares \f$s_3\f$ and \f$s_1\f$.
     *
     * Assuming a plaintext vector \f$v = \{s^1, s^2, ..., s^{n}\}\f$ with *n* values that
     * are secret-shared by three parties, the respective EVectors in Replicated_3PC look as shown
     * below:
     *
     * - \f$v = \{~\{s^1_1, s^2_1,~...~s^{n}_1\},~\{s^1_2, s^2_2,~...,~s^{n}_2\}~\}\f$     (Party 1)
     * - \f$v = \{~\{s^1_2, s^2_2,~...~s^{n}_2\},~\{s^1_3, s^2_3,~...,~s^{n}_3\}~\}\f$     (Party 2)
     * - \f$v = \{~\{s^1_3, s^2_3,~...~s^{n}_3\},~\{s^1_1, s^2_1,~...,~s^{n}_1\}~\}\f$     (Party 3)
     *
     * In this case, calling \f$v(1)\f$ from Party 2 will return a reference to the internal vector
     * at index *j*=1 that includes the third share \f$s_3\f$ of each secret value:
     *
     * - \f$\{s^1_3, s^2_3,~...,~s^{n}_3\}\f$
     */
    inline Vector<Share> &operator()(const int &column) { return contents[column]; }

    /**
     * Returns a read-only reference to a column of the EVector. For more information, see the
     * version of the same operator that returns a mutable reference.
     *
     * @param column The column index.
     * @return A read-only reference to a vector of encodings at the given index.
     *
     */
    inline const Vector<Share> &operator()(const int &column) const { return contents[column]; }

    define_binary_evector_element_op(+);
    define_binary_evector_element_op(-);
    define_binary_evector_element_op(*);
    define_binary_evector_element_op(/);

    define_binary_evector_element_op(&);
    define_binary_evector_element_op(|);
    define_binary_evector_element_op(^);

    define_binary_evector_element_op(>>);
    define_binary_evector_element_op(<<);

    define_binary_evector_element_op(<);
    define_binary_evector_element_op(>);
    define_binary_evector_element_op(==);
    define_binary_evector_element_op(!=);

    define_binary_evector_evector_op(+);
    define_binary_evector_evector_op(-);
    define_binary_evector_evector_op(*);
    define_binary_evector_evector_op(/);

    define_binary_evector_evector_op(&);
    define_binary_evector_evector_op(|);
    define_binary_evector_evector_op(^);

    define_binary_evector_evector_op(>>);
    define_binary_evector_evector_op(<<);

    define_binary_evector_evector_op(<);
    define_binary_evector_evector_op(>);
    define_binary_evector_evector_op(==);
    define_binary_evector_evector_op(!=);

    define_binary_evector_assignment_op(+=);
    define_binary_evector_assignment_op(-=);
    define_binary_evector_assignment_op(&=);
    define_binary_evector_assignment_op(|=);
    define_binary_evector_assignment_op(^=);

    define_unary_evector_op(-);
    define_unary_evector_op(~);
    define_unary_evector_op(!);

    // Friend classes
    template <typename ShareType, typename EVector>
    friend class BSharedVector;
    template <typename ShareType, typename EVector>
    friend class ASharedVector;
    friend class service::RunTime;
    template <typename InputType, typename ReturnType, typename ObjectType>
    friend class orq::service::Task_1;
    template <typename InputType, typename ReturnType, typename ObjectType>
    friend class orq::service::Task_2;

    template <typename OtherShare, int R>
    friend class EVector;

    // Allow `reshare` to directly access EVector contents for recv
    template <typename D, typename S, typename V, typename E>
    friend class Protocol;
};

}  // namespace orq
