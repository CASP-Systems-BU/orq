# Oblivious Join

This document outlines our oblivious join protocol and describes some common patterns when using it.

## Normal Inner Join

Outside of the context of secure computation, an **inner join** allows us to match data between two tables based on some shared keys. For example, we might have one table with information about `courses` and another with information about `professors`. A join allows us to learn combined information from both tables.

Table `professors`:

| `prof_id` | `prof_name` |
| ---- | ------ |
| 1    | John   |
| 2    | Vasia  |
| 3    | Mayank |
| 4    | Ran    |

Table `courses`:

| `course_id` | `course_name` | `prof_id` | `num_students` |
| ---- | ------ | -------------- | -------------- |
|    1 | Grad Systems | 1 | 30 |
|    2 | Undergrad Systems | 1 | 100 |
|    3 | Crypto | 3 | 50 |
|    4 | Streaming | 2 | 25 |
|    5 | Data Structures | 2 | 250 |

This is an example of a _primary key-foreign key_ join; `professors` contains unique primary keys; `courses` contains a foreign key to `professors`. (`courses` also contains its own primary key, `course_id`, which could be a foreign key in another table, such as `students`.)

A PK-FK join between these two tables (over key `prof_id`) would give

| `course_id` | `course_name` | `prof_id` | `num_students` | `prof_name` |
| ---- | ------ | -------------- | -------------- | ---- |
|    1 | Grad Systems | 1 | 30 | John |
|    2 | Undergrad Systems | 1 | 100 | John |
|    3 | Crypto | 3 | 50 | Mayank |
|    4 | Streaming | 2 | 25 | Vasia |
|    5 | Data Structures | 2 | 250 | Vasia |

Note that Ran (`prof_id = 4`) is not teaching any classes, so does not appear in the join output. If desired, we could use an _outer join_, instead, to also include non-matching rows.

We might also perform an aggregation, such as `sum(num_students)`, grouping by `prof_id`. This would tell us how many students each professor teaches.

`prof_id` | `num_students` | `prof_name` |
| ---- | ------ | -------------- |
|    1 | 130 | John |
|    3 | 50 | Mayank |
|    2 | 275 | Vasia |

## Oblivious Inner Join

Computing joins normally requires multiple equality operations (or perhaps some other data structure like a hash table), which would be slow under MPC (as well as harming privacy by allowing inference attacks). Instead, we take a difference approach: first, the two tables are concatenated; the larger combined table is sorted; and an aggregation step is applied to update data. The annotated pseudocode is shown below.

We refer to the tables as `left` and `right`; while these usually will be the primary-key table and foreign-key table, respectively, the algorithm is general enough to handle cases where the PK-FK constraint is violated and some rows on the right have keys non-existent on the left. These rows will be deleted. However, the algorithm does _not_ handle duplicate keys on the left.

```python
def left.join(right, keys, aggregations):
    # Build the combined table. `concatenate` adds a table ID column: 0 for the 
    # left, 1 for the right
    concat = left.concatenate(right)

    # Sort table by the given key. Within each group, the single row from the 
    # left (TID 0) will be the first row, should it exist, followed by multiple 
    # rows from the right (TID 1).
    #
    # We also sort by the VALID column, so that all invalid rows are pushed to
    # the top.
    keys = ["VALID"] || keys
    keys_plus_tid = keys || ["TABLE ID"]
    concat.sort(keys_plus_tid, ASCENDING)

    # Mark distinct rows. Since we ignore TID here, we can use the result to
    # mark rows with no matching PK on the left. Specifically, the first row of 
    # each key group is invalidated. Below, we will apply the valid state of the
    # first row of each group to all other rows in that group.
    concat["VALID TEMP"] = concat["VALID"] & ~concat.distinct(keys)

    # Update the validity of the merged rows, using a special validity 
    # aggregator (this looks similar to the join-copy aggregator, with slightly
    # special handling). Since the aggregation here includes the TID column, we
    # only aggregate across rows originating from the same table. This is how
    # the `distinct` call from above is used. 
    concat.reverse_aggregate(keys_plus_tid, {
        {"VALID TEMP", "VALID TEMP", aggregators::valid}
    })

    # Perform actual aggregation as requested by the user. We don't care what 
    # happens to the invalid rows, as long as they are grouped together and then
    # ignored.
    #
    # Not pictured: in reality this call includes some additional arguments to
    # properly handle inter-table copying and valid updates; the details are not
    # important. See the implementation of `aggregate` for more details.
    concat.reverse_aggregate(keys, aggregations)

    # Invalidate rows that do not have a valid primary key, using the temporary
    # column from above. We cannot update before the aggregation, because this
    # would contaminate the results of the aggregation.
    concat.updateValid(concat["VALID TEMP"])

    ### Post-processing ###

    # (Optional) The output is at most the size of the right table, so sort by 
    # the valid bit and truncate.
    concat.sort("VALID", ASCENDING)
    concat.tail(right.size())

    # Delete remaining intermediate columns

    # (Optional) Mask any remaining invalid rows and shuffle the table if needed
    concat.finalize()

    return concat
```

