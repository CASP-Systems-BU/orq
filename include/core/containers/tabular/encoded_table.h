#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>

#include "backend/common/runtime.h"
#include "core/operators/aggregation_selector.h"
#include "core/operators/distinct.h"
#include "core/operators/merge.h"
#include "core/operators/sorting.h"
#include "core/operators/streaming.h"
#include "encoded_column.h"
#include "profiling/stopwatch.h"
#include "profiling/utils.h"
#include "shared_column.h"

/**
 * @brief Internal name for the Table ID column during joins
 *
 */
#define ENC_TABLE_JOIN_ID "[##TID]"

/**
 * @brief Internal name for the Valid column. All tables have a hidden Valid column.
 *
 */
#define ENC_TABLE_VALID "[##V]"

/**
 * @brief Internal name for the Unique column during joins
 *
 */
#define ENC_TABLE_UNIQ "[##UNIQ]"
#define RESERVED_COLUMNS {ENC_TABLE_JOIN_ID, ENC_TABLE_VALID, ENC_TABLE_UNIQ}

#ifdef INSTRUMENT_TABLES
#define PRINT_TABLE_INSTRUMENT(...) single_cout(__VA_ARGS__)
#define BEGIN_TABLE_PROFILING()                                                                    \
    stopwatch::profile_comm("other", thread_stopwatch::get_aggregate_comm(runTime->getPartyID())); \
    stopwatch::profile_timepoint("other");

#define END_TABLE_PROFILING(LABEL)                \
    stopwatch::profile_timepoint("table." LABEL); \
    stopwatch::profile_comm("table." LABEL,       \
                            thread_stopwatch::get_aggregate_comm(runTime->getPartyID()));

#else
#define PRINT_TABLE_INSTRUMENT(...)
#define BEGIN_TABLE_PROFILING()
#define END_TABLE_PROFILING(...)
#endif

// #define STOPWATCH_JOIN

#ifdef STOPWATCH_JOIN
#define STOPWATCH(x) stopwatch::timepoint("[J] " x)
#else
#define STOPWATCH(x)
#endif

using namespace orq::debug;
using namespace orq::service;
using namespace orq::benchmarking::utils;
using namespace orq::operators;
using namespace orq::aggregators;
using namespace orq::benchmarking;

namespace orq::relational {

/**
 * @brief Options struct for join. Construct an instance and pass it in,
 * or use inline struct initializer syntax.
 *
 * Available fields:
 *
 * - `left_outer`: if this is a left outer join.
 * - `right_outer`: if this is a right outer join.
 * - `anti`: if this is an anti join.
 * - `trim_invalid`: whether to trim invalid rows from the output (bounded
 *    by the size of the right table)
 *
 * The join options generalize the other kinds of joins: inner joins are
 * neither left outer nor right outer; full outer joins are both left
 * outer and right outer. The default is an inner join; i.e., both are
 * `false`.
 */
struct JoinOptions {
    bool left_outer = false;
    bool right_outer = false;

    bool anti = false;

    bool trim_invalid = true;
};

/**
 * @brief Options struct for aggregation.
 *
 * Available fields:
 *
 * - `reverse`: whether this is a forward or reverse aggregation.
 * (default `false`)
 * - `do_sort`: whether to sort first. (default `true`)
 * - `mark_valid`: whether to mark valid rows post-aggregation. (default
 * `true`)
 * - `table_id`: an optional string pointing to the name of the table ID
 * column.
 *
 */
struct AggregationOptions {
    bool reverse = false;
    bool do_sort = true;
    bool mark_valid = true;
    std::optional<std::string> table_id = {};
};

// TODO (john): Support columns with different share types.
/**
 * An EncodedTable is a relational table that contains encoded data organized in columns.
 ORQ users can perform
 * vectorized secure operations directly on table columns via a Pandas-like interface, as shown
 below:
 *
 \verbatim
 // ORQ program (executed by an untrusted party)
 t["col3"] = t["col1"] * t["col2"];
 \endverbatim
 *
 * The above line performs a secure elementwise multiplication between two columns `col1` and
 `col2` in table `t`
 * and stores the result in a third column `col3`.
 *
 * @tparam SharedColumn Secret-shared column type.
 * @tparam A A-shared vector type.
 * @tparam B B-shared vector type.
 * @tparam EncodedVector Encoded vector type.
 * @tparam DataTable Plaintext table type.
 */
template <typename Share, typename SharedColumn, typename A, typename B, typename EncodedVector,
          typename DataTable>
class EncodedTable {
    // The table schema, i.e., a mapping from column names to column pointers.
    std::map<std::string, std::shared_ptr<EncodedColumn>> schema;

    // The number of rows in the table
    size_t rows;

    Share MASK_VALUE = std::numeric_limits<Share>::max();

    using LabeledDataTable = std::pair<DataTable, std::vector<std::string>>;

    using AggregationSpec =
        std::vector<std::tuple<std::string, std::string, AggregationSelector<A, B>>>;

    using uniqA = std::unique_ptr<A>;
    using uniqB = std::unique_ptr<B>;

    bool deleted = false;

    /**
     * @brief Mask the contents of multiple columns using the type's mask
     * value (usually the maximum value). This is useful for obliviously
     * hiding rows that are not selected for output by some prior process.
     *
     * Rows which have a `1` in the mask column will remain untouched.
     * Rows which have a `0` in the mask column will be set to MASK_VALUE.
     *
     * Will NOT mask the mask-column itself.
     *
     * @param mask_column_name the column to use as the mask
     * @param keys which other columns to mask
     * @return EncodedTable&
     */
    EncodedTable &mask(const std::string &mask_column_name, const std::vector<std::string> &keys) {
        B mask_col_b = *(B *)(*this)[mask_column_name].contents.get();
        auto _size = mask_col_b.size();

        A mask_col_a = mask_col_b.b2a_bit();

        // TODO: why isn't repeated_subset_reference working here?
        Vector<Share> _mask_vec(1, MASK_VALUE);

        A shared_single_mask_a(1);
        B shared_single_mask_b(1);

        secret_share_vec(_mask_vec, shared_single_mask_a);
        secret_share_vec(_mask_vec, shared_single_mask_b);

        A full_mask_a(_size);
        full_mask_a = shared_single_mask_a.repeated_subset_reference(_size);

        B full_mask_b(_size);
        full_mask_b = shared_single_mask_b.repeated_subset_reference(_size);

        for (auto k : keys) {
            if (k == mask_column_name) {
                continue;
            }

            auto c = (*this)[k].contents.get();
            if (isBShared(k)) {
                *(B *)c = multiplex(mask_col_b, full_mask_b, *(B *)c);
            } else {
                *(A *)c = multiplex(mask_col_a, full_mask_a, *(A *)c);
            }
        }
        return *this;
    }

    /**
     * @brief Mask all columns (except the mask itself)
     *
     * @param mask_column_name
     * @return EncodedTable&
     */
    EncodedTable &mask(const std::string &mask_column_name) {
        return mask(mask_column_name, getColumnNames());
    }

    /**
     * @brief Internal join implementation.
     */
    EncodedTable _join(EncodedTable &right, std::vector<std::string> keys, AggregationSpec agg_spec,
                       JoinOptions opt);

