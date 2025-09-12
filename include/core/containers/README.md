The `containers/` directory provides vector-like container abstractions used to store plaintext and secret-shared values within ORQ.

Contents:

- `a_shared_vector.h` – Arithmetic shared vector container.
- `b_shared_vector.h` – Boolean shared vector container.
- `e_vector.h` – Vector-of-vector wrapper for replicated sharing schemes.
- `encoded_vector.h` – Generic encoded vector implementation.
- `encoding.h` – Encoding trait helpers.
- `mapped_iterator.h` – Iterator adaptor for custom containers.
- `dummy_vector.h` – Dummy vector for tests and benchmarks.
- `mapping_access_vector.h` – Mapping access vector implementation.
- `permutation.h` – Container for local and secret-shared permutations.
- `shared_vector.h` – Abstract (untyped) secret-shared vector implementation.
- `vector.h` – Convenience alias to the default vector type.
- `tabular/` - Encoded Table and Column classes.
