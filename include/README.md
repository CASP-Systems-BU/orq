The `include/` directory is the primary entry-point for the library’s public C++ headers.

- `core/` – Core data structures, cryptographic protocols, random generators, and communication primitives.
- `backend/` – Runtime and network (MPI/TCP) service implementations.
- `debug/` – Compile-time assertions and run-time debugging utilities.
- `profiling/` – Micro-benchmark helpers, profiling timers, and performance measurement utilities.
- `orq.h` – Convenience umbrella header that aggregates the most frequently used components. **Should be included by C++ programs.**
- `mpc.h` – High-level header that includes all types, protocols, and helper utilities. 