   public:
    /**
     * @brief Name of this table. Used for output and organization.
     *
     */
    std::string tableName;

    /**
     * Constructs a table and initializes it with zeros.
     *
     * @param _tableName The name of the table.
     * @param _columns The column names.
     * @param _rows The number of rows to allocate.
     */
    EncodedTable(const std::string &_tableName, const std::vector<std::string> &_columns,
                 const int &_rows)
        : tableName(_tableName), rows(_rows) {
        addColumns(_columns, _rows);  // Allocate columns
        configureValid();
    }

    /**
     * Constructs a table from encoded columns.
     * @param contents The table columns.
     */
    EncodedTable(std::vector<std::shared_ptr<EncodedColumn>> &&contents) {
        // Set table cardinality and metadata
        assert(contents.size() > 0);
        rows = contents[0]->size();
        schema.clear();
        for (auto &c : contents) {
            assert(rows == c->size());
            auto p = std::pair<std::string, std::shared_ptr<EncodedColumn>>(c->name, c);
            auto r = schema.insert(p);
            assert(r.second);
        }
        configureValid();
    }

    /**
     * @brief Configured the valid column. If it does not exist, create it. If it does exist,
     * re-initialize and make all rows valid.
     *
     */
    void configureValid() {
        // Can't find VALID column. create it.
        if (auto c = schema.find(ENC_TABLE_VALID); c == schema.end()) {
            addColumn(ENC_TABLE_VALID, rows);
        }

        // plaintext vector of size 1 containing the value 1
        Vector<Share> one_(size(), 1);
        B one = runTime->public_share<SharedColumn::replicationNumber>(one_);
        auto vv = *getValidVector();
        vv = one;
    }

    /**
     * @brief Update the valid column based on the value of a provided
     * column or expression (i.e., bitwise-and the input with valid)
     *
     * This accepts either a column, or a unique pointer to a column.
     *
     * @tparam T
     * @param e the expression to filter on. Rows evaluating to a
     * boolean `true` will remain valid; those evaluating to a boolean
     * `false` will be marked invalid.
     */
    template <typename T>
    void filter(T &&e) {
        (*this)[ENC_TABLE_VALID] &= std::forward<T>(e);
    }

    /**
     * @brief Update the valid column based on the value of an expression,
     * passed as a unique pointer. Compound operators are only supported on
     * columns, not pointers to columns, so dereference.
     *
     * @tparam T
     * @param e
     */
    template <typename T>
    void filter(std::unique_ptr<T> e) {
        (*this)[ENC_TABLE_VALID] &= std::forward<T>(*e);
    }

    /**
     * @brief Get the valid vector
     *
     * @return BSharedVector *
     */
    B *getValidVector() { return (B *)(*this)[ENC_TABLE_VALID].contents.get(); }

    /**
     * Returns a mutable reference to the column with the given name.
     * @param name The name of the column.
     * @return A reference to the column (throws an error if the column is not found).
     */
    inline EncodedColumn &operator[](const std::string &name) {
        if (deleted) {
            std::cerr << "ERROR: trying to access deleted table\n";
            abort();
        }

        if (auto c = schema.find(name); c != schema.end()) {
            return *((EncodedColumn *)c->second.get());
        } else {
            std::cerr << "ERROR: column '" << name << "' not found\n";
            abort();
        }
    }

    /**
     * @brief Get column `name` as a BSharedVector.
     *
     * @param name
     * @return B
     */
    B asBSharedVector(const std::string &name) {
        assert((*this)[name].encoding == Encoding::BShared);
        return *(B *)(*this)[name].contents.get();
    }

    /**
     * @brief Get column `name` as an ASharedVector.
     *
     * @param name
     * @return A
     */
    A asASharedVector(const std::string &name) {
        assert((*this)[name].encoding == Encoding::AShared);
        return *(A *)(*this)[name].contents.get();
    }

    /**
     * @brief Get column `name` as an untyped SharedVector
     *
     * @param name
     * @return B::SharedVector_t
     */
    B::SharedVector_t asSharedVector(const std::string &name) {
        return *static_cast<typename B::SharedVector_t *>((*this)[name].contents.get());
    }

    /**
     * Reads secret shares for a column from a file. The file contain one
     * line for each element in the column. Each line contains the replicated
     * secret shares separated by a space.
     * Note: this function does not affect valid bit.
     * @param columnName The name of the column.
     * @param inputFile The file containing the secret shares.
     */
    inline void inputSecretShares(const std::string &columnName, const std::string &inputFile) {
        if ((*this)[columnName].encoding == Encoding::BShared) {
            *(B *)((*this)[columnName].contents.get()) = B(rows, inputFile);
        } else {
            *(A *)((*this)[columnName].contents.get()) = A(rows, inputFile);
        }
    }

    /**
     * Reads secret shares for a column from a file. The file contains a csv table with the
     * data.
     * @param _file_path The file containing the table data.
     * @param _input_party The party that has the file.
     */
    inline void inputCSVTableData(const std::string &_file_path, const int &_input_party) {
        // get the table size
        int current_rows = this->size();

        // All parties gerenate initial data vectors for all columns
        std::vector<std::string> available_column_names;
        std::vector<Vector<Share>> read_column_data;
        for (auto const &imap : schema) {
            available_column_names.push_back(imap.first);
            read_column_data.push_back(Vector<Share>(current_rows, 0));
        }

        if (service::runTime->getPartyID() == _input_party) {
            std::vector<int> found_column_index(available_column_names.size(), -1);

            std::string line;
            std::ifstream file(_file_path);
            if (file.is_open()) {
                // Header line
                std::getline(file, line);
                std::stringstream ss(line);
                std::string token;
                int token_index = 0;
                while (std::getline(ss, token, ',')) {
                    auto it = std::find(available_column_names.begin(),
                                        available_column_names.end(), token);
                    if (it != available_column_names.end()) {
                        found_column_index[token_index] =
                            std::distance(available_column_names.begin(), it);
                    } else {
                        std::cerr << "Column " << token << " not found in schema!" << std::endl;
                        exit(-1);
                    }
                    token_index++;
                }

                // Data lines
                int row_index = 0;
                while (std::getline(file, line) && row_index < current_rows) {
                    std::stringstream ss(line);
                    std::string token;
                    int token_index = 0;
                    while (std::getline(ss, token, ',')) {
                        if (found_column_index[token_index] != -1) {
                            read_column_data[found_column_index[token_index]][row_index] =
                                (Share)std::stoll(token);
                        }

                        token_index++;
                    }
                    row_index++;
                }

                // Setting VALID bit to 1 if it was read
                // Check if VALID bit exists in table.
                auto valid_it = std::find(available_column_names.begin(),
                                          available_column_names.end(), ENC_TABLE_VALID);
                if (valid_it != available_column_names.end()) {
                    // VALID bit exist in table and has following index
                    int valid_index = std::distance(available_column_names.begin(), valid_it);
                    auto valid_index_it = std::find(found_column_index.begin(),
                                                    found_column_index.end(), valid_index);
                    if (valid_index_it == found_column_index.end()) {
                        // VALID bit was not read, so we need to set it to 1
                        for (int i = 0; i < row_index; ++i) {
                            read_column_data[valid_index][i] = 1;
                        }
                    }
                }

                file.close();
            } else {
                std::cerr << "Unable to open file" << std::endl;
                exit(-1);
            }
        }

        // iterate over each column, secret share it and assign it
        for (int i = 0; i < available_column_names.size(); ++i) {
            if (isBShared(available_column_names[i])) {
                secret_share_vec(read_column_data[i],
                                 *(B *)((*this)[available_column_names[i]].contents.get()),
                                 _input_party);
            } else {
                secret_share_vec(read_column_data[i],
                                 *(A *)((*this)[available_column_names[i]].contents.get()),
                                 _input_party);
            }
        }
    }

