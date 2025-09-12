The `common/` directory provides general runtime facilities shared by all ORQ services.

Contents:

- `setup.h` – Parses command-line flags and sets up the runtime environment.
- `rand_setup.h` – Randomness setup helpers.
- `benchmark.h` – Micro-benchmark harness for service components.
- `task.h` – Work unit abstraction.
- `worker.h` – Worker thread implementation.
- `runtime.h` – Central runtime object orchestrating tasks, memory, and communication. 