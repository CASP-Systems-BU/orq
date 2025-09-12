The `operators/` directory implements MPC-enabled data-parallel operators used by ORQ queries.

Contents:

- `aggregation_selector.h` - Internal helper class to select between aggregation types at runtime
- `aggregation.h` – Secure aggregation functions (SUM, COUNT, etc.) and oblivious aggregation network
- `circuits.h` - Implementation of various boolean circuits.
- `common.h` – Common utilities shared by operators.
- `distinct.h` – Distinct operator.
- `join.h` - Join operator.
- `merge.h` – Oblivious Merge.
- `quicksort.h` – Quicksort implementation.
- `radixsort.h` – Radixsort implementation.
- `shuffle.h` – Oblivious shuffle operators.
- `sorting.h` – Sorting helper functions.
- `streaming.h` – Streaming operators. 