### Pseudocode Walkthrough
Here is a step-by-step walkthrough of the above pseudocode.

Assume we have two tables, `towns` and `residents`, with `town_id` as the PK-FK relation. If not explicitly specified, assume all rows are valid.

Table `towns`:

| `town_id` | `taxes` | `zipcode` |
| - | - | - |
| 1 | 500 | 22210 |
| 2 | 300 | 25889 |
| 3 | 950 | 67201 |
| 4 | 4000 | 40023 |

Table `residents`:

| `rid` | `salary` | `town_id` |
| - | - | - |
| 1 | 40000 | 3 |
| 2 | 110000 | 2 |
| 3 | 94000 | 1 |
| 4 | 72000 | 2 |
| 5 | 63000 | 1 |
| 6 | 0 | 7 |

By joining these two tables, we learn each resident's zip code and taxes. The two `join` aggregations tell the `join` function that we want to copy those columns from the left table to the right (otherwise, they are dropped).

> No one lives in town 4: the taxes are too high. Resident 6 has no income and lives in an imaginary town.

```python
out = towns.join(residents, {"town_id"},
    {"taxes",   "taxes",   copy},
    {"zipcode", "zipcode", copy}
)
```

 The expected output will be:

| `rid` | `salary` | `town_id` | `taxes` | `zipcode`
| - | - | - | - | - |
| 1 | 40000 | 3 | 950 | 67201 |
| 2 | 110000 | 2 | 300 | 25889 |
| 3 | 94000 | 1 | 500 | 22210 |
| 4 | 72000 | 2 | 300 | 25889 |
| 5 | 63000 | 1 | 500 | 22210 |

Here is a walkthrough of the full join operation, with `left = towns` and `right = residents`.

---

    concat = left.concatenate(right)

| `rid` | `salary` | `town_id` | `taxes` | `zipcode` | `TABLE ID` |
| - | - | - | - | - | - |
| | | 1 | 500 | 22210 | 0
| | | 2 | 300 | 25889 | 0 
| | | 3 | 950 | 67201 | 0
| | | 4 | 4000 | 40023 | 0
| 1 | 40000 | 3 | | | 1
| 2 | 110000 | 2 | | | 1
| 3 | 94000 | 1 | | | 1
| 4 | 72000 | 2 | | | 1
| 5 | 63000 | 1 | | | 1
| 6 | 0 | 7 | | | 1

> Assume blank cells take some default value, such as zero.

---

    concat.sort(keys_plus_tid, ASCENDING)

We sort on `town_id` first, and then `TABLE ID`.

| `rid` | `salary` | `town_id` | `taxes` | `zipcode` | `TABLE ID` |
| - | - | - | - | - | - |
|   |       | 1 | 500 | 22210 | 0
| 3 | 94000 | 1 | | | 1
| 5 | 63000 | 1 | | | 1
|   |       | 2 | 300 | 25889 | 0 
| 4 | 72000 | 2 | | | 1
| 2 | 110000 | 2 | | | 1
|   |       | 3 | 950 | 67201 | 0
| 1 | 40000 | 3 | | | 1
| | | 4 | 4000 | 40023 | 0
| 6 | 0 | 7 | | | 1

> Observe that we do not assume stable sort: the order of residents 2 and 4 have switched, because they have the same compound key. The algorithm will be more efficient with stable sort.

---

Store the temporary valid column. At this point, assume all rows are valid.

    concat["VALID TEMP"] = concat["VALID"] & ~concat.distinct(keys)

