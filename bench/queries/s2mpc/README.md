# S2MPC

Welcome to the [S2MPC Tutorial](https://sites.bu.edu/casp/S2MPC/) at EuroSys 2026!

In the hands-on session, you will create your own CloudLab node, try your hand at implementing queries in ORQ, and then run a distributed three-party computation with other participants.

## CloudLab

To sign up for a CloudLab account, click [this link](https://www.cloudlab.us/signup.php?pid=S2MPC-EuroSys26). If you already have a CloudLab account, request to join project `S2MPC-EuroSys26` (case insensitive).

## Setting up your node

Clone this repo:

```bash
sudo git clone https://github.com/CASP-Systems-BU/orq.git -b s2mpc-2026 /s2mpc/orq
```

> It is important that we clone the repo into a fixed directory rather than `~/`. Later on, we will rely on the MPC executable existing at the same absolute path on all machines.

Next, run the following commands (copy and paste is fine). This will set up ORQ's minimum dependencies, compile, and run the MPC primitives test.

```bash
cd /s2mpc/orq
sudo chmod a+rwx .
mkdir build
cd build
../scripts/_setup_required.sh
cmake .. -DPROTOCOL=3
make test_primitives
mpirun -n 3 test_primitives
```

## Queries

We'll walk through `distinct_patients.cpp` together. Then, you'll have the opportunity to work on the Secure Yannakakis query:
- `sec_yan.cpp` is a skeleton query with some blanks for you to fill in
- `sec_yan_hard.cpp` is an entirely blank query, if you want a challenge (we just load the data tables)
- `sec_yan_solution.cpp` contains our solution (and the version benchmarked in the ORQ paper). You start at `// Query begins here.`

Data for these queries resides in `/examples/data/data-owner-*/`. We imagine that each data owner holds a single table. You can change the CSVs and should observe changes in the query output.

You are welcome to use VSCode SSH or your favorite remote IDE if that's easier.
