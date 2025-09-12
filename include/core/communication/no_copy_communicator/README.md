The `no_copy_communicator/` directory provides a custom high-performance communicator that minimises memory copies during message passing.

Contents:

- `no_copy_communicator.h` – Zero-copy communicator implementation.
- `no_copy_communicator_factory.h` – Factory for building zero-copy communicators.
- `no_copy_ring.h` – Ring buffer used by the zero-copy communicator. 