| `rid` | `salary` | `town_id` | `taxes` | `zipcode` | `TABLE ID` | `VALID_TEMP` |
| - | - | - | - | - | - | - |
|   |       | 1 | 500 | 22210 | 0 | 0
| 3 | 94000 | 1 | | | 1 | 1
| 5 | 63000 | 1 | | | 1 | 1
|   |       | 2 | 300 | 25889 | 0 | 0
| 4 | 72000 | 2 | | | 1 | 1
| 2 | 110000 | 2 | | | 1 | 1
|   |       | 3 | 950 | 67201 | 0 | 0
| 1 | 40000 | 3 | | | 1 | 1
| | | 4 | 4000 | 40023 | 0 | 0
| 6 | 0 | 7 | | | 1 | 0

---

Run an validity aggregation on the temp column:

    concat.reverse_aggregate(keys_plus_tid, {
        {"VALID TEMP", "VALID TEMP", aggregators::valid}
    })

In this example, no updates are required, but if we had multiple FK rows without a PK (e.g., many people in town `7`), these would be invalidated here. Really, the `valid` aggregator should be called `invalidate`. It never works in the opposite direction.

---

Run the actual aggregation.

    concat.reverse_aggregate(keys, aggregations)

Here, we're just running two `copy` operations, so no aggregation functions.
These copy down the `taxes` and `zipcode` fields from each row on the left to the corresponding rows on the right.

| `rid` | `salary` | `town_id` | `taxes` | `zipcode` | `TABLE ID` | `VALID_TEMP` |
| - | - | - | - | - | - | - |
|   |       | 1 | 500 | 22210 | 0 | 0
| 3 | 94000 | 1 | 500 | 22210 | 1 | 1
| 5 | 63000 | 1 | 500 | 22210 | 1 | 1
|   |       | 2 | 300 | 25889 | 0 | 0
| 4 | 72000 | 2 | 300 | 25889 | 1 | 1
| 2 | 110000 | 2 | 300  | 25889 | 1 | 1
|   |       | 3 | 950 | 67201 | 0 | 0
| 1 | 40000 | 3 | 950 | 67201 | 1 | 1
| | | 4 | 4000 | 40023 | 0 | 0
| 6 | 0 | 7 | | | 1 | 0

---

Now that the aggregation is done, we can begin finally update the `VALID` column using the temp column before. The reason we need a temp column is because the `VALID` column itself can be updated by `aggregate`: if we ran a true aggregation function, like `sum` or `count`, only the result row would be marked valid. In this case, we're just removing all rows from the left, as well as unmatched rows from the right.

    concat.updateValid(concat["VALID TEMP"])
    concat.sort("VALID", ASCENDING)
    concat.tail(right.size()) # 6

For simplicity of presentation, I'm just removing invalid rows; in reality, this would need to be handled oblivious by the sort + trim operation.

| `rid` | `salary` | `town_id` | `taxes` | `zipcode` | `TABLE ID` | `VALID_TEMP` |
| - | - | - | - | - | - | - |
| 3 | 94000 | 1 | 500 | 22210 | 1 | 1
| 5 | 63000 | 1 | 500 | 22210 | 1 | 1
| 4 | 72000 | 2 | 300 | 25889 | 1 | 1
| 2 | 110000 | 2 | 300  | 25889 | 1 | 1
| 1 | 40000 | 3 | 950 | 67201 | 1 | 1

> We know the output size is upper-bounded by `right.size()`, but some invalid rows could still be included in a trimmed output of that size.

---

The last step is to remove intermediate columns.

| `rid` | `salary` | `town_id` | `taxes` | `zipcode`
| - | - | - | - | - |
| 3 | 94000 | 1 | 500 | 22210 |
| 5 | 63000 | 1 | 500 | 22210 |
| 4 | 72000 | 2 | 300 | 25889 |
| 2 | 110000 | 2 | 300  | 25889
| 1 | 40000 | 3 | 950 | 67201 |

...which is what we expected (up to a sort).

### Handling Validity

We handle propagations of validity simply by including `VALID` as the first aggregation group-by key. This pushes all invalid rows to the top of the table, and all valid rows to the bottom (assuming we sort ascending). Then, aggregations can continue, effectively locally, ignoring the valid bit. To see why this works, consider the possible cases for a PK-FK join.

