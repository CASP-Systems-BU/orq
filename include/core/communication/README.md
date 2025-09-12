The `communication/` directory abstracts how ORQ parties exchange messages and offers multiple back-ends.

Contents:

- `no_copy_communicator/` – Zero-copy communicator implementation.
- `communicator.h` – Abstract communicator interface.
- `communicator_factory.h` – Factory for constructing communicators.
- `mpi_communicator.h` – MPI communicator back-end.
- `null_communicator.h` – Null communicator for single-process or testing setups.
- `ring.h` - definitions for Ring and RingEntry structs.