    /**
     * Reads table secret shares from a csv file. Each column is secret shared using
     * columns names as follows: [column_name]_[replication_index]. For example,
     * column "A" with replication index 0 is secret shared using column name "[A]_0" and
     * same for "[A]_1", "[A]_2", etc. depending on the replication number for the protocol.
     * Note: this function updates the valid bit so that unread rows are assigned zero.
     * @param _file_path The file containing the table secret shares.
     */
    inline void inputCSVTableSecretShares(const std::string &_file_path) {
        // First read each column in a separate vector
        std::set<std::string> column_names_set;
        std::vector<std::pair<std::string, int>> column_mapping;
        std::string line;

        std::ifstream file(_file_path);

        int row_index = 0;
        if (file.is_open()) {
            // Read the first line to get the column names
            std::getline(file, line);
            std::stringstream ss(line);
            std::string token;
            while (std::getline(ss, token, ',')) {
                // divide each name into column name and replication index
                // they are separated with a "_" and store result in column_mapping
                // the "_" delimiter is the latest "_" in the name.
                std::string column_name;
                int replication_ind = 0;
                size_t pos = token.find_last_of("_");
                if (pos != std::string::npos) {
                    column_name = token.substr(0, pos);
                    replication_ind = std::stoi(token.substr(pos + 1));
                } else {
                    column_name = token;
                }

                column_mapping.push_back(std::make_pair(column_name, replication_ind));
                column_names_set.insert(column_name);
            }

            // Read the rest of the lines to get the column values
            long long current_rows = this->size();
            while (std::getline(file, line) && row_index < current_rows) {
                std::stringstream ss(line);
                std::string token;

                int token_index = 0;
                while (std::getline(ss, token, ',') && token_index < column_mapping.size()) {
                    // TODO: Does it have to split into A/B?
                    (*((B *)((*this)[column_mapping[token_index].first].contents.get())))
                        .vector(column_mapping[token_index].second)[row_index] =
                        (Share)std::stoll(token);

                    token_index++;
                }
                row_index++;
            }
            file.close();
        } else {
            std::cerr << "Unable to open file" << std::endl;
            exit(-1);
        }

        ////////////////// CHECK TO MAKE IT SECURE SHARING //////////////////
        // check if "SEL" column is present in the table's schema but not in column_names

        if (this->schema.find(ENC_TABLE_VALID) != this->schema.end() &&
            column_names_set.find(ENC_TABLE_VALID) == column_names_set.end()) {
            Vector<Share> sel_plain(row_index, 1);
            B sel_secret =
                (*(B *)((*this)[ENC_TABLE_VALID].contents.get())).slice(0, row_index + 1);

            secret_share_vec(sel_plain, sel_secret);
        }
    }

    /**
     * Outputs the table secret shares to a csv file. Each column is outputted
     * using columns names as follows: [column_name]_[replication_index]. For example,
     * column "A" with replication index 0 is outputted using column name "[A]_0" and
     * same for "[A]_1", "[A]_2", etc. depending on the replication number for the protocol.
     *
     * TODO: create directory if does not exist
     *
     * @param _file_path The file to write the table secret shares to.
     */
    inline void outputCSVTableSecretShares(const std::string &_file_path) {
        std::ofstream file(_file_path, std::ios::out | std::ios::trunc);
        if (file.is_open()) {
            // Write the column names
            for (auto const &imap : schema) {
                for (int i = 0;
                     i <
                     ((B *)(imap.second.get())->contents.get())->asEVector().getReplicationNumber();
                     ++i) {
                    file << imap.first << "_" << i;
                    if (imap.first != schema.rbegin()->first ||
                        i < ((B *)(imap.second.get())->contents.get())
                                    ->asEVector()
                                    .getReplicationNumber() -
                                1) {
                        file << ",";
                    }
                }
            }
            file << std::endl;

            // Write the column values
            for (int i = 0; i < rows; ++i) {
                for (auto const &imap : schema) {
                    for (int j = 0; j < ((B *)(imap.second.get())->contents.get())
                                            ->asEVector()
                                            .getReplicationNumber();
                         ++j) {
                        file << ((B *)(imap.second.get())->contents.get())->vector(j)[i];
                        if (imap.first != schema.rbegin()->first ||
                            j < ((B *)(imap.second.get())->contents.get())
                                        ->asEVector()
                                        .getReplicationNumber() -
                                    1) {
                            file << ",";
                        }
                    }
                }
                file << std::endl;
            }

            file.close();
        } else {
            std::cerr << "Unable to open file" << std::endl;
            exit(-1);
        }
    }

    /**
     * Output the secret shares of a column to a file. The file will contain one
     * line for each element in the column. Each line will contain the replicated
     * secret shares separated by a space.
     * @param columnName The name of the column.
     * @param outputFile The file to write the secret shares to.
     */
    inline void outputSecretShares(const std::string &columnName, const std::string &outputFile) {
        if ((*this)[columnName].encoding == Encoding::BShared) {
            ((B *)((*this)[columnName].contents.get()))->outputSecretShares(outputFile);
        } else {
            ((A *)((*this)[columnName].contents.get()))->outputSecretShares(outputFile);
        }
    }

    /**
     * @brief Shuffle the table and mask invalid rows. Call `finalize`
     * before revealing the outputs to untrusted parties.
     *
     * @param do_shuffle whether to shuffle the table before revealing.
     * default true. If not shuffled (or sorted in a known order),
     * inference attacks may be possible.
     *
     */
    void finalize(bool do_shuffle = true) {
        if (do_shuffle) {
            shuffle();
        }

        // Blind invalid rows
        mask();
    }

    /**
     * Opens the encoded table to all computing parties.
     * @return The opened (plaintext) table.
     */
    DataTable open() {
        DataTable res;
        for (auto &[k, v] : schema) {
            if (k == ENC_TABLE_VALID) {
                continue;
            }

            if (v->encoding == Encoding::BShared) {
                res.push_back(((B *)(v.get())->contents.get())->open());
            } else if (v->encoding == Encoding::AShared) {
                res.push_back(((A *)(v.get())->contents.get())->open());
            }
        }
        return res;
    }

