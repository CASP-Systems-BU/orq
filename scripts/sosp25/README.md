# Reproducibility Instructions

## Overview

We target all three artifact badges:
- **Available**: we publish ORQ on [Github](https://github.com/CASP-Systems-BU/orq), and once reviewers are otherwise satisfied, will publish to Zenodo.
- **Functional**: we provide instructions for running a simple test query.
- **Reproduced**: our main results -- Figures 4, 5, 6 and 7 -- are reproducible. Figure 10 is reproducible but is optional, because the largest two data points require other machines with larger RAM, and there is significant overlap with other experiments.

> [!warning]
>
> These experiments take a long time: a few hours up to a few days to run all experiments. You should use a terminal multiplexer such as `tmux` or `screen` to prevent your SSH connection from dropping and killing the experiment. We provide instructions below using `tmux`; a helpful reference is available [here](https://tmuxcheatsheet.com/).

We run all experiments on `c7a.16xlarge` AWS instances with Ubuntu 22.04.5
LTS and gcc 11.4.0.

Here we summarize the experiments outlined in this document, including their approximate running time.
- TPC-H and other queries (Figure 4), 3-45 hours (flexible)
- Comparison with Secrecy (Figure 5a), 8.5 hours
- Comparison with SecretFlow queries (Figure 5b), TODO
- Comparison with SecretFlow sorting (Figure 6), TODO
- Comparison with MP-SPDZ sorting (Figure 7), 1 day
- (Optional) ORQ sorting (Figure 10), 5-36 hours

In total, we expect the experiments to take between 36 (+ secretflow) hours and 114 (+ secretflow) hours (approximately 5 days). If needed, we can set up additional clusters to run experiments in parallel (additional clusters will be needed for SecretFlow anyway).

## Tips and Common Mistakes

We will make every effort to prevent any common mistakes, but we list them here in case they arise anyway.

1) All machines should be up to date (achievable with `sudo apt update && sudo apt upgrade`). We will ensure all machines provided have been updated.
2) You may be prompted to upgrade your kernel after beginning a script if the script involves installing additional dependencies. If this occurs, all experiments will be blocked until you interact with the popup. We recommend checking after beginning an experiment to see that the data collection has begun. If it seems like no data is being produced to the specified output folder, it is likely that the `tmux` or `screen` session has opened the popup.
3) 4PC sorting results may be slower than expected if run with the wrong 4PC configuration. We have automated the configuration selection. If the numbers appear to be approximately 1.5x-2x slower than expected, let us know, as the configuration may have been set incorrectly.

## Access

We will provide AWS access information (SSH keypairs and IP addresses) on HotCRP.

> [!important]
> All experiments run on AWS. To simplify artifact evaluation, we are providing an AWS environment to reviewers. Please be mindful that this environment is expensive and should be shut down when not in use. Let us know via HotCRP when you are done using a cluster and we can pause the instances.

From your local machine, SSH into the AWS cluster and start a tmux session.

```bash
$ ssh ubuntu@[CLUSTER-IP]
# first time: start a tmux session
$ tmux
# or if you are returning, re-attach
$ tmux a
# clone the repo
$ git clone https://github.com/CASP-Systems-BU/orq
$ cd orq
```

To _detach_ from a running `tmux` session, press `Ctrl-b D`.

## Available

_Time: 5 minutes_

All code for ORQ is contained in our public repository. Important components to highlight:
- `scripts/`
    - `sosp25`: all scripts for reproducing our experiments
    - `orchestration/aws`: scripts for launching an AWS cluster (we have run this for you)
- `include/`: our main framework, including
    - `core/communication/no_copy_communicator`: our custom communication layer
    - `core/operators`: implementations of oblivious operators, including oblivious quicksort, radixsort, and shuffle
    - `core/protocols`: implementations of secure multiparty computation protocols: 2PC [SH-DM], 3PC [SH-HM], and 4PC [Mal-HM]
    - `relational/database`: the oblivious analytics subsystem for tabular data
    - `service/common`: the backend of our framework, including our data-parallel vectorized runtime.
- `src/`: source code for all queries profiled in ORQ
- `tests/`: a test suite for our framework

## Functional

_Time: 10 minutes_

In this section we give instructions for running a single query. Execute the following commands on the AWS cluster:

```bash
$ cd ~/orq/build
$ ../scripts/query-experiments.sh tpch 0.1 3 16 lan 13..13
```

This command will run `tpch` query `13` at scale factor `0.1`, under the `3PC` protocol (SH-HM) with `16 threads`. (Watch out for the trailing `..13`. It is required.)

You should see output that looks something like this:

