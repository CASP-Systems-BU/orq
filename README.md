# ORQ

ORQ is a multi-party computation framework for relational analytics. For more information, see our paper at SOSP '25.

This repository is organized as follows:
- `docs/`: miscellaneous documentation (see `doxygen` for full documentation)
- `examples/`: input data for examples
- `include/`: core functionality of ORQ (`core/`), including the implementation of MPC primitives (`core/protocols/`), relational oblivious operators (`core/operators`; `relational/`), and party communication (`core/communication/`; `service/`).
- `scripts/`: various scripts for testing and benchmarking ORQ
- `src/`: source code for queries and benchmarks
- `tests/`: the test suite

## Artifact Evaluation

See [here](./scripts/sosp25/README.md) for instructions on artifact replication.

## Building ORQ

> [For artifact evaluator] You do not need to follow these instructions to build ORQ. The artifact evaluation README has all the information you need.

There are separate instructions for single-node and multi-node systems. All instructions assume \*nix systems.

### Single-Node

The following applies to computation run on a single machine, where each party is a separate process. On platforms with `apt` (Ubuntu), simply run the setup script:

```bash
$ git clone https://github.com/CASP-Systems-BU/orq
$ cd orq
$ ./scripts/setup.sh
```

If you do not have `apt` (other Linux, macOS, etc.) you will need to install the
dependencies manually. As of this writing, that list (using their `apt` package names) is:

```
git cmake pkg-config build-essential manpages-dev gfortran wget libsqlite3-0 libsqlite3-dev libsodium23 libsodium-dev libopenmpi3 libopenmpi-dev openmpi-bin openmpi-common python3 python3-pip libtool autoconf automake
```

### Multi-Node

> [For artifact evaluator] The hostfile has already been updated on the cluster you have been given. The artifact evaluation README discusses how to run the `deploy.sh` script.

In a distributed deployment with multiple nodes, there is an additional setup to connect the nodes. First, we run `scripts/_update_hostfile.sh` to write to `/etc/hosts/` so that we can refer to the other servers as `node0` through `node1/2/3` (for 2PC, 3PC, 4PC), or any other naming scheme. Second, we run the deployment script `scripts/orchestration/deploy.sh` with the location of the repository and the names of the servers. The following example assumes a four-party setup, although we can achieve a setup with fewer parties by simply not entering the corresponding IP address(es) and name(s). Replace `<ip-X>` with the IP address of `nodeX`.

```bash
$ git clone https://github.com/CASP-Systems-BU/orq
$ cd orq
$ ./scripts/_update_hostfile.sh -x node -i <ip-0>,<ip-1>,<ip-2>,<ip-3>
$ ./scripts/orchestration/deploy.sh ~/orq node0 node1 node2 node3
```

`deploy.sh` runs `setup.sh`, so we do not need to run `setup.sh` explicitly. If you are on a machine without `apt`, you will need to install dependencies manually, as specified in the single-node section.

### Dependencies

> [For artifact evaluator] All dependency installation is handled by the `deploy.sh` script run either in the Functional section of the artifact evaluation README or automatically by some of the experiment scripts. You do not need to do anything to install dependencies.

All dependencies are installed automatically with the `setup.sh` or `deploy.sh` scripts described above.

