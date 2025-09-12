The `scripts/` directory aggregates helper scripts for setting up ORQ’s dependencies, running experiments, and visualizing results.

Sub-directories:
- `comm/` - communication scripts, including WAN simulation
- `competitors/` - scripts for running competitors in the ORQ paper
- `orchestration/` - cluster setup, AWS
- `plot/` - plotting scripts
- `profiling/` - profiling scripts
- `sosp25-replication` - directory for SOSP Artifact Evaluation
- `testing/` - test helpers

Top-level scripts:
- `setup.sh` – One-shot installer for ORQ’s external dependencies; calls:
    - `_setup_required.sh` – `apt install` required dependencies
    - `_setup_libote.sh` – fetches and builds libOTe
    - `_setup_securejoin.sh` – fetches and builds secureJoin
- `_update_hostfile.sh` – Updates the hostfile for multi-node runs.
- `run_experiment.sh` – Generic wrapper that compiles and executes a program.
- `query-experiments.sh` – Helper for query benchmarks.
- `compare-to.py` – Compares two branches' performance.