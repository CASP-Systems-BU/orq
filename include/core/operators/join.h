/**
 * @file join.h
 * @brief Implementation of EncodedTable join
 *
 */

#include "core/containers/tabular/encoded_table.h"

#define TEMPLATE_DEF \
    template <typename T, typename Col, typename A, typename B, typename E, typename DT>
#define TABLE_T EncodedTable<T, Col, A, B, E, DT>

namespace orq::relational {
    
/**
 * @brief Arbitrary join between this table and `right`. assumes primary
 * key-foreign key relationship, but _does not enforce_.
 *
 * Used to implement EncodedTable::inner_join, EncodedTable::left_outer_join,
 * EncodedTable::right_outer_join, EncodedTable::full_outer_join,
 * EncodedTable::semi_join, and EncodedTable::anti_join
 *
 * @param right the other table
 * @param keys the set of key(s) to join on, with a primary (this table)
 * / foreign key (`right` table) relationship
 * @param agg_spec an aggregation specification, a vector of tuples
 * consisting of `{input-column, output-column, aggregation-type}`
 * @param opt See `JoinOptions`
 * @return TABLE_T
 */
TEMPLATE_DEF
TABLE_T TABLE_T::_join(TABLE_T &right, std::vector<std::string> keys, AggregationSpec agg_spec,
                       JoinOptions opt) {
    PRINT_TABLE_INSTRUMENT("[TABLE_JOIN] L=" << size() << " R=" << right.size()
                                             << " k=" << keys.size() << " a=" << agg_spec.size());

    // Aggregation needs a power-of-two sized input. However,
    // new sorting algorithms do not, so we should sort on the
    // smallest table possible.
    //
    // Don't pad yet; we need to sort and can do that on the smaller
    // table.
    TABLE_T concat = this->concatenate(right, false);

    STOPWATCH("concat");

    // Project out the result columns
    // - All key and right-table columns
    // - All aggregation columns
    // - ...and then either columns from left only if left outer.
    std::set<std::string> output_columns(keys.begin(), keys.end());
    auto rc = right.getColumnNames();
    output_columns.insert(rc.begin(), rc.end());

    for (auto s : agg_spec) {
        auto [_data, _result, _] = s;
        output_columns.insert(_data);
        output_columns.insert(_result);
    }

    if (opt.left_outer) {
        auto c = this->getColumnNames();
        output_columns.insert(c.begin(), c.end());
    }

    concat.project(std::vector<std::string>(output_columns.begin(), output_columns.end()));

    // Adding VALID as the first key sorts the invalid rows to the
    // top so they can be ignored
    keys.insert(keys.begin(), ENC_TABLE_VALID);

    // But add TID to the end of key vec, so rows from different
    // tables are interleaved
    auto keys_plus_tid = keys;
    keys_plus_tid.push_back(ENC_TABLE_JOIN_ID);

    // note: actually sorting on `valid || keys || tid`
    concat.sort(keys_plus_tid);

    STOPWATCH("sort");

    // need a temp column for valid because valid column is itself
    // one of the aggregation keys, and it is not possible to have
    // a key be both an aggregated value as well as a key.
    concat.addColumns(std::vector<std::string>{ENC_TABLE_UNIQ, "[##VALID_TEMP]"});

    if (!opt.right_outer) {
        // If left outer join or inner join, need to invalidate the
        // right with an aggregation. For right outer and full outer,
        // this is not required.
        agg_spec.push_back({"[##VALID_TEMP]", "[##VALID_TEMP]", valid<B>});

        // distinct over `valid || keys`. only needed for left outer
        // and inner join.
        concat.distinct(keys, ENC_TABLE_UNIQ);
    }

    if (opt.left_outer) {
        if (opt.right_outer) {
            // Full outer join. All rows valid, so do nothing.
            concat["[##VALID_TEMP]"] = concat[ENC_TABLE_VALID].deepcopy();
        } else {
            // Left outer join. Row is valid if a match, or a non-match on
            // the left. That is, if the row is:
            //  - not unique (0) and from the right table (1), OR
            //  - unique (1) and from the left table (0).
            // We do not want rows from the left where a match on the right
            // exists.
            //
            // Thus, invalidated rows are:
            //  (a) unique and from the right (= no match)
            //  (b) not unique and from the left (= duplicate PK)
            //  (c) from the left but already matched
            //
            // (Unique/right is invalid because we run distinct
            // ignoring table ID. Thus, a row from the right being
            // unique means that it doesn't have a left PK.)
            //
            // To compute the final condition (c), we need a distinct() in
            // reverse, but this is just distinct offset by 1.
            // The condition would be `~(L & ~uniq)` but this is equivalent
            // to `R | Uniq`
            //
            // TODO: unique_ptr dereference shouldn't be required here...

            // (a) and (b)
            concat["[##VALID_TEMP]"] =
                concat[ENC_TABLE_VALID] & ~(*(concat[ENC_TABLE_JOIN_ID] & concat[ENC_TABLE_UNIQ]));
            auto n = concat.size();
            // (c)
            // invalidate if LEFT and NEXT NOT UNIQUE
            concat.asBSharedVector("[##VALID_TEMP]").slice(0, n - 1) &=
                concat.asBSharedVector(ENC_TABLE_JOIN_ID).slice(0, n - 1) |
                concat.asBSharedVector(ENC_TABLE_UNIQ).slice(1);
        }
    } else {
        if (opt.right_outer) {
            // Right outer join. All rows on the right are valid,
            // invalidate the left.
            concat["[##VALID_TEMP]"] = concat[ENC_TABLE_VALID] & concat[ENC_TABLE_JOIN_ID];
        } else {
            // Inner join. Row is valid if not unique. Unique rows
            // are invalidated (whether from the left, or
            // nonmatching on the right).
            concat["[##VALID_TEMP]"] = concat[ENC_TABLE_VALID] & ~concat[ENC_TABLE_UNIQ];
        }
    }

    if (opt.anti) {
        // We do actually need copy (vs. valid), because we are only
        // able to mark first row valid of antijoin (in case of
        // duplicates)
        //
        // Does not violate VALID non-revalidation contract because
        // we still call `filter()` below.
        agg_spec.push_back({"[##VALID_TEMP]", "[##VALID_TEMP]", copy<B>});
    }

    concat.deleteColumns({ENC_TABLE_UNIQ});

    STOPWATCH("valid");

    // Run actual joins over `valid || keys`
    concat.aggregate(keys, agg_spec,
                     {
                         .reverse = true,
                         .do_sort = false,
                         .table_id = ENC_TABLE_JOIN_ID  // for aggregation selection
                     });

    STOPWATCH("agg");

    // From earlier.
    concat.filter(concat["[##VALID_TEMP]"]);
    concat.deleteColumns({"[##VALID_TEMP]", ENC_TABLE_JOIN_ID});

    STOPWATCH("valid");

    // Trim inner joins and right outer joins if requested: both are bounded
    // by the size of the right table.
    if (opt.trim_invalid && !opt.left_outer) {
        // Trim if
        //   R / L < (w + c - 1) / (c + 2)
        // where
        // - R, L are the sizes of the right and left tables, resp.
        // - w is the bitwidth
        // - c is the number of columns in this table
        //
        // For a much more thorough discussion & analysis, see issue #304.
        // The high-level idea is that we only want to trim when the cost incurred by
        // the extra sort is worth the savings for future join operations.
        //
        // NOTE: this is the radix sort version. See ORQ paper for Quicksort
        // equation; we may want to switch eventually.
        auto L = size();
        auto R = right.size();

        auto w = std::numeric_limits<std::make_unsigned_t<T>>::digits;
        auto c = schema.size() - 1;  // don't count valid

        if (R < (L * (w + c - 1) / (c + 2))) {
            // move valid rows to top
            concat.sort({ENC_TABLE_VALID}, DESC);
            // the output is at most the size of the right table
            concat.head(R);
        } else {
            // Uncomment this line to debug the trim heuristic
            // single_cout("Note: skipping requested trim: " << VAR(L) << VAR(R) << VAR(w) <<
            // VAR(c));
        }
    }

    STOPWATCH("trim");

    return concat;
}

/**
 * @brief Inner join
 *
 * @param right
 * @param keys
 * @param agg_spec
 * @param opt
 * @return TABLE_T
 */
TEMPLATE_DEF
TABLE_T TABLE_T::inner_join(TABLE_T &right, std::vector<std::string> keys, AggregationSpec agg_spec,
                            JoinOptions opt) {
    opt.left_outer = false;
    opt.right_outer = false;
    return _join(right, keys, agg_spec, opt);
}

/**
 * @brief
 *
 * @param right
 * @param keys
 * @param agg_spec
 * @param opt
 * @return TABLE_T
 */
TEMPLATE_DEF
TABLE_T TABLE_T::left_outer_join(TABLE_T &right, std::vector<std::string> keys,
                                 AggregationSpec agg_spec, JoinOptions opt) {
    opt.left_outer = true;
    opt.right_outer = false;
    return _join(right, keys, agg_spec, opt);
}

/**
 * @brief
 *
 * @param right
 * @param keys
 * @param agg_spec
 * @param opt
 * @return TABLE_T
 */
TEMPLATE_DEF
TABLE_T TABLE_T::right_outer_join(TABLE_T &right, std::vector<std::string> keys,
                                  AggregationSpec agg_spec, JoinOptions opt) {
    opt.left_outer = false;
    opt.right_outer = true;
    return _join(right, keys, agg_spec, opt);
}

/**
 * @brief
 *
 * @param right
 * @param keys
 * @param agg_spec
 * @param opt
 * @return TABLE_T
 */
TEMPLATE_DEF
TABLE_T TABLE_T::full_outer_join(TABLE_T &right, std::vector<std::string> keys,
                                 AggregationSpec agg_spec, JoinOptions opt) {
    opt.left_outer = true;
    opt.right_outer = true;
    return _join(right, keys, agg_spec, opt);
}

/**
 * @brief Semi-join between this table and `right`. Under the hood, performs
 * `right.inner_join(this)`. See the paper for more details on why this is correct.
 *
 * @param right
 * @param keys
 * @return TABLE_T
 */
TEMPLATE_DEF
TABLE_T TABLE_T::semi_join(TABLE_T &right, std::vector<std::string> keys) {
    auto t = right.inner_join(*this, keys);
    t.project(getColumnNames());
    return t;
}

/**
 * @brief Anti-join between this table and `right`. Under the hood, perms
 * `right.right_outer_join(this)`, along with some special handling of the valid bit. See the
 * paper for more details on why this is correct.
 *
 * @param right
 * @param keys
 * @return TABLE_T
 */
TEMPLATE_DEF
TABLE_T TABLE_T::anti_join(TABLE_T &right, std::vector<std::string> keys) {
    auto t = right.right_outer_join(*this, keys, {}, {.anti = true});
    t.project(getColumnNames());
    return t;
}

/**
 * @brief Unique key inner join. Execute a more optimized algorithm
 * which only works if both tables have unique (compound) keys. Under
 * oblivious compute, this condition of course cannot be checked, so it
 * is the responsibility of the developer to verify application of this
 * function is appropriate. Calling `uu_join` on tables with non-unique
 * keys will return incorrect results.
 *
 * This effectively performs a private set intersection over `keys`.
 *
 * @param right
 * @param keys
 * @param agg_spec Aggregation specification used for listing columns to
 * copy from left table to output.
 * @param opt aggregation options
 * @param protocol which sorting protocol to use
 * @return TABLE_T
 */
TEMPLATE_DEF
TABLE_T TABLE_T::uu_join(TABLE_T &right, std::vector<std::string> keys, AggregationSpec agg_spec,
                         JoinOptions opt, const SortingProtocol protocol) {
    TABLE_T concat = this->concatenate(right, false);

    auto user_keys = keys;

    // sort on `valid || keys || tid`
    // NOTE: these aren't always necessary (for the initial tables, where
    // all rows are known to be valid), but in general, we need to valid
    // sort.

    // keys.insert(keys.begin(), ENC_TABLE_VALID);
    // keys.push_back(ENC_TABLE_JOIN_ID);
    auto sortingKeys = keys;
    if (protocol == SortingProtocol::BITONICMERGE) {
        sortingKeys.push_back(ENC_TABLE_JOIN_ID);
    }
    concat.sort(sortingKeys, SortOrder::ASC, protocol);

    auto S = concat.size();

    // TODO: is this equivalent to distinct()?
    for (auto k : user_keys) {
        // check for adjacent match for each key
        auto ksv = concat.asBSharedVector(k);
        concat.asBSharedVector(ENC_TABLE_VALID).slice(1) &= ksv.slice(0, S - 1) == ksv.slice(1);
    }

    // first row guaranteed to be invalid
    concat.asSharedVector(ENC_TABLE_VALID).slice(0, 1).zero();

    auto out_columns = right.getColumnNames();

    for (auto s : agg_spec) {
        auto [_data, _result, func] = s;

        // only allow copy<> aggregations
        if (func.isAggregation()) {
            std::cerr << "Aggregations are not supported for unique-key join.\n";
            abort();
        }

        assert(_data == _result);

        // TODO: ideally this would use a data view, not a copy, to just
        // iterate backwards.
        concat.asSharedVector(_data).reverse();
        concat.asSharedVector(_data).slice(0, S - 1) = concat.asSharedVector(_data).slice(1);
        concat.asSharedVector(_data).reverse();

        out_columns.push_back(_data);
    }

    concat.project(out_columns);

    // This needs to be removed manually.
    concat.deleteColumns({ENC_TABLE_JOIN_ID});

    if (opt.trim_invalid) {
        concat.sort({ENC_TABLE_VALID}, DESC, protocol);
        concat.head(std::min(this->size(), right.size()));
    }

    return concat;
}
}  // namespace orq::relational