    /**
     * @brief Opens the encoded table to all computing parties and includes
     * the schema
     *
     * @return A LabeledDataTable pair (a.k.a. pair<DataTable, vector<string>>)
     * Access the column-major data via the first element of the pair, and
     * the schema via the second. Both will be in the same order
     */
    LabeledDataTable open_with_schema(bool remove_invalid = true) {
        DataTable full_res;
        std::vector<std::string> names;
        Vector<Share> valid(rows);
        for (auto &c : schema) {
            Vector<Share> v(rows);
            if (c.second->encoding == Encoding::BShared) {
                v = ((B *)(c.second.get())->contents.get())->open();
            } else if (c.second->encoding == Encoding::AShared) {
                v = ((A *)(c.second.get())->contents.get())->open();
            } else {
                std::cerr << "Unidentified Encoding Type: " << c.second->encoding << std::endl;
                exit(-1);
            }

            if (remove_invalid && c.first == ENC_TABLE_VALID) {
                valid = v;
            } else {
                full_res.push_back(v);
                names.push_back(c.first);
            }
        }

        if (!remove_invalid) {
            return std::make_pair(full_res, names);
        }

        DataTable res;

        // clear out invalid
        for (auto &r : full_res) {
            res.push_back(r.extract_valid(valid));
        }

        return std::make_pair(res, names);
    }

    /**
     * Creates table columns based on a schema.
     * @param columns_ The schema.
     * @param rows_ The column length.
     */
    void addColumns(const std::vector<std::string> &columns_, const int &rows_) {
        for (auto &column : columns_) {
            addColumn(column, rows_);
        }
    }

    /**
     * @brief Create table columns given a list of names. All rows will have the same length as the
     * current table
     *
     * @param columns_
     */
    void addColumns(const std::vector<std::string> &columns_) { addColumns(columns_, rows); }

    /**
     * Allocates a column of a given size and initializes it with zeros.
     *
     * The default column encoding is A-shared. To create a B-shared
     * column, you must enclose the column's name with square brackets, i.e.
     * `column`=[name].
     *
     * @param column The name of the column to allocate.
     * @param rows_ The column size in number of elements.
     *
     */
    void addColumn(const std::string &column, const int rows_) {
        std::unique_ptr<EncodedVector> v;
        if (isBShared(column)) {
            v = std::make_unique<B>(rows_);
        } else {
            v = std::make_unique<A>(rows_);
        }
        // Create a column from the allocated vector
        auto c = std::make_shared<SharedColumn>(std::move(v), column);
        // Update table schema
        auto r = schema.insert({column, c});
        if (!r.second) {
            std::cerr << "Table column with name " << column << " already exists." << std::endl;
            exit(-1);
        }
    }

    /**
     * @brief Add a single column with the given name, with the default number of rows
     *
     * @param column
     */
    void addColumn(const std::string &column) { addColumn(column, rows); }

    /**
     * @brief Remove provided columns from the table.
     *
     * @param cols Vector of column names to remove
     */
    void deleteColumns(const std::vector<std::string> cols) {
        for (auto c : cols) {
            if (schema.find(c) == schema.end()) {
                single_cout("WARNING: trying to delete non-existent column " << c);
            }

            schema.erase(c);
        }
    }

    /**
     * @brief Projection operation, i.e. only keep columns in the provided list.
     * @param cols Vector of column names to keep
     */
    void project(const std::vector<std::string> cols) {
        std::set<std::string> cols_set(cols.begin(), cols.end());
        // Don't remove reserved columns
        cols_set.insert(RESERVED_COLUMNS);

        std::vector<std::string> delete_columns;
        for (const auto &[key, _] : schema) {
            if (!cols_set.count(key)) delete_columns.push_back(key);
        }

        deleteColumns(delete_columns);
    }

    /**
     * @return The table name.
     */
    std::string name() const { return this->tableName; }

    /**
     * @return The table cardinality.
     */
    size_t size() const { return rows; }

    /**
     * @brief Get the schema
     *
     * @return std::map<std::string, std::shared_ptr<EncodedColumn>>
     */
    std::map<std::string, std::shared_ptr<EncodedColumn>> getSchema() const { return schema; }

    /**
     * @brief Get the column names
     *
     * @return std::vector<std::string>
     */
    std::vector<std::string> getColumnNames() {
        std::vector<std::string> _schema;
        for (auto const &imap : schema) {
            _schema.push_back(imap.first);
        }
        return _schema;
    }

    /**
     * Sorts `this` table in place given a specification of columns and sorting directions using
     * the default sorting protocol.
     * @param spec The names of the columns to sort by along with a sorting direction.
     * @return EncodedTable&
     */
    EncodedTable &sort(const std::vector<std::pair<std::string, SortOrder>> spec) {
        return sort(spec, getColumnNames());
    }

    /**
     * Sorts `this` table in place given a specification of columns and sorting directions.
     * @param spec The names of the columns to sort by along with a sorting direction.
     * @param protocol The sorting protocol to use.
     * @return EncodedTable&
     */
    EncodedTable &sort(const std::vector<std::pair<std::string, SortOrder>> spec,
                       const SortingProtocol protocol) {
        return sort(spec, getColumnNames(), protocol);
    }

    /**
     * @brief Sort all given columns in the same direction using the default sorting protocol.
     * @param columns The list of BSharedVColumns to sort on.
     * @param allOrder The direction to sort all sort columns.
     * @return EncodedTable&
     */
    EncodedTable &sort(const std::vector<std::string> columns, SortOrder allOrder = ASC) {
        std::vector<std::pair<std::string, SortOrder>> spec;

        for (auto c : columns) {
            spec.push_back({c, allOrder});
        }

        return sort(spec, getColumnNames());
    }

    /**
     * @brief Sort all given columns in the same direction.
     * @param columns The list of BSharedVColumns to sort on.
     * @param allOrder The direction to sort all sort columns.
     * @param protocol The sorting protocol to use.
     * @return EncodedTable&
     */
    EncodedTable &sort(const std::vector<std::string> columns, SortOrder allOrder,
                       const SortingProtocol protocol) {
        std::vector<std::pair<std::string, SortOrder>> spec;

        for (auto c : columns) {
            spec.push_back({c, allOrder});
        }

        return sort(spec, getColumnNames(), protocol);
    }

