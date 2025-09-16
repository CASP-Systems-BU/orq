# ORQ: An Oblivious Relational Query Engine

ORQ is a multi-party computation framework for relational analytics. For more information, see our paper at SOSP '25.

- [Paper](https://arxiv.org/abs/2509.10793)
- [Documentation](https://casp-systems-bu.github.io/orq/)
- [Our Lab](https://sites.bu.edu/casp/)

> ORQ has received all SOSP artifact evaluation badges. Evaluation occurred on the `sosp-artifact` tag, available [here](https://github.com/CASP-Systems-BU/orq/tree/sosp-artifact). The current version of the repository has gone through a substantial cleanup for public release, but is functionally identical.
> 
> ORQ is also archived on Zenodo:
>
> [![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.17027577.svg)](https://doi.org/10.5281/zenodo.17027577)

## Table of Contents

- [Dependencies](#dependencies)
- [Building ORQ](#building-orq)
  - [Single-Node](#single-node)
  - [Multi-Node](#multi-node)
- [Running ORQ](#running-orq)
  - [Compiling ORQ Programs](#compiling-orq-programs)
  - [`run_experiment.sh`](#run_experimentsh)
  - [Running ORQ Programs Locally](#running-orq-programs-locally)
  - [Running ORQ Programs on Multiple Servers](#running-orq-programs-on-multiple-servers)
  - [Running ORQ Programs in Simulated WAN](#running-orq-programs-in-simulated-wan)
  - [Running the Test Suite](#running-the-test-suite)
  - [Running Queries](#running-queries)
- [Writing New ORQ Programs](#writing-new-orq-programs)
- [Creating a Cluster](#creating-a-cluster)

This repository is organized as follows:
- `bench/`: source code for queries and benchmarks
- `doc/`: documentation (available online, above)
- `examples/`: example programs
- `include/`: core functionality of ORQ (`core/`), including the implementation of MPC primitives (`core/protocols/`), containers (`core/containers/`) relational oblivious operators (`core/operators/`), and party communication (`core/communication/`), as well as the framework backend (`backend/`).
- `scripts/`: various scripts for testing and benchmarking ORQ
- `tests/`: the test suite

This README walks through each of the steps to deploy an ORQ cluster and run ORQ programs. You can also get started with [writing ORQ programs](#writing-new-orq-programs) and check out the [examples](https://github.com/CASP-Systems-BU/orq/tree/main/examples).

## Dependencies

All dependencies are installed automatically on Ubuntu by running the [`setup.sh`](./scripts/setup.sh) or [`deploy.sh`](./scripts/orchestration/deploy.sh) scripts.

To build ORQ, you will need:
- C++ 20 or newer
- [libsodium](https://libsodium.gitbook.io/doc/installation)
- Your platform's threading library, such as `pthread` (should be installed by default)
- `sqlite`, for optional correctness tests for queries. Ideally, you should install SQLite via your system's package manager, but it is also available on their [website](https://www.sqlite.org/index.html).

As of this writing, the complete list of ORQ dependencies is:

```
git cmake pkg-config build-essential manpages-dev gfortran wget libsqlite3-0 libsqlite3-dev libsodium23 libsodium-dev libopenmpi3 libopenmpi-dev openmpi-bin openmpi-common python3 python3-pip libtool autoconf automake
```

These are automatically installed by `setup.sh`. On non-Ubuntu systems (Mac, other \*nix), `setup.sh` will not be able to install all dependencies, so you will need to install them yourself.


If you plan to use ORQ in the two-party setting, you will also need to install the following libraries for the offline phase:

- [libOTe](https://github.com/osu-crypto/libOTe) for secure Beaver triples
- [secure-join](https://github.com/Visa-Research/secure-join) for a two-party oblivious pseudorandom function (OPRF) with secret-shared output

> Note: the above dependencies may impose additional restrictions on the environment. For example, they may not support all Linux distributions that ORQ's online phase supports.

> [!IMPORTANT]
> You should still run `setup.sh` even if you install the dependencies manually, because other libraries will be compiled and built manually. You can ignore errors from, e.g. `apt` not being installed.

## Building ORQ

There are separate instructions for single-node and multi-node systems. All instructions assume \*nix systems.

### Single-Node

The following applies to computation run on a single machine, where each party is a separate process. On platforms with `apt` (Ubuntu), simply run the setup script:

```bash
$ git clone https://github.com/CASP-Systems-BU/orq
$ cd orq
$ ./scripts/setup.sh
```

If you do not have `apt` (other Linux, macOS, etc.) you will need to install the
dependencies manually (see [above](#dependencies)).

### Multi-Node

> [!NOTE]
> These instructions assume a cluster has already been created. For instructions on how to create a cluster, see [below](#creating-a-cluster).

In a distributed deployment with multiple nodes, there is an additional setup to connect the nodes. **All commands in this section should be run from `node0`.** `node0` _must_ be able to SSH into all other nodes.

> [!WARNING]
> Having SSH access into all other nodes, of course, compromises the non-collusion assumption of multi-party computation. We require SSH access here for ease of development and testing. However, in a real system, ORQ nodes would need to be configured out-of-band. We do not currently support this setup.
>
> Connections between parties do _not_ use TLS for MPC. SSH is only used for setup.

- First, we run `scripts/_update_hostfile.sh` to write to `/etc/hosts/` so that we can refer to the other servers as `node0`, `node1`, etc.
- Second, we run the deployment script `./scripts/orchestration/deploy.sh` with the location of the repository and the names of the servers. The following example assumes a four-party setup, although we can achieve a setup with fewer parties by simply not entering the corresponding IP address(es) and name(s). Replace `<ip-X>` with the IP address of `nodeX`.

```bash
$ git clone https://github.com/CASP-Systems-BU/orq
$ cd orq
$ ./scripts/_update_hostfile.sh -x node -i <ip-0>,<ip-1>,<ip-2>,<ip-3>
$ ./scripts/orchestration/deploy.sh ~/orq node0 node1 node2 node3
```

`deploy.sh` runs `setup.sh`, so we do not need to run `setup.sh` explicitly. If you are on a machine without `apt`, you will need to install dependencies manually **on all machines**, as specified [above](#dependencies).

## Running ORQ

This section describes the process to compile and run programs. To run programs, you must be in the `build` directory. You can follow standard `cmake` patterns for compiling ORQ programs, and we provide commands for running programs in a local or distributed fashion. Compilation is the same in either case.

### Compiling ORQ Programs

The following example compiles a single program, [TPCH Q1](https://github.com/CASP-Systems-BU/orq/blob/main/bench/queries/tpch/q1.cpp), in the replicated three-party protocol (the default).

```bash
$ mkdir build
$ cd build
$ cmake .. -DPROTOCOL=3
$ make q1
```

Various options can be specified to `cmake`.
- `-DPROTOCOL=N` to change the protocol. We currently support
   - `-DPROTOCOL=1` a single-party plaintext test protocol
   - `-DPROTOCOL=2` [ABY](https://www.ndss-symposium.org/ndss2015/ndss-2015-programme/aby-framework-efficient-mixed-protocol-secure-two-party-computation/) two party dishonest majority protocol with Beaver Triples
   - `-DPROTOCOL=3` [Araki et al.](https://eprint.iacr.org/2016/768) three party replicated honest majority protocol (the default)
   - `-DPROTOCOL=4` [Fantastic Four](https://eprint.iacr.org/2020/1330) honest-majority malicious 4PC protocol
- `-DNO_X86_SSE=1` to disable x86 hardware optimizations (you will get warnings otherwise if built on ARM platforms, like newer Macs)
- `-DPROFILE=1` enable profiling (compile with `-pg`)
- `-DEXTRA=XXX` pass the additional argument `XXX` to `make`
- `-DCOMM=XXX` enable the given communicator. Options are `"MPI" "NOCOPY"`. If you do not specify anything, CMake will use `NOCOPY`.
- `-DTRIPLES=XXX` specify the kind of Beaver triples to use for 2PC (`ZERO` (all zeros, for profiling the online phase), `DUMMY` (insecurely generated, fast), or `REAL` (secure)).

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

See `debug/orq_debug.h` and `CMakeLists.txt` for more information on compile options. Not all compile options are made available via CMake and instead must be manually configured within `orq_debug.h`.

### `run_experiment.sh`

We provide an execution harness script, `run_experiment.sh`, which automates the compilation and execution process. **This is the recommended method of running ORQ programs.**

To see a comprehensive set of options for the `run_experiment` script, simply run it without arguments to display a help message.

```bash
Usage: ../scripts/run_experiment.sh [options] <exp_name>
  exp_name Experiment to run
OPTIONS:
  [-h]                                    Show this help
  [-p 1|2|3|4]                            Protocol; default: 3
  [-s same|lan|wan]                       Setting; default: same
  [-c mpi|nocopy]                         Communicator; default: mpi for same; nocopy otherwise"
  [-n num_comm_threads]                   [NoCopyComm only] Number of communicator threads (negative: # per worker); default: -1
  [-r min_rows_pow[-max_rows_pow]]        Number of rows, as powers of 2, can be a range; default: 20
  [-d]                                    Use powers of 10 for the number of rows flag (-r)
  [-f scale_factor]                       Scale factor for TPC-H and other queries. Overrides -r if set.
  [-t min_threads_pow[-max_threads_pow]]  Number of threads, as powers of 2, can be a range
  [-T threads]                            Number of threads (arbitrary); default: 1
  [-b batch_size]                         Batch size; default: -12
  [-e exp_repetitions]                    Number of times to repeat each rows/threads pairing; default: {exp_repetitions}
  [-m cmake_args]                         Pass additional arguments to cmake (can be repeated for more)
  [-a experiment_args]                    Pass additional arguments to the experiment binary (can be repeated for more)
  [-x node prefix]                        Prefix for remote nodes. Machines are prefix0, prefix1, ...; default: node
```

### Running ORQ Programs Locally

For testing and development, you may want to run ORQ programs locally. In this setting, separate processes act as each node of the multi-party computation.

A minimal test:

```bash
$ cd build
$ ../scripts/run_experiment.sh test_primitives
# ... everything should pass ...
```

This command will run the program `test_primitives.cpp` with all default options:
- Replicated 3PC
- `same` environment
- MPI communicator
- 1 thread
- etc.

> [!TIP]
> We recommend always using `mpi` for local (`-s same`) tests. The no-copy communicator performs poorly over the loopback interface.

We can try a different program:
```bash
$ ../scripts/run_experiment.sh micro_primitives    
# ...
Vector 1048576 x 32b
[=SW]            Start
[ SW]      Reserve MUL 1.625e-06 sec
[ SW]      Reserve AND 4.25e-06 sec
[ SW]              AND 0.01723  sec
[ SW]             MULT 0.01167  sec
[ SW]               EQ 0.04605  sec
[ SW]               GR 0.07226  sec
[ SW]              RCA 0.1298   sec
[ SW]             RCA< 0.05846  sec
[ SW]      Dot Product 0.008903 sec
```

More rows ($2^{24}\approx 16\mathrm{M}$):
```bash
$ ../scripts/run_experiment.sh -r 24 micro_primitives
# ...
Vector 16777216 x 32b
[=SW]            Start
[ SW]      Reserve MUL 9.59e-07 sec
[ SW]      Reserve AND 3.541e-06 sec
[ SW]              AND 0.1686   sec
[ SW]             MULT 0.1992   sec
[ SW]               EQ 1.14     sec
[ SW]               GR 1.499    sec
[ SW]              RCA 1.913    sec
[ SW]             RCA< 0.8353   sec
[ SW]      Dot Product 0.1913   sec
```

A different protocol (Malicious-secure Fantastic 4PC):

```bash 
$ ../scripts/run_experiment.sh -r 24 -p 4 micro_primitives
# ...
Vector 16777216 x 32b
[=SW]            Start
[ SW]      Reserve MUL 1.167e-06 sec
[ SW]      Reserve AND 3.958e-06 sec
[ SW]              AND 1.126    sec
[ SW]             MULT 1.109    sec
[ SW]               EQ 6.42     sec
[ SW]               GR 8.01     sec
[ SW]              RCA 4.401    sec
[ SW]             RCA< 2.709    sec
[ SW]      Dot Product 1.449    sec
```

Or more threads:

```bash
$ ../scripts/run_experiment.sh -r 24 -p 4 -T 4 micro_primitives
# ...
Vector 16777216 x 32b
[=SW]            Start
[ SW]      Reserve MUL 1.421e-05 sec
[ SW]      Reserve AND 3.875e-06 sec
[ SW]              AND 0.7819   sec
[ SW]             MULT 0.5872   sec
[ SW]               EQ 3.194    sec
[ SW]               GR 4.201    sec
[ SW]              RCA 2.858    sec
[ SW]             RCA< 2.194    sec
[ SW]      Dot Product 0.8624   sec
```

You should not expect much of a speedup with more threads when running locally: this is the number of worker threads _per party_. There is also a main thread per party. In the above example, therefore, we have 4 parties, each with $4+1=5$ threads, but our test machine only has 8 cores.

### Running ORQ Programs on Multiple Servers

ORQ programs can be run over LAN just by changing the setting (`-s`) argument to `run_experiment.sh`:

```
$ ../scripts/run_experiment.sh -s lan -c nocopy -n 4 -T 8 micro_sorting
```

This runs the `micro_sorting` experiment, with the 3PC protocol (the default), on nodes `node0`, `node1`, and `node2`. We use the `nocopy` communicator with `4` communication threads and `8` worker threads. `run_experiment.sh` takes care of configuring the other nodes, and copies the compiled binary from `node0` to all other nodes in the cluster. 

> [!WARNING]
> ORQ programs will crash in mysterious ways if different versions of a binary are present on different hosts.

> [!TIP]
> We recommend using `-c nocopy` for LAN tests. We have found `-n 4` (4 communication threads) sufficient for the LAN environment.
>
> If your machines are named something else, you can specify a new prefix with `-x [node]`. However, our scripts assume a consistent numbering:
> 
> ```bash
> $ ../scripts/run_experiment.sh -s lan -p 4 -x lab-server- test_primitives
> ```
> 
> This will run `test_primitives`, with the 4PC protocol, on nodes `lab-server-0`,`lab-server-1`, `lab-server-2`, and `lab-server-3`.

To check the scaling behavior of ORQ, we can use the variable-thread (`-t`) argument. This example will run `micro_sorting` in LAN, with the 3PC protocol, using `1, 2, 4, 8, 16, 32` worker threads.

```bash
$ ../scripts/run_experiment.sh -s lan -c nocopy -n 4 -t 0-5 micro_sorting
```

### Running ORQ Programs in Simulated WAN

ORQ demonstrates practical performance over the internet (WAN). While we show true WAN experiments in the paper, it is tricky to deploy a geodistributed WAN cluster. For simple tests, we use simulated WAN, which uses the Linux `tc qdisc` command to simulate high-latency, low-bandwidth network conditions.

The WAN simulator is very easy to use:

```bash
# Assuming pwd = ./build
# Turn on simWAN for node0 (implied), node1, node2, and node3
$ ../scripts/comm/cluster-wan-sim.sh on node{1,2,3}
# Run your experiment
# [Soon, run_experiment will handle running cluster-wan-sim for you]
$ ../scripts/run_experiment.sh -s wan ...
# Disable simWAN
$ ../scripts/comm/cluster-wan-sim.sh off node{1,2,3}
```

This simple script makes some assumptions about the network topology (e.g., that all nodes are routable over the same interface), so modifications may be required for more complex deployments.

`cluster-wan-sim` also runs `ping` just as a sanity check:

```bash
# from node0
$ ping -q -c 3 node1
# ...
rtt min/avg/max/mdev = 0.177/0.235/0.270/0.041 ms
$ ./scripts/cluster-wan-sim.sh on node{1,2,3}
# ...
Enabling WAN on bond0@ 6Gbit, 10ms
# ...
# Latency has increased:
rtt min/avg/max/mdev = 20.321/20.394/20.446/0.053 ms
# ...
$ ./scripts/cluster-wan-sim.sh off node{1,2,3}
# ...
# Back to normal:
rtt min/avg/max/mdev = 0.150/0.200/0.258/0.044 ms
```

You can use `iperf` to check the bandwidth limiter.

### Running the Test Suite

Tests can be compiled and run like any other program. Tests can be run in LAN, but this just slows things down without exercising much functionality, so we recommend running tests locally. We provide a script to automate running the entire test suite.

The syntax is:

```
../scripts/testing/run_multithread_test.sh [protocol=3] [threads=1]
```

For example:

```bash
# Run the test suite under 3PC with 1 thread...
$ ../scripts/testing/run_multithread_test.sh
# 4PC and 2 threads...
$ ../scripts/testing/run_multithread_test.sh 4 2
# 2PC and 1 thread
$ ../scripts/testing/run_multithread_test.sh 2
```

### Running Queries

Queries (`/bench/queries`) can be run in the same way as other programs, but we also provide a test harness to run an entire suite of queries.

```bash
$ ../scripts/query-experiments.sh 
Usage: ../scripts/query-experiments.sh <tpch|other|secretflow> <sf> <protocol> <threads> <enviro=lan> <query_range=1..22>
```

The first argument specifies the suite of queries:
- `tpch`: run all tpch queries. Optionally, specify a range of queries as the final argument in Bash syntax:
  - Comma separated values: `1,2,9,14` will run TPC-H queries 1, 2, 9, and 14.
  - Ranges: `8..20` will run TPC-H queries 8 through 20, inclusive.
  - The two syntaxes cannot currently be mixed.
- `other`: run the other 9 queries from the ORQ paper
- `secretflow`: run the SecretFlow queries

The next argument, `sf` refers to Scale Factor. For TPC-H, this is a well-defined term in the specification. For other queries, we define SF1 to be approximately the same size, on average; that is, about 5M rows.

`enviro` is as in `run_experiment.sh`: it may be `lan`, `wan`, or `same` (local).

## Writing New ORQ Programs

See the [examples](https://github.com/CASP-Systems-BU/orq/tree/main/examples) for some simple programs that showcase the main features of the framework. **Example 0** contains a minimal C++ file you can use to get started with your own programs.

`CMakeLists.txt` only looks in certain directories for C++ sources. New programs should be placed into the `examples/`, `bench/`, or `tests/` directories. Alternatively, modify `CMakeLists.txt` to look for your sources.

## Creating a Cluster

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

## Citation

```
@inproceedings{orq,
  author    = {Eli Baum and Sam Buxbaum and Nitin Mathai and Muhammad Faisal and Vasiliki Kalavri and Mayank Varia and John Liagouris},
  title     = {ORQ: Complex analytics on private data with strong security guarantees},
  booktitle = {Proceedings of the 29th ACM Symposium on Operating Systems Principles (SOSP '25)},
  year      = {2025},
  publisher = {Association for Computing Machinery},
  doi       = {https://doi.org/10.1145/3731569.3764833},
}
```