To build ORQ, you will need:
- C++ 20 or newer
- [libsodium](https://libsodium.gitbook.io/doc/installation)
- `sqlite`, for optional correctness tests for queries.

Not all dependencies are required for all operations. For the offline phase of the two-party setting, we have the following additional dependencies.

- [libOTe](https://github.com/osu-crypto/libOTe) for secure Beaver triples
- [secure-join](https://github.com/Visa-Research/secure-join) for a two-party oblivious pseudorandom function (OPRF) with secret-shared output

> Note: for artifact replication, we do not install these dependencies, since we only present benchmarks of the online phase.

> Note: the above dependencies may impose additional restrictions on the environment. For example, they may not support all Linux distributions that ORQ's online phase supports.

You should still run `setup.sh` even if you install the dependencies manually, because other libraries will be compiled and built manually.

## Creating a Cluster

> [For artifact evaluator] This has already been done for you. We will give you access to an already created AWS cluster.

We provide instructions for launching a cluster both on AWS and on CloudLab. For AWS, we provide a cluster launching script, and for CloudLab, we provide simple step-by-step instructions. Of course, other types of clusters will work, but we do not provide an automated process for creating them.

### AWS

To deploy an AWS cluster, you will need an AWS access key and secret key. This is often already stored inside `~/.aws/credentials`, in which case no action is needed. If it is not (or if you do not wish to create such a file), enter your access key and secret key into `scripts/orchestration/aws/secrets.sh` and run the following command before proceeding.

```bash
$ source scripts/orchestration/aws/secrets.sh
```

To create the cluster, run the following command.

```bash
$ ./scripts/orchestration/aws/deploy-aws-cluster.sh <cluster-name> <number-of-nodes>
```

You will now have access to a cluster with your specified number of nodes. To setup the repository, follow the instructions in the [section](#building-orq) about building ORQ.

### CloudLab

From the main page in CloudLab, execute the following operations.

```
click the 'Experiments' dropdown in the upper left and select 'Start Experiment'
click 'Next'
enter your desired number of nodes (2, 3, or 4)
select either Ubuntu 22 or Ubuntu 24 as your operating system
select any Intel-based server type
click 'Next'
give the cluster an arbitrary name
use the default node names (node0, node1, ...)
click 'Next'
click 'Finish'
```

When the cluster has been successfully created, you can SSH into `node0` and follow the instructions in the [section](#building-orq) about building ORQ. Note that on CloudLab, you **do not need** to run the `_update_hostfile.sh` script, as CloudLab has already configured the servers to recognize each other as `node0`, `node1`, and so on.

## Running ORQ

This section describes the process to compile and run programs. To run programs, you must be in the `build` directory. You can follow standard standard `cmake` patterns for compiling ORQ programs, and we provide commands for running programs in a distributed or local fashion.

### Compiling ORQ Programs

The following example compiles a single program, TPCH Q1, in the replicated three-party protocol (the default).

```bash
$ mkdir build
$ cd build
$ cmake .. -DPROTOCOL=3
$ make q1
```

We provide some `cmake` shortcuts to make compiling multiple executables easier.

```bash
# compile everything
$ make -j
# compile tests
$ make -j tests-only
# compile all tpch queries
$ make -j tpch-queries
# compile queries for the secretflow comparison
$ make -j secretflow-queries
# compile all other queries
$ make -j other-queries
```

Various options can be specified to `cmake`.
- `-DPROTOCOL=N` to change the protocol. We currently support
   - `-DPROTOCOL=1` a single-party plaintext test protocol
   - `-DPROTOCOL=2` two party dishonest majority protocol with Beaver Triples
   - `-DPROTOCOL=3` three party replicated honest majority protocol (the default)
   - `-DPROTOCOL=4` [Fantastic Four](https://eprint.iacr.org/2020/1330) honest-majority malicious 4PC protocol
- `-DNO_X86_SSE=1` to disable x86 hardware optimizations (you will get warnings otherwise if built on ARM platforms, like newer Macs)
- `-DPROFILE=1` enable profiling (compile with `-pg`)
- `-DEXTRA=XXX` pass the additional argument `XXX` to `make`
- `-DCOMM=XXX` enable the given communicator. Options are `"MPI" "NOCOPY"`. The default is `NOCOPY`.

See `debug/debug.h` and `CMakeLists.txt` for more information on compile options.

### Running ORQ Programs Locally

Run programs using the `startmpc` or `run_experiment` script. If you are using the MPI communicator, replace `startmpc` with `mpirun`.

First, we describe the process of running computation with `startmpc`. The `run_experiment` script automates much of this process.

We begin with an example on a single machine with multiple processes.

```bash
# run cmake and make the target
$ cmake .. -DPROTOCOL=3
$ make q1
# run the program with three parties as three separate processes on the same machine
$ startmpc -n 3 ./q1
# or run with additional parameters - 2 thread, batch size 16384, and scale factor 0.001
$ startmpc -n 3 ./q1 2 1 16384 0.001
```

### Running Tests

Tests can be compiled and run like any other program. Below we compile all tests and run a select few.

```bash
$ cmake .. -DPROTOCOL=3
$ make -j tests-only
$ startmpc -n 3 ./test_primitives
$ startmpc -n 3 ./test_sorting
```

### Running ORQ Programs on Multiple Servers

To run an ORQ program on multiple servers, we need to make sure all parties have a copy of the compiled program.

```bash
$ cmake .. -DPROTOCOL=4
$ make q2
# copy the q2 binary to all other parties using `scp`
$ scp ./q2 node1:~/orq/build/
$ scp ./q2 node2:~/orq/build/
$ scp ./q2 node3:~/orq/build/
# run the program
$ startmpc -n 4 -h node0,node1,node2,node3 ./q2
```

This entire process can be automated using the `run_experiment.sh` script. For example,
```bash
$ ../scripts/run_experiment.sh -p 4 -s lan -c nocopy -n 4 -f 1 -T 16 -x node q5
```
will compile and run TPCH query `q5` at Scale Factor 1, under 4PC LAN, with `4` nocopy communicator threads, 16 worker threads, and with servers named `node0` through `node3`.

To see a comprehensive set of options for the `run_experiment` script, simply run it without arguments to display a help message.

```bash
$ ../scripts/run_experiment.sh
Usage: ../scripts/run_experiment.sh [options] <exp_name>
  exp_name Experiment to run
OPTIONS:
  [-h]                                    Show this help
  [-p 1|2|3|4]                            Protocol, defaults to 3
  [-s same|lan|wan]                       Setting, defaults to same
  [-c mpi|socket|nocopy]                  Communicator, defaults to mpi
  [-n num_comm_threads]                   [NoCopyComm only] Number of communicator threads, defaults to -1 (1 comm thread per app thread)
  [-r min_rows_pow[-max_rows_pow]]        Number of rows, as powers of 2, can be a range, defaults to 10-20
  [-d]                                    Use powers of 10 for the number of rows flag (-r)
  [-f scale_factor]                       Scale factor for TPC-H and other queries. Overrides -r if set.
  [-t min_threads_pow[-max_threads_pow]]  Number of threads, as powers of 2, can be a range, defaults to 0 (1 thread)
  [-T threads]                            Number of threads (arbitrary)
  [-b batch_size]                         Batch size.
  [-e exp_repetitions]                    Number of times to repeat each rows/threads pairing, defaults to 1
  [-m cmake_args]                         Pass additional arguments to cmake (can be repeated for more)
  [-a experiment_args]                    Pass additional arguments to the experiment binary (can be repeated for more)
  [-x node prefix]                        Prefix for remote nodes. Default is 'machine-' or 'machine-wan-'.
```

## Citation

```
@inproceedings{orq,
  author    = {Eli Baum and Sam Buxbaum and Nitin Mathai and Muhammad Faisal and Vasiliki Kalavri and Mayank Varia and John Liagouris},
  title     = {ORQ: Complex analytics on private data with strong security guarantees},
  booktitle = {Proceedings of the 29th ACM Symposium on Operating Systems Principles (SOSP '25)},
  year      = {2025},
  publisher = {Association for Computing Machinery}
}
```