    /**
     * @brief Sort the table given a specification.
     * @param spec The names of the columns to sort by along with a
     * sorting direction.
     * @param to_be_sorted_columns The non-sort columns to be sorted
     * according to the sort columns.
     * @param protocol The sorting protocol to use. Default is bitonic
     * sort for 2PC, and quicksort otherwise.
     * @return EncodedTable&
     */
    EncodedTable &sort(const std::vector<std::pair<std::string, SortOrder>> spec,
                       const std::vector<std::string> &to_be_sorted_columns,
                       const SortingProtocol protocol = SortingProtocol::DEFAULT) {
        BEGIN_TABLE_PROFILING();

        size_t original_size = size();
        bool can_unpad = false;
        bool unpad_from_top = true;

        if (protocol == SortingProtocol::BITONICSORT) {
            // bitonic sort only - table size must be a power of two
            pad_power_of_two();
        }

        // Build the sorting input vectors
        std::vector<std::string> _keys;
        std::vector<SortOrder> order;
        std::vector<bool> single_bit_cols(spec.size());
        int i = 0;
        for (auto s : spec) {
            auto k = s.first;
            auto o = s.second;

            if (k == ENC_TABLE_VALID) {
                can_unpad = true;
                single_bit_cols[i] = true;
                // if sort ascending, unpad from top
                // otherwise, unpad from bottom
                unpad_from_top = (o == SortOrder::ASC);
            }

            if (k == ENC_TABLE_JOIN_ID) {
                single_bit_cols[i] = true;
            }

            _keys.push_back(k);
            order.push_back(o);

            i++;
        }

        // Sorting keys must be B-shared columns
        std::vector<B *> keys_vec;
        for (int i = 0; i < _keys.size(); ++i) {
            assert((*this)[_keys[i]].encoding == Encoding::BShared);
            keys_vec.push_back((B *)((*this)[_keys[i]].contents.get()));
        }

        // Now, let's get remaining data in the table
        std::vector<A *> data_a;
        std::vector<B *> data_b;
        for (auto it = schema.begin(); it != schema.end(); ++it) {
            if (std::find(_keys.begin(), _keys.end(), it->first) == _keys.end() &&
                std::find(to_be_sorted_columns.begin(), to_be_sorted_columns.end(), it->first) !=
                    to_be_sorted_columns.end()) {
                // std::cout << it->first << std::endl;
                if (it->second->encoding == Encoding::AShared) {
                    data_a.push_back((A *)(it->second->contents.get()));
                } else if (it->second->encoding == Encoding::BShared) {
                    data_b.push_back((B *)(it->second->contents.get()));
                }
            }
        }

        PRINT_TABLE_INSTRUMENT("[TABLE_SORT] "
                               << (protocol == SortingProtocol::RADIXSORT ? "RS" : "QS")
                               << " k=" << keys_vec.size() << " n=" << keys_vec[0]->size());

        if (protocol == SortingProtocol::BITONICSORT) {
#ifndef DEBUG_SKIP_EXPENSIVE_TABLE_OPERATIONS
            operators::bitonic_sort(keys_vec, data_a, data_b, order);
#else
            single_cout("...skipped");
#endif
            // We can only shrink back if we sorted on valid, since all
            // padded rows are invalid.
            if (can_unpad) {
                if (unpad_from_top) {
                    // chop the top padded rows
                    tail(original_size);
                } else {
                    // chop the bottom padded rows
                    resize(original_size);
                }
            }
        } else if (protocol == SortingProtocol::BITONICMERGE) {
            operators::bitonic_merge(keys_vec, data_a, data_b, order);
        } else {
#ifndef DEBUG_SKIP_EXPENSIVE_TABLE_OPERATIONS
            operators::table_sort(keys_vec, data_a, data_b, order, single_bit_cols, protocol);
#else
            single_cout("...skipped");
#endif
        }

        END_TABLE_PROFILING("sort");

        return *this;
    }

    /**
     * Shuffles each column according to the same permutation.
     */
    EncodedTable &shuffle() {
        // split the columns into AShared and BShared
        std::vector<A *> data_a;
        std::vector<B *> data_b;
        for (auto it = schema.begin(); it != schema.end(); ++it) {
            if (it->second->encoding == Encoding::AShared) {
                data_a.push_back((A *)(it->second->contents.get()));
            } else if (it->second->encoding == Encoding::BShared) {
                data_b.push_back((B *)(it->second->contents.get()));
            }
        }

        operators::shuffle(data_a, data_b, size());

        return *this;
    }

    /**
     * @brief Convert column `input_a` to binary and store the result in `output_b`
     *
     * @param input_a
     * @param output_b
     * @return EncodedTable&
     */
    EncodedTable &convert_a2b(const std::string &input_a, const std::string &output_b) {
        *((B *)(*this)[output_b].contents.get()) = ((A *)(*this)[input_a].contents.get())->a2b();
        return *this;
    }

    /**
     * @brief Convert the single-bit column `input_b` to arithmetic and store the result in
     * `output_a`
     *
     * @param input_b
     * @param output_a
     * @return EncodedTable&
     */
    EncodedTable &convert_b2a_bit(const std::string &input_b, const std::string &output_a) {
        *((A *)(*this)[output_a].contents.get()) =
            ((B *)(*this)[input_b].contents.get())->b2a_bit();
        return *this;
    }

    // TODO: add selection bits for the odd even aggregation
    // TODO: for gap window, we do not actually needs to use max aggregation function
    //  but we can use greater than zero aggregation function.
    /**
     * @brief perform odd-even aggregation.
     *
     * @param _keys column names to perform aggregation over (i.e.,
     * group by)
     * @param agg_spec the aggregation specification, a vector of tuples
     * of (input, output, function).
     * @param opt See `AggregationOptions`
     * @return EncodedTable&
     */
    EncodedTable &aggregate(const std::vector<std::string> &_keys, AggregationSpec agg_spec,
                            AggregationOptions opt = {}) {
        // If sorting requested, prepend valid.
        // Otherwise, we assume user has manually sorted and specified all columns explicitly.
        std::vector<std::string> keys = _keys;
        if (opt.do_sort) {
            keys.insert(keys.begin(), ENC_TABLE_VALID);
            this->sort(keys, ASC);
        }

        BEGIN_TABLE_PROFILING();

        size_t original_size = size();

        // pad if necessary
        pad_power_of_two();

        // Create a vector that has the B for keys
        std::vector<B> keys_vec;
        for (int i = 0; i < keys.size(); ++i) {
            assert((*this)[keys[i]].encoding == Encoding::BShared);
            keys_vec.push_back(*(B *)((*this)[keys[i]].contents.get()));
        }

        std::vector<std::tuple<B, B, void (*)(const B &, B &, const B &)>> b_agg;
        std::vector<std::tuple<A, A, void (*)(const A &, A &, const A &)>> a_agg;

        bool has_any_aggregation = false;

        for (auto s : agg_spec) {
            // Unpack pair - column names
            auto [_data, _result, func] = s;

            auto d_encoding = (*this)[_data].encoding;
            auto size = (*this)[_data].size();

            // Types must match
            ASSERT_SAME(d_encoding, (*this)[_result].encoding);

            if (func.isAggregation()) {
                has_any_aggregation = true;
            }

            if (d_encoding == Encoding::AShared) {
                void (*f)(const A &, A &, const A &) = func.getA();

                auto d = *(A *)(*this)[_data].contents.get();
                auto r = *(A *)(*this)[_result].contents.get();
                a_agg.push_back({d, r, f});
            } else {  // BShared
                void (*f)(const B &, B &, const B &) = func.getB();

                auto d = *(B *)(*this)[_data].contents.get();
                auto r = *(B *)(*this)[_result].contents.get();
                b_agg.push_back({d, r, f});
            }
        }

        std::optional<B> table_id_vec;
        if (opt.mark_valid) {
            /* Only return rows that match the left. To do this, we use
             * distinct to check if the _first_ row of a given key is from
             * table 0 or 1. An additional copy aggregation propagates
             * this value down to all other rows for later masking.
             */
            this->addColumns(std::vector<std::string>{ENC_TABLE_UNIQ}, this->rows);
            this->distinct(keys, ENC_TABLE_UNIQ);

            if (opt.table_id.has_value()) {
                // dereference operator on an optional type gives the value
                table_id_vec = *(B *)(*this)[*opt.table_id].contents.get();
            }
        }

        // single_cout("++++ PRE-AGG");
        // print_table(this->open_with_schema(), runTime->getPartyID());

        auto dir = opt.reverse ? Direction::Reverse : Direction::Forward;

        PRINT_TABLE_INSTRUMENT("[TABLE_AGG] k=" << keys_vec.size() << " n=" << keys_vec[0].size()
                                                << " a=" << b_agg.size() + a_agg.size());

#ifndef DEBUG_SKIP_EXPENSIVE_TABLE_OPERATIONS
        orq::aggregators::aggregate(keys_vec, b_agg, a_agg, dir, table_id_vec);
#else
        single_cout("...skipped");
#endif

        if (original_size < size()) {
            resize(original_size);
        }

        // single_cout("//// POST-AGG");
        // print_table(this->open_with_schema(), runTime->getPartyID());

        ////// Validity post-processing //////
        if (opt.mark_valid) {
            if (has_any_aggregation) {
                /* If we're doing aggregations, only return the final-result
                 * rows. If the user only requested join functions (i.e.,
                 * include columns from the left), do not run this filtering.
                 * This is probably not the best way to handle this edge case
                 * but a better option is not immediately apparent.
                 *
                 *
                 * normally, aggregate down. if `_reverse`, aggregate up.
                 * invalidate intermediate rows, leaving only final result
                 * row valid.
                 */

                auto uniq_col = *((B *)(*this)[ENC_TABLE_UNIQ].contents.get());
                auto valid_col = *getValidVector();

                if (opt.reverse) {
                    // reverse. bottom row valid
                    auto short_valid = valid_col.slice(0, valid_col.size() - 1);
                    short_valid &= uniq_col.slice(1);
                } else {
                    // non-reverse. select top row only
                    filter((*this)[ENC_TABLE_UNIQ]);
                }
            }

            this->deleteColumns({ENC_TABLE_UNIQ});
        }

        END_TABLE_PROFILING("aggregate");

        return *this;
    }

