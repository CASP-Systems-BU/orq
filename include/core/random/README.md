The `random/` directory provides all randomness and correlation generators required by ORQ’s protocols.

Below is an overview of its immediate contents:

- `permutations/` – Families of permutation generators for sharded permutations.
- `pooled/` – A pooled randomness wrapper generator that allows for separating generation from retrieval.
- `correlation/` - correlation generators, including beaver triples, OPRFs, and OTs
- `prg/` - local and shared PRG interfaces