```
node0                                                                    [42/53]
Checking node2 => 172.31.20.108 => enp55s0                                      
Error: Cannot delete qdisc with handle of zero.                                 
Disabled WAN on enp55s0...                                                      
wan-sim.py                                    100% 1967     9.3MB/s   00:00     
Error: Cannot delete qdisc with handle of zero.                                 
Checking node0 => 172.31.18.79 => enp55s0                                       
Disabled WAN on enp55s0...                                                      
node0                                                                           
Checking node3 => 172.31.25.208 => enp55s0                                      
Error: Cannot delete qdisc with handle of zero.                                 
Disabled WAN on enp55s0...                                                      
wan-sim.py                                    100% 1967     8.6MB/s   00:00     
Error: Cannot delete qdisc with handle of zero.                                 
Checking node0 => 172.31.18.79 => enp55s0                                       
Disabled WAN on enp55s0...                                                      
PING node1 (172.31.29.51) 56(84) bytes of data.                                 
                                                                                
--- node1 ping statistics ---                                                   
3 packets transmitted, 3 received, 0% packet loss, time 2046ms                  
rtt min/avg/max/mdev = 0.205/0.228/0.248/0.017 ms                               
                                                                                
PING node2 (172.31.20.108) 56(84) bytes of data.                                
                                                                                
--- node2 ping statistics ---                                                   
3 packets transmitted, 3 received, 0% packet loss, time 2046ms                  
rtt min/avg/max/mdev = 0.190/0.194/0.201/0.004 ms                               
                                                                                
PING node3 (172.31.25.208) 56(84) bytes of data.                                
                                                                                
--- node3 ping statistics ---                                                   
3 packets transmitted, 3 received, 0% packet loss, time 2046ms                  
rtt min/avg/max/mdev = 0.170/0.196/0.221/0.020 ms                               
                                                                                
Query q13 | Scale Factor 0.1 | Protocol 3 | Threads 16                          
                                                                                
Common interface: enp55s0; subnet 172.31.18.79/20                               
Compiler flags: -O3 -g -pthread -D PROTOCOL_NUM=3 -mbmi -march=native -DCOMMUNIC
ATOR_NUM=NOCOPY_COMMUNICATOR -D NOCOPY_COMMUNICATOR_THREADS=4                   
-- Configuring done                                                             
-- Generating done                                                              
-- Build files have been written to: /home/ubuntu/orq/build         
[  0%] Built target print_4pc_info                                              
Consolidate compiler generated dependencies of target q13
[100%] Built target q13
==== 0.1 SF; 16 threads ====
NoCopyComm | Communication Threads: 4
Q13 SF 0.1
C size: 15000
O size: 150000
[=SW]            Start
[ SW]           Filter 0.01778  sec
[ SW]       outer join 12.52    sec
[ SW]     Distribution 11.93    sec
[ SW]       Final sort 12.99    sec
[=SW]          Overall 37.46    sec
30 rows OK
```

The first half of the output is just network configuration (specifically, confirming that the WAN simulator is not running; the `qdisc` errors are expected and safe to ignore). Query execution begins on the line `==== 0.1 SF; 16 threads ====`. The output `30 rows OK` signifies that ORQ's output from the oblivious execution was compared against a plaintext (SQLite) execution and matched.

> [Optional] To run the test suite, you can use the following commands:
> 
> ```bash
> $ cd ~/orq/build
> $ ../scripts/run_multithreaded_test.sh 3 1
> ```
> 
> Note: while this is not part of the artifact, it may be helpful for debugging. If at any time you run into issues -- or if any execution fails -- reach out to us on HotCRP.

## Reproduced

### TPC-H & other queries (Fig. 4)

_Time: 3-45 hours (flexible)_

To reproduce the TPC-H experiments, use these commands:

```bash
$ cd ~/orq
# specify protocol (2, 3, or 4) and network environment (lan or wan)
# 2 = SH-DM
# 3 = SH-HM
# 4 = Mal-HM
$ ./scripts/sosp25/artifact-tpch.sh 3 lan # ~ 3 hr
$ ./scripts/sosp25/artifact-tpch.sh 4 wan # ~ 16 hr
# once you've run the experiments you want to run, generate a plot
$ ./scripts/sosp25/artifact-tpch.sh plot
```

The full experiments take a long time. We estimate that running all 31 queries end-to-end takes, approximately:

|              |   LAN  |   WAN  |
| ------------ | -----: | -----: |
| 2PC (SH-DM)  | 3.5 hr | 9   hr |
| 3PC (SH-HM)  | 3   hr | 6.5 hr |
| 4PC (Mal-HM) | 6.5 hr | 16  hr |

You do not need to run all experiments to generate a partial plot. The plotting script can be re-run any time, and will ingest any new data generated. Here is an example of the output:

![](img/plot-tpch-example.png)

In this case we ran `2 lan`, `2 wan`, `3 lan`, and `4 wan`.

> [Optional] Reproducing the entirety of Figure 4 would take around 45 hours. We have added support for optionally running only a _subset_ of the TPC-H queries. You can specify either a list of a range of queries as the optional final argument to the script:
>
> ```bash
> # run queries Q1, Q9, and Q17 in 3PC LAN
> $ ./scripts/sosp25/artifact-tpch.sh 3 lan 1,9,17
> # run queries Q8 through Q15 in 2PC WAN
> $ ./scripts/sosp25/artifact-tpch.sh 2 wan 8..15
> ```
> 
> Note that when running a subset of TPC-H queries, we still run all `Other` queries, since these complete quickly, even in the slowest setting.