- **PK valid, FK valid**: normal handling. We walk through this above.
- **PK valid, FK invalid**: the FK row gets moved to the top of the table and does not take part in the join. If there are other rows with this same key, they will be included independent of the invalid row.
- **PK invalid, FK valid**: this is the most interesting case. Here, we need to treat the PK as if it does not exist. But since we sort by the valid bit, the PK is no longer "next to" its FK: so the non-matching check above, using `distinct`, will cause all FK rows with the given invalid key to be deleted.
- **PK invalid, FK invalid**: Because we [never revalidate invalid rows](#never-revalidate-invalid-rows), we actually don't care what happens in this case. In reality, whatever aggregation has been requested will run in the invalid rows (it must, for security to hold) but we ignore the result.

We have not discussed the final case of validating summary aggregations. In this case, we again use `distinct` to mark only the _last_ row of each group valid; all other rows contain intermediate values of the aggregation calculation and should be ignored. This functionality is handled internally by `aggregate`.

> There are some cases where the intermediate values may be desired (e.g., computing a prefix sum, using `count` to assign identifiers, etc). In these cases, `aggregate` can be used separately from join and the validation mechanism can be disabled via an optional argument.

### Optimizations

The pseudocode presented above specifies two separate calls to `aggregate`. This was originally how `join` was implemented, but a later optimization combined these two calls into one. The functionality remains the same, however, and we keep them separate in the pseudocode for clarity of presentation.

## Aggregations

Most of the magic of join is hidden inside the `aggregate` function. This section will provide a high-level overview of its operation.

The `aggregations` argument to `join(...)` has type `AggregationSpec`, which is an alias for

```c++
std::vector<
    std::tuple<
        std::string, // Input column
        std::string, // Output column
        // Aggregation to run
        AggregationSelector<ASharedVector, BSharedVector>>>;
```

Because columns are dynamically typed, we can't know the function signature of the aggregation until runtime. Therefore, `AggregationSelector` is a helper class which can hold _either_ a `BSharedVector` function or an `ASharedVector` function (but not both), type-checked and selected at runtime. However, this dynamic selection is transparent to the user.

An aggregation specification might look something like this:

```c++
tj = t1.join(t2, {"[UID]"}, {
    {"COUNT",   "COUNT",   aggregators::count<A>},
    {"Amount",   "SUM_AMT", aggregators::sum<A>},
    {"Age",      "Age",     aggregators::copy<A>},
    {"[Zip]",    "[Zip]",   aggregators::copy<B>},
})
```

In this call, we group by column `[UID]` and
- put the count in `COUNT`
- sum the `Amount` column and put it in `SUM_AMT`
- copy the `Age` and `[ZIP]` columns from the left table (`t1`) into the output. (All data from the right is included by default, but we must explicitly copy data from the left.)

Since `COUNT` ignores its input data, the input column isn't actually too important. But a column of the same type must be specified (it is best to just use the output column for consistency).

Observe the inclusion of template arguments (`<A>`, `<B>`) on the aggregation functions: this argument must match the type annotations on the input and output columns. For example, calling

```c++
{"Age", "Age", join<B>}
```

would cause a runtime error.

### Available Aggregation

We have implemented the following aggregations:

- `sum`: templated so it can use `AShared` addition (local) or `BShared` (RCA, slow)
- `count`: implemented as a `sum` over a column of all 1s
- `min` (boolean only)
- `max` (boolean only)
- `copy`: copies rows from the left table in to the output
- `valid`: an internal version of `copy` used for updating the valid column (with an additional operation to ensure we never revalidate rows).

See [below](#writing-new-aggregations) for more information on implementation new aggregations.

### Vector Aggregation

The table aggregation function above extracts each vector from the table and then calls the underlying _vector_ aggregation function, which actually performs the operation. (The table aggregation function is mostly just a data transformation wrapper around the vector call.) Its function signature is:

```c++
void aggregate(
    vector<B> keys,
    vector<tuple<B, B, void (*)(B, B, B)>> &agg_spec_b,
    vector<tuple<A, A, void (*)(A, A, A)>> &agg_spec_a,
    bool reverse = false, optional<B> sel_b = {});
```

where `<B>` (resp. `<A>`) is a type alias for `BSharedVector<Share, EVector>`. (resp. `<ASharedVector<Share, EVector>>`).

Since vector aggregations are _compile-time_ type-checked, we need to specify B-shared and A-shared aggregations in separate arguments, as opposed to combined as above in the table case. This also means we don't need template arguments on the aggregation functions, because the compiler can infer the types:

```c++
secrecy::aggregators::aggregate(keys, {
    {db, r_max, secrecy::aggregators::max},
    {db, r_min, secrecy::aggregators::min},
    {db, r_jb,  secrecy::aggregators::copy},
}, {
    {da, r_sum, secrecy::aggregators::sum},
    {da, r_cnt, secrecy::aggregators::count},
    {da, r_ja,  secrecy::aggregators::copy},
});
```

The argument ordering is the same as above. This call means:
- group by vector keys specified in the vector `keys`
- perform the following _boolean_ aggregations
  - find the max of vector `db` and put the output in vector `r_max`
  - find the min and put the output in `r_min`
  - copy the values from `db` into `r_jb`
- perform the following _arithmetic_ aggregations
    - sum `da` into `r_sum`
    - put the count in `r_cnt`
    - copy the values from `da` into `r_ja`


> Note: this nested compile-time checking + template inference can get tricky to debug. A common error I've run into many times is passing in non-matching vector types; if this happens, the compiler can't correctly infer the values of `A` and `B`.

### Writing new aggregations

Due to the structure of our aggregation operation, implementing new functions is nontrivial. Consider the `sum` function:

```c++
// Aggregations should be templated. While in many cases functions may only 
// apply to BShared or AShared values, full templates make it easier to handle
// different datatypes (and we get handling of the other vector type for free). 
template<typename A>
// Arguments:
//  `group`: the grouping bits; either 0 or 1. These are generated in each
//      stage of the aggregation by comparing keys at different offsets.
//      Aggregation functions should be implemented with the assumption that
//      values from one group will have one binary value, and in the other
//      group, the other binary value.
//  `a`: the first vector, and the one that will be updated (hence, no `const`)
//  `b`: the other vector
void sum(const A& group, A& a, const A& b) {
    a = a + group * b;
}
```

`a` will usually be some _prefix_ of the input list, and `b` some suffix of equal size. However, the exact generation is not super important, as long as the function is incrementally computable and associative.

To see why the sum example works,
- rows with `group = 0` (different groups) will not be updated; `a = a`
- rows with `group = 1` (same group) will be updated to `a = a + b`

So, we add up all rows from the same group.

The `copy` aggregation operates in a similar manner:

```c++
template<typename T>
void join(const T& group, T& a, const T& b) {
    a = multiplex(group, a, b);
}
```

When `group = 1`, we copy `a = b`; otherwise, we leave `a = a`.

In general, we believe most aggregations should be implemented in such a way that `a` is _not_ updated when `group = 0`, but is updated (to some arbitrary function of `a` and `b`) when `group = 1`. For example, if one needed a `product` aggregation, it might be implemented as

```c++
a = a + group * (a * b - a)
```

(This expression can be rearranged into a call to `multiplex` but in this form uses one fewer oblivious multiplication.) In general, aggregations should look like

```c++
a = a + group * (f(a, b) - a)
```

### Edge cases

#### Count

`count` is a bit of a weird operator, since it fully ignores its input data: it only considers keys. To implement `count` we therefore need slightly special handling: `aggregate` checks if any of the aggregations are `count` (by checking the function pointer; yes, it's a bit sketchy), and if so, replaces the input with a vector of secret-shares ones.

#### Copy Aggregation

We also need special handling for `copy` In "regular" aggregations like count and sum, we _only_ aggregate over the rows from the right, ignoring anything in the primary-key (left) row. But for `copy`, we explicitly need to consider this row, since this is where we are copying from.

Therefore, for all `copy`s, we aggregate only over the keys provided by the user (and the `VALID` column). But for all other aggregations, we need to include the table ID. This conditional is again handled by a comparison to the relevant function pointer but should be transparent to the user.

#### Overwriting values
Do not use keys as aggregation outputs. You'll get weird behavior. It's maybe ok to use a key as an aggregation _input_ but you should be careful.

## Join Patterns

### Minimize sorting

Sorting is one of the most expensive oblivious operations, and in the current implementation dominates the runtime of every operation (including join) which uses it. As such, it is important to minimize sorting to the greatest extent possible.

1. **Remove unnecessary columns.** The asymptotic runtime of sort is dominated by the number of rows, but the runtime also increases by a constant amount for each additional column being sorted. (That is, there is a fixed cost up-front cost to generate the sorted permutation, and then the cost to apply the permutation to each column.) Removing unnecessary intermediate columns can significantly speed up sorting.

    > The move from bitonic sort to other more efficient algorithms may diminish the utility of this recommendation. But, for example, the current implementation took 75 seconds to sort a single column of 500k elements, but 150 seconds to sort 8 columns.

2. **Reuse sorts where possible**. It may not always be necessary to call `sort` again, if a recent aggregation or join has already ordered the table correctly. Aggregations operate in place and do not change the location of any elements. Additionally, if you are using a stable sorting algorithm (quicksort or radixsort), you may be able to partially reuse prior sorts - especially of the valid column.

### Trim tables when the size is known
Oblivious operations mean that we can't "filter" output by actually removing data: the cardinality of the output might then leak sensitive information. Therefore, all queries which (in a plaintext database) would have variable output size. Specifically, this is going to be whatever the *max* output length would be over all possible inputs; e.g., for filter operations, this is the length of the input; for joins, the sum of the lengths of the inputs.

Additionally, all table operations are (at least) linear in the number of rows. These two facts combined mean that over time our tables *never get smaller* and thus *always become slower to process*. In general, there's not much to be done about this, but when the size of some output (or at least an upper bound) is known, we can trim tables down with the `head` or `tail` functions and significantly speed up future computation.

A simple example: say you are running a join-aggregation over some column which has, at most, 10 values. After running the aggregation, the actual table returned will have size $L+R$, but you can trim the output (after sorting correctly) to just the first 10 rows.

> One counterargument: this trimming operation will necessarily require a sort, which is expensive. It may not always make sense to incur the cost of a full table sort just to throw away a few rows. See [this issue](https://github.com/CASP-Systems-BU/secrecy-private/issues/304) for more discussion.

### Never revalidate invalid rows
Rows should only move from valid (column `[#V] == 1`) to invalid (`== 0`). This rule is easy to follow if you only ever touch the valid column by calling `EncodedTable.updateValid(...)`, since that function just `&=`s its input with the valid column, so it can either invalidates or does nothing.

Invalidating a row is merely an oblivious way of "deleting" the row from the table, so invalid rows should be treated as if they do not exist. Revalidating invalid rows may lead to unexpected behavior, especially since all operations *continue to be executed* on invalid rows (again, due to obliviousness); the results are just never visible in the output.

### Don't include `VALID` in joins
The `join` function currently automatically handles the `VALID` column as an implicit key, so you shouldn't add it. However, other functions, like `aggregate` and `distinct`, are not yet `VALID`-aware, so you need to explicitly specify them there.


## Other kinds of joins

The discussion above only considers inner joins. However, other kinds of joins (outer, semi, anti) can be implemented with few changes. Primarily these changes entail how the `VALID` bit is updated. Otherwise the high-level join framework is shared across all variants. See `encoded_table.h:_join(...)` for more details.

### Left outer join
In a left outer join, all rows from the left are included, even if they have no matches on the right. Rephrased, we want to include both unique rows from the right (inner join) and non-unique rows from the left. We replace this line

```c++
concat["VALID_TEMP"] = concat["VALID"] & ~concat.distinct(keys)
```

with

```c++
concat["VALID_TEMP"] = concat["VALID"] & (concat[ENC_TABLE_JOIN_ID] ^ concat.distinct(keys));
```

### Right outer
A parallel construction, where all rows from the right remain even if they have no matching PK on the left. In this case we ignore the `distinct` operation and
only have to consider the table ID.

```c++
concat["VALID_TEMP"] = concat["VALID"] & concat[ENC_TABLE_JOIN_ID];
```

A technical detail here is that we also do not aggregate the `VALID_TEMP` column for a right outer join, because we do not want to invalidate additional rows. This is also true for the full outer join, next.

### Full outer
All rows remain. We do not update the `VALID` bit at all.

### Semi-join
In a semi-join, only rows on the left matching rows on the right are returned. This can be implemented via an inner join by changing the order of arguments. Specifically, to implement `LEFT semi join RIGHT`, we execute `RIGHT inner join LEFT`. Assuming `RIGHT` has unique keys, this is equivalent. (We must also `project` to keep only those columns from `LEFT`)

### Anti-join
In an anti-join, we want to perform the _opposite_ operation, only keeping non-matching rows. It turns out that this is very similar to a right outer join run in the opposite direction (again assuming `RIGHT` has unique keys).

To implement `LEFT anti join RIGHT`, we call `RIGHT right outer join LEFT` with an additional flag set. This flag tells right outer join to slightly modify its validity-update procedure (using `copy<B>` instead of `valid<B>`). This is required because `valid<>` disallows the _revalidation_ of invalid rows. However, in an anti-join, we are performing an inherently inverted operation, so in this case revalidation is unfortunately required. A `copy<>` aggregation allows this to occur.