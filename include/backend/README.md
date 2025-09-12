The `backend/` directory bridges ORQ’s core MPC logic with runtime services such as task scheduling, networking, and memory management.

Sub-directories & files:

- `common/` – General-purpose runtime, task, and benchmark helpers.
- `null_communicator/` – Null service setup
- `nocopy_communicator/` – TCP-based service setup
- `service.h` – Umbrella header for all service components. 