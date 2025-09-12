The `core/` directory houses the fundamental building blocks of ORQ, including container types, communication primitives, MPC protocols, secure operators, and random generators.

Below is an overview of the immediate sub-directories:

- `containers/` – Plaintext, encoded, and secret-shared containers
- `communication/` – Abstractions and concrete back-ends (MPI and no-copy) for inter-party messaging.
- `operators/` – Data-parallel MPC operators such as shuffle, sort, aggregation, etc.
- `protocols/` – Implementations of the MPC protocols supported by ORQ (Beaver 2PC, Replicated 3PC, and Fantastic 4PC).
- `random/` – Randomness and correlation generators (PRGs, triples, permutations) used by the protocols. 