    /**
     * @brief Distinct operation on provided columns.
     * Requires equivalent rows to be adjacent to be considered for the distinct.
     *
     * @param _keys: Column names on which to run distinct
     * @param _res: Column name to mark the result
     * @return EncodedTable&
     */
    EncodedTable &distinct(const std::vector<std::string> &_keys, const std::string &_res) {
        // Create a vector that has the B for keys
        std::vector<B *> keys_vec;
        for (int i = 0; i < _keys.size(); ++i) {
            assert(_keys[i] != _res);

            assert((*this)[_keys[i]].encoding == Encoding::BShared);
            keys_vec.push_back((B *)((*this)[_keys[i]].contents.get()));
        }

        B *res_ptr = (B *)((*this)[_res].contents.get());

        operators::distinct(keys_vec, res_ptr);

        return *this;
    }

    /**
     * @brief Single-argument wrapper around the base distinct operation.
     * Performs other operations required for the distinct to work correctly.
     *
     * @param _keys: Column names on which to run distinct
     * @return EncodedTable&
     */
    EncodedTable &distinct(const std::vector<std::string> &_keys) {
        // Sort according to the valid bit and provided keys
        auto sort_keys = _keys;
        sort_keys.insert(sort_keys.begin(), ENC_TABLE_VALID);
        this->sort(sort_keys);

        // Store distinct values in a new column
        this->addColumns({ENC_TABLE_UNIQ});
        this->distinct(_keys, ENC_TABLE_UNIQ);

        // Filter out non-distinct rows
        this->filter((*this)[ENC_TABLE_UNIQ]);
        this->deleteColumns({ENC_TABLE_UNIQ});

        return *this;
    }

    /**
     * @brief Compute a tumbling window on the table
     *
     * @param _time_a
     * @param window_size
     * @param _res
     * @return EncodedTable&
     */
    EncodedTable &tumbling_window(const std::string &_time_a, const Share &window_size,
                                  const std::string &_res) {
        assert((*this)[_time_a].encoding == Encoding::AShared);
        A key_ptr = *(A *)((*this)[_time_a].contents.get());

        assert((*this)[_res].encoding == Encoding::AShared);
        A res_ptr = *(A *)((*this)[_res].contents.get());

        operators::tumbling_window(key_ptr, window_size, res_ptr);

        return *this;
    }

    /**
     * @brief Compute a gap session window on the table
     *
     * @param _keys
     * @param _time_a
     * @param _time_b
     * @param _window_id
     * @param _gap
     * @param _do_sorting
     * @return EncodedTable&
     */
    EncodedTable &gap_session_window(const std::vector<std::string> &_keys,
                                     const std::string &_time_a, const std::string &_time_b,
                                     const std::string &_window_id, const int &_gap,
                                     const bool &_do_sorting = true) {
        if (_do_sorting) {
            std::vector<std::string> sorting_attrs;
            sorting_attrs.push_back(_time_b);
            for (int i = 0; i < _keys.size(); ++i) {
                if (_keys[i] != _time_b) {
                    sorting_attrs.push_back(_keys[i]);
                }
            }
            this->sort(sorting_attrs, ASC);
        }

        std::vector<B> keys_vec;
        for (int i = 0; i < _keys.size(); ++i) {
            assert((*this)[_keys[i]].encoding == Encoding::BShared);
            keys_vec.push_back(*(B *)((*this)[_keys[i]].contents.get()));
        }

        assert((*this)[_time_a].encoding == Encoding::AShared);
        A time_a = *(A *)((*this)[_time_a].contents.get());

        assert((*this)[_time_b].encoding == Encoding::BShared);
        B time_b = *(B *)((*this)[_time_b].contents.get());

        assert((*this)[_window_id].encoding == Encoding::BShared);
        B window_id = *(B *)((*this)[_window_id].contents.get());

        operators::gap_session_window(keys_vec, time_a, time_b, window_id, _gap);

        return *this;
    }

    /**
     * @brief Compute a threshold session window on the table
     *
     * @param _keys
     * @param _function_res
     * @param _time_b
     * @param _window_id
     * @param _threshold
     * @param _do_sorting
     * @param _mark_valid
     * @return EncodedTable&
     */
    EncodedTable &threshold_session_window(const std::vector<std::string> &_keys,
                                           const std::string &_function_res,
                                           const std::string &_time_b,
                                           const std::string &_window_id, const int &_threshold,
                                           const bool _do_sorting = true,
                                           const bool _mark_valid = true) {
        if (_do_sorting) {
            std::vector<std::string> sorting_attrs;
            sorting_attrs.push_back(_time_b);
            for (int i = 0; i < _keys.size(); ++i) {
                if (_keys[i] != _time_b) {
                    sorting_attrs.push_back(_keys[i]);
                }
            }

            this->sort(sorting_attrs, ASC);
        }

        std::vector<B> keys_vec;
        for (int i = 0; i < _keys.size(); ++i) {
            assert((*this)[_keys[i]].encoding == Encoding::BShared);
            keys_vec.push_back(*(B *)((*this)[_keys[i]].contents.get()));
        }

        assert((*this)[_function_res].encoding == Encoding::BShared);
        B function_res = *(B *)((*this)[_function_res].contents.get());

        assert((*this)[_time_b].encoding == Encoding::BShared);
        B time_b = *(B *)((*this)[_time_b].contents.get());

        assert((*this)[_window_id].encoding == Encoding::BShared);
        B window_id = *(B *)((*this)[_window_id].contents.get());

        operators::threshold_session_window(keys_vec, function_res, time_b, window_id, _threshold);

        if (_mark_valid) {
            filter((*this)[_window_id] > 0);
        }

        return *this;
    }

