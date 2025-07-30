# ORQ

ORQ is a multi-party computation framework for relational analytics. For more information, see our paper at SOSP '25.

This repository is organized as follows:
- `docs/`: miscellaneous documentation (see `doxygen` for full documentation)
- `examples/`: input data for examples
- `include/`: core functionality of ORQ (`core/`), including the implementation of MPC primitives (`core/protocols/`), relational oblivious operators (`core/operators`; `relational/`), and party communication (`core/communication/`; `service/`).
- `scripts/`: various scripts for testing and benchmarking ORQ
- `src/`: source code for queries and benchmarks
- `tests/`: the test suite

## Artifact Replication

See [here](./scripts/sosp25/README.md) for instructions on artifact replication.

## Building ORQ
The following instructions assume a single-node \*nix system. On platforms with `apt` (Ubuntu), simply run the setup script:

```bash
$ git clone https://github.com/CASP-Systems-BU/orq
$ cd orq
$ scripts/setup.sh
```

If you do not have `apt` (other Linux, macOS, etc.) you will need to install the
dependencies manually. As of this writing, that list is:

```
git cmake pkg-config build-essential manpages-dev gfortran wget libsqlite3-0 libsqlite3-dev libsodium23 libsodium-dev libopenmpi3 libopenmpi-dev openmpi-bin openmpi-common python3 python3-pip libtool autoconf automake
```

Not all dependencies are required for all operations. For the offline phase of **secure Beaver triples** and **secure 2-party permutation correlations**, we require
- [libOTe](https://github.com/osu-crypto/libOTe) and
- [secure-join](https://github.com/Visa-Research/secure-join), respectively.

> Note: for artifact replication, we do not install these dependencies, since we only present benchmarks of the online phase.

To build ORQ, you will need, at a minimum:
- C++ 20
- [libsodium](https://libsodium.gitbook.io/doc/installation)
- Your platform's threading library (should be installed by default)
- `sqlite`, for testing TPCH queries.
- If you do not want to use our custom communicator, [OpenMPI](https://www.open-mpi.org/software/ompi/)

You should still run `setup.sh` even if you install the dependencies manually, because other libraries will be compiled and built manually.

## Running ORQ
Follow the standard `cmake` patterns for compiling and running ORQ programs.

```bash
$ mkdir build
$ cd build
$ cmake ..
$ make -j test_primitives
$ mpirun -np 3 test_primitives
```

This example will compile and run the `test_primitives` test under the replicated three-party protocol (the default).

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
   - `-DPROTOCOL=3` three party replicated honest majority protocol
   - `-DPROTOCOL=4` [Fantastic Four](https://eprint.iacr.org/2020/1330) honest-majority malicious 4PC protocol
- `-DNO_X86_SSE=1` to disable x86 hardware optimizations (you will get warnings otherwise if built on ARM platforms, like newer Macs)
- `-DPROFILE=1` enable profiling (compile with `-pg`)
- `-DEXTRA=XXX` pass the additional argument `XXX` to `make`
- `-DCOMM=XXX` enable the given communicator. Options are `"MPI" "SOCKET"`. The default is `MPI`.

See `debug/debug.h` and `CMakeLists.txt` for more information on compile options.

Run programs using the `startmpc` or `run_experiment` script.
- Run `scripts/setup.sh` to make the `startmpc` script available.
- Set the `COMM` CMake argument to compile ORQ with the custom communicator.
    ```
    cmake .. -DCOMM=SOCKET
    ```
- Build the required binary.
    ```
    make test_primitives
    ```
- Copy the binary to the other machines.
- Run the binary using the `startmpc` script.
    ```
    // Local execution
    startmpc -n 3 ./test_primitives

    // Remote execution
    startmpc -n 3 -h machine-1,machine-2,machine-3 ./test_primitives
    ```

This entire process can be automated using the `run_experiment.sh` script. For example,
```bash
$ ../scripts/run_experiment.sh -p 3 -s lan -c nocopy -n 4 -f 1 -T 16 q5
```
will compile and run TPCH query `q5` at Scale Factor 1, under 3PC LAN, with `4` nocopy communicator threads, 16 worker threads.
