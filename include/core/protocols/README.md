The `protocols/` directory contains the MPC protocol implementations supported by ORQ.

Contents:

- `dummy_0pc.h` – Dummy 0-party protocol (mock only, not functional, useful for testing).
- `plaintext_1pc.h` – Plaintext 1-party protocol (no privacy, useful for testing).
- `beaver_2pc.h` – Dishonest-majority 2-party protocol using Beaver triples.
- `replicated_3pc.h` – Replicated secret sharing 3-party protocol.
- `dalskov_4pc.h` – Dalskov et al. Fantastic-Four 4-party protocol.
- `custom_4pc.h` – Rewrite of Fantastic 4PC to be round efficient.
- `dummy_0pc.h` – Dummy 0-party protocol (mock only, not functional).
- `plaintext_1pc.h` – Plaintext 1-party protocol (no privacy, useful for testing and debugging, but could be used inside of a TEE).
- `replicated_3pc.h` – Replicated secret sharing 3-party protocol.
- `protocol.h` – Base protocol interface.
- `protocol_factory.h` – Factory that instantiates protocol objects.