> [!Warning]
> 
> The TPC-H script runs some installation and setup on first execution. **You must run** TPC-H first before attempting other experiments.

### Secrecy (Fig. 5a)

_Time: 8.5 hours_

To replicate the Secrecy comparison, run the script for both Secrecy and ORQ:

```bash
$ cd ~/orq
# Run secrecy queries and collect data
$ ./scripts/sosp25/secrecy/run_secrecy.sh
# Run ORQ queries and collect data
$ ./scripts/sosp25/secrecy/run_orq.sh
# Generate Plot
$ ./scripts/sosp25/secrecy/plot_comparison.py
```

Due to a vartiety of factors which we improve on in our work, the Secrecy experiments take a long time (**8+ hours**). ORQ will take about five minutes. The output plot will look like this:

> Note: due to the large speedups of ORQ compared to Secrecy on some complex queries, the exact ratios may differ slightly.

Here is an example plot from this experiment:

![](img/secrecy-comp-example.png)

### SecretFlow

The comparison with Secretflow involves two experiments: `Sorting` and `TPC-H Queries`

#### Sorting (Fig. 6)

_Time: XXX_

The sorting experiment can be run using the following commands:
```bash
# Runs the complete experiment for both systems
./scripts/sosp25/secretflow/artifact-secretflow-sort.sh

# Once the experiments are complete, this command can be used to generate plots
./scripts/sosp25/secretflow/artifact-secretflow-sort.sh plot
```
The raw result along with the generated plot will be available in the `~/orq/results/secretflow-sort` directory once the above commands have been run.

#### TPC-H (Fig. 5b)

_Time: XXX_

The TPC-H query experiment can be run using the following commands:
```bash
# Runs the complete experiment for both systems
./scripts/sosp25/secretflow/artifact-secretflow-tpch.sh

# Once the experiments are complete, this command can be used to generate plots
./scripts/sosp25/secretflow/artifact-secretflow-tpch.sh plot
```
This script requires the following:
- A docker image for secretflow named `scql-aws-vldb-image` should be present in the `~/scql_image` directory.
- Pre-generated data for Secretflow tables should be present in `~/scql/vldb/docker-compose/data`

The raw result along with the generated plot will be available in the `~/orq/results/secretflow-tpch` directory once the above commands have been run.

### MP-SPDZ (Fig. 7)

_Time: approximately 1 day_

To reproduce the comparison with MP-SPDZ's sorting implementations, the relevant files are in `scripts/sosp25/mpspdz/reproducibility`. The entire comparison can be run with a single script: `full-experiment.sh`. Make sure to call this from the `mpspdz/reproducibility` directory. This script will run the ORQ benchmarks, then it will run the MP-SPDZ benchmarks, and then it will plot the result. The results will all go in the `~/results/` directory. The ORQ benchmarks will take 2-3 hours, while the MP-SPDZ benchmarks will take approximately 18 hours.

The plot in the paper for the comparison shows that ORQ can be run on larger inputs than MP-SPDZ. We run MP-SPDZ on all input sizes that we run ORQ. It is possible that 2PC execution of MP-SPDZ at the largest size will succeed, although it frequently crashes at this size. 3PC and 4PC execution of MP-SPDZ will run out of memory at the largest input size and silently crash. We omit the annotations from the plot when reproducing the results.

To run the script, execute the commands below.

```bash
$ cd ~/orq/scripts/sosp25/mpspdz/reproducibility/
# we recommend using tmux or screen since this will take a while
$ screen -S mpspdz
$ ./full-experiment.sh
```

Here is an example plot of this experiment:

![](img/mpspdz.png)

### ORQ Sorting (Fig. 10)

_Time: 5-36 hours (flexible)_

The large ORQ sorting experiment tests our sorting protocols with input sizes up to $2^{29}$ (~0.5B) elements. Only the largest data points are unique to this experiment. To run the two largest data points, we need machines with additional memory. The script will try to run them anyway, but they will crash on small machines. Since this experiment has a high degree of overlap with other experiments and takes over a day, we recommend prioritizing other experiments over this one and running the large sorting experiment if time permits. If you wish to run a subset of the experiment, you can optionally pass in a maximum power of two input size to stop the experiment early. By default, the maximum power of two input size is $2^{29}$.

Running with inputs up to $2^{25}$ would take approximately 5 hours. Running with inputs up to $2^{29}$ would take approximately 36 hours. If you wish to run with inputs up to $2^{29}$, let us know, and we can configure larger machines for the additional data points.

```bash
$ cd ~/orq/scripts/sosp25/sorting-main/
$ screen -S sorting-main
# runs input sizes up to and including 2^25
$ ./sorting-main.sh 25
```

## Cleaning Up

As a reminder, let us know when you are done testing (or just taking a break) so we can pause instances. Reach out to us on HotCRP with any questions.