    /**
     * @brief Compute an (arithmetic) prefix sum on the column `key`. NOTE: this ignores the valid
     * vector, so may include invalid rows in the prefix sum. Only call this function if all rows
     * are guaranteed to be valid, or invalid rows have already been zeroed out.
     *
     * @param key
     */
    void prefix_sum(std::string key) { asASharedVector(key).prefix_sum(); }

    /**
     * @brief Zero out the specified columns.
     *
     * @param keys
     * @return EncodedTable&
     */
    EncodedTable &zero(const std::vector<std::string> &keys) {
        for (auto k : keys) {
            (*this)[k].zero();
        }
        return *this;
    }

    /**
     * @brief Resize this table to have `n` rows. Warn if `n` is larger
     * than the current size.
     *
     * NOTE: this does not create a copy. For that, use `deepcopy` first.
     *
     * @param n
     */
    void head(size_t n) {
        if (n > this->size()) {
            std::cerr << "warning: taking head(" << n << ") of table of size " << this->size()
                      << "\n";
        }

        resize(n);
    }

    /**
     * @brief Resize this table to only have its last `n` rows. Warn if
     * `n` is larger than the current size.
     *
     * NOTE: this does not create a copy. For that, use `deepcopy` first.
     *
     * @param n
     */
    void tail(const size_t n) {
        if (n > this->size()) {
            std::cerr << "warning: taking tail(" << n << ") of table of size " << this->size()
                      << "\n";
        }

        for (auto &c : this->getColumnNames()) {
            (*this)[c].tail(n);
        }

        rows = n;
    }

    /**
     * @brief Resize this table to have `n` rows. If n is larger than the
     * current size, extend the table with invalid rows.
     *
     * @param n
     */
    void resize(size_t n) {
        if (n == size()) {
            return;
        }

        for (auto c : this->getColumnNames()) {
            (*this)[c].resize(n);
        }

        rows = n;
    }

    /**
     * @brief Resize this table to the next power-of-two size. Required for
     * aggregation, bitonic sort, and bitonic merge.
     *
     * By default, just resize the table, setting all padded values to 0.
     * However, can optionally pad with a specified value; this is necessary for
     * e.g. bitonic merge.
     */
    void pad_power_of_two(Share pad_value = 0) {
        // -1 because bit_width computes 1+floor(lg(x))
        auto old_size = size();
        resize(1 << std::bit_width(old_size - 1));

        if (pad_value != 0) {
            auto pad_size = size() - old_size;
            Vector<Share> pad(1, pad_value);
            auto fill_vec = runTime->public_share<SharedColumn::replicationNumber>(pad)
                                .repeated_subset_reference(pad_size);

            for (auto c : this->getColumnNames()) {
                if (c == ENC_TABLE_VALID) {
                    continue;
                }

                // Fill the new rows with `fill_vec`
                (*this).asSharedVector(c).vector.slice(old_size) = fill_vec;
            }
        }
    }

    /**
     * @brief Delete the table. Memory will be released; it cannot be
     * used again.
     *
     * Used in larger queries where a table is no longer needed. Preferably, handle this
     * automatically using scoping.
     *
     */
    void deleteTable() {
        schema.clear();
        rows = 0;
        deleted = true;
    }

    /**
     * @brief Rename a column in the EncodedTable.
     *
     * This function allows you to change the name of an existing column in the
     * table's schema. It checks for the existence of the old column name and
     * ensures that the new column name does not already exist in the schema.
     *
     * @param old_name The current name of the column to be renamed.
     * @param new_name The new name to assign to the column.
     *
     * @throws std::runtime_error if the old column name does not exist.
     * @throws std::runtime_error if the new column name already exists.
     *
     * Usage:
     * ```
     * EncodedTable table(...);
     * table.renameColumn("old_column_name", "new_column_name");
     * ```
     */
    void renameColumn(std::string old_name, std::string new_name) {
        if (schema.find(old_name) == schema.end()) {
            std::cerr << "ERROR: column '" << old_name << "' not found\n";
            return;
        }
        if (schema.find(new_name) != schema.end()) {
            std::cerr << "ERROR: column '" << new_name << "' already exists\n";
            return;
        }

        // extract() allows changing a key in a map without reallocating
        auto extracted = schema.extract(old_name);
        extracted.key() = new_name;
        schema.insert(std::move(extracted));
    }

    /**
     * @brief Deep copy the table. Useful if you want to make changes to
     * a table but also maintain the original data.
     */
    EncodedTable deepcopy() {
        auto col = getColumnNames();
        EncodedTable out(tableName, col, size());
        for (auto c : col) {
            out[c] = (*this)[c].deepcopy();
        }
        return out;
    }

    /**
     * @brief concatenate two tables by appending. Rows from `other` are
     * placed below those from the current table. Shared columns (i.e.,
     * those with the same column name) are merged. An additional
     * column, `[~TABLE_ID]`, will be added to denote which original
     * table a given row came from.
     *
     * A new table will be created and returned; the existing tables will be
     * unaffected.
     *
     * If columns with the same name exist in both tables, but are not join
     * keys, one of them should be renamed to prevent conflicts.
     *
     * @param other the table to append
     * @param power_of_two whether to add the table out to the next
     * power of two
     * @return EncodedTable&
     */
    EncodedTable concatenate(EncodedTable &other, bool power_of_two = false) {
        using TableType = EncodedTable<Share, SharedColumn, A, B, EncodedVector, DataTable>;

        std::vector<std::string> new_schema = {ENC_TABLE_JOIN_ID};

        // add columns from this table
        for (auto &c : this->schema) {
            if (c.first == ENC_TABLE_JOIN_ID) {
                continue;
            }

            new_schema.push_back(c.first);
        }

        // add columns from other table, skipping duplicates
        for (auto &c : other.schema) {
            if (this->schema.count(c.first)) {
                continue;
            }
            new_schema.push_back(c.first);
        }

        // Could use `resize()` here, but that incurs an extra table
        // creation, so just figure out the size upfront.

        size_t old_size = this->size() + other.size();
        size_t new_size = old_size;
        if (power_of_two) {
            // If requested, pad the table out to a power of two
            // NOTE: this will pad with all zeros. This may introduced
            // unexpected behavior in certain applications where zero is a
            // valid value.
            new_size = 1 << std::bit_width(new_size - 1);
        }

        TableType new_table(this->name() + "+" + other.name(), new_schema, new_size);

        // copy data from this table to the new table, at the beginning
        // table id here is zero, so ignore
        for (auto &c : this->schema) {
            new_table.copy_column(*this, c.first);
        }

        // copy data from other table to the new table, *after* current
        // table's rows (pass optional start_index)
        auto other_start = this->size();
        for (auto &c : other.schema) {
            new_table.copy_column(other, c.first, other_start);
        }

        // Table ID = 0 for this table; = 1 for the other table.
        // Vector default value is zero, so only need to update other table
        // rows
        auto id_col = (B *)(new_table[ENC_TABLE_JOIN_ID].contents.get());
        // Subset reference to the rows corresponding to other table
        B other_table_id = id_col->slice(other_start, other_start + other.size());
        Vector<Share> one(1, 1);
        B secretSharedOne(1);
        orq::operators::secret_share_vec(one, secretSharedOne);
        // Set them all to 1
        other_table_id = secretSharedOne.repeated_subset_reference(other_table_id.size());

        if (power_of_two) {
            // invalidate padded rows
            new_table.getValidVector()->slice(old_size).zero();
        }

        return new_table;
    }

