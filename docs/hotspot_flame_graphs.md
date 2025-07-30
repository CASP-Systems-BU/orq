# Generating flame graphs with perf and Hotspot

## Installation
To generate these graphs, you need the Linux `perf` tool and [Hotspot](https://github.com/KDAB/hotspot/). Unfortunately, this means this method only works on Linux, as `perf` uses code specific to the Linux kernel.

For Ubuntu, you can install these with
```bash
sudo apt install linux-tools hotspot
```

## Permissions

You need to loosen some kernel tracing restrictions for `perf` to do all the relevant tracing. On my machine (running Ubuntu) this consists of running the following commands, but it may vary by OS.
```bash
sudo chmod -R o+rx /sys/kernel/tracing
sudo sysctl kernel.perf_event_paranoid=-1
```
These don't persist across restarts, I'm sure it's possible to make them do so but in the interest of security it's probably safer not to.

## Running perf

This `perf` command is taken from the [Hotspot documentation](https://github.com/KDAB/hotspot?tab=readme-ov-file#off-cpu-profiling) and allows for profiling both on-CPU time (cycles) and off-CPU time (calculated by Hotspot based on sched_switch). I get some warnings about kernel tracing when I run it, but as far as I can tell they aren't relevant for our purposes.
```
perf record -e cycles -e sched:sched_switch --switch-events --sample-cpu -m 8M --aio --call-graph dwarf -o <output file> -- <command to profile>
```

## Using Hotspot

Launch Hotspot with
```bash
hotspot <data file>
```
I see (probably related) kernel tracing warnings when I launch Hotspot, but again this hasn't appeared to cause any problems.

### Flame Graph

I've found the Flame Graph tab to be the most useful, both for the flame graph itself and the timeline below it. For secrecy programs you should see two main stacks in the flame graph, one starting with `_start` and the other starting with `__clone3`. The `_start` stack is the main thread, anything here will not be parallelized with more threads. The `__clone3` is computation threads, anything here will be parallelized.

### Timeline

You can also see each of the threads from each process in the timeline. For secrecy programs, you should see the main thread, each computation thread, and two extras that only seem to exist for a short time and can be ignored. If you right click on a thread name in the timeline, you can Filter In on it to show only computation from that thread in the flame graph. Generally this will zoom in on the corresponding stack (which you can also do just by clicking on the relevant function in the flame graph). You can right click anywhere in the timeline for the option to Filter Out or Reset Filter.

The timeline can also show you the distribution of work between the main and computation threads. For example, if some element of local computation takes more time later in an algorithm, you'll see more orange in the main thread later in the timeline and less in the computation threads. If you click and drag over a section of the timeline, you can Filter In on the selection to see which functions are taking up that extra time in the flame graph.