    EncodedTable inner_join(EncodedTable &right, std::vector<std::string> keys,
                            AggregationSpec agg_spec = {}, JoinOptions opt = {});

    EncodedTable left_outer_join(EncodedTable &right, std::vector<std::string> keys,
                                 AggregationSpec agg_spec = {}, JoinOptions opt = {});

    EncodedTable right_outer_join(EncodedTable &right, std::vector<std::string> keys,
                                  AggregationSpec agg_spec = {}, JoinOptions opt = {});

    EncodedTable full_outer_join(EncodedTable &right, std::vector<std::string> keys,
                                 AggregationSpec agg_spec = {}, JoinOptions opt = {});

    EncodedTable semi_join(EncodedTable &right, std::vector<std::string> keys);

    EncodedTable anti_join(EncodedTable &right, std::vector<std::string> keys);

    EncodedTable uu_join(EncodedTable &right, std::vector<std::string> keys,
                         AggregationSpec agg_spec = {}, JoinOptions opt = {},
                         const SortingProtocol protocol = SortingProtocol::DEFAULT);

    /**
     * @brief Extend the LSB of a column. All other bits are ignored. A
     * value ending with a binary `1` will become all `1`s; namely, `-1`
     * for signed types. values ending with a `0` will become 0. This
     * function should be used to convert boolean 0/1 values into bitmasks,
     * for selectively modifying certain columns.
     *
     * However, for actually hiding of invalid values, see `mask`, above, to
     * set those columns to the table-defined masking value.
     *
     * @param _b_col Column name to extend LSB for.
     * @return EncodedTable&
     */
    EncodedTable &extend_lsb(const std::string &_b_col) {
        B *v = (B *)(*this)[_b_col].contents.get();
        v->extend_lsb(*v);
        return *this;
    }

    /**
     * @brief Mask all columns using the VALID column.
     *
     * @return EncodedTable&
     */
    EncodedTable &mask() { return mask(ENC_TABLE_VALID, getColumnNames()); }

    /**
     * Checks if a column with name `name` contains boolean shares.
     * @param name The name of the column.
     * @return True if the column is B-shared, False otherwise.
     *
     * NOTE: Names of columns that include boolean shares are within square brackets, i.e.,
     * `[column_name]`.
     */
    static bool isBShared(const std::string name) { return name.find("[") != std::string::npos; }

    /**
     * Secret-shares the plaintext columns according to the encodings in the `schema`.
     * @tparam T The plaintext data type.
     * @tparam R The replication factor of the secret-shared columns.
     * @param columns The plaintext columns.
     * @param schema The encoded column names.
     * @param _party_id data owner
     * @return A vector of EncodedColumns that can be used to instantiate an EncodedTable.
     *
     * NOTE: This method can be used to construct an EncodedTable from a plaintext one as
     * follows: EncodedTable t = secret_share(plaintext_table, schema);
     */
    template <typename T, int R>
    static std::vector<std::shared_ptr<EncodedColumn>> secret_share(
        const std::vector<orq::Vector<T>> &columns, const std::vector<std::string> &schema,
        const int &_party_id = 0) {
        assert(columns.size() == schema.size());
        using namespace service;
        std::vector<std::shared_ptr<EncodedColumn>> cols;
        for (int i = 0; i < columns.size(); i++) {
            std::unique_ptr<EncodedVector> v;
            if (isBShared(schema[i])) {
                v = std::make_unique<B>(runTime->secret_share_b<R>(columns[i], _party_id));
            } else {
                v = std::make_unique<A>(runTime->secret_share_a<R>(columns[i], _party_id));
            }
            cols.push_back(std::make_shared<SharedColumn>(std::move(v), schema[i]));
        }
        return cols;
    }

    /**
     * @brief Given an opened, labeled table, return the associated column.
     * NOTE: Once we replace LabeledDataTable with a better-implemented class
     * this can go away.
     *
     * We need this to be a member function because it needs to know the
     * correct template arguments for DataTable and Share.
     */

    /**
     * @brief Given an opened, labeled table, return the associated column. We need this to be a
     * member function because it needs to know the correct template arguments for DataTable and
     * Share.
     *
     * TODO: Replace LabeledDataTable with a better-implemented class so this can go away.
     *
     * @param table
     * @param c column name
     * @return * Vector<Share>
     */
    static Vector<Share> get_column(std::pair<DataTable, std::vector<std::string>> table,
                                    std::string c) {
        auto [data, labels] = table;
        auto idx = std::find(labels.begin(), labels.end(), c);
        if (idx == labels.end()) {
            std::cerr << "ERROR: column '" << c << "' not found\n";
            abort();
        }
        return data[idx - labels.begin()];
    }

   private:
    /**
     * @brief Copy column from table `t` into this table. This table's
     * schema is assumed to already contain a column of the correct name.
     *
     * @tparam T  the type of the vector `ASharedVector` or `BSharedVector`
     * @param t source table
     * @param from  the table to copy from
     * @param to  column name to copy
     * @param start_index  the row in this table where the copied column
     * should be placed (default 0)
     */
    template <typename T>
    void copy_column_typed(EncodedTable &t, std::string from, std::string to,
                           VectorSizeType start_index = 0) {
        T *src = (T *)(t[from].contents.get());
        T *dst = (T *)((*this)[to].contents.get());
        dst->slice(start_index, start_index + src->size()) = *src;
    }

    /**
     * @brief Untyped copy column - see copy_column_typed<T>
     *
     * @param t
     * @param from
     * @param to
     * @param start_index
     */
    void copy_column(EncodedTable &t, std::string from, std::string to,
                     VectorSizeType start_index = 0) {
        auto enc = t[from].encoding;

        switch (enc) {
            case Encoding::AShared:
                copy_column_typed<A>(t, from, to, start_index);
                break;
            case Encoding::BShared:
                copy_column_typed<B>(t, from, to, start_index);
                break;
            default:
                std::cerr << "Unidentified Encoding Type" << enc << std::endl;
                exit(-1);
        }
    }

    /**
     * @brief Copy column `name` from table `t` into this table using the same name
     *
     * @param t
     * @param name
     * @param start_index
     */
    void copy_column(EncodedTable &t, std::string name, VectorSizeType start_index = 0) {
        copy_column(t, name, name, start_index);
    }
};
}  // namespace orq::relational
