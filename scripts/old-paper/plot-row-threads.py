#!/usr/bin/env python3

import matplotlib.pyplot as plt
from pathlib import Path
import sys
from collections import defaultdict
import numpy as np
import matplotlib.ticker as ticker

input_file = [Path(p) for p in sys.argv[1:]]

plt.ion()

algo = "Quick"

LOGLOG = True
T_PER_ROW = True

data = defaultdict(lambda: defaultdict(list))

for _f in input_file:
    # map: threads -> rows -> time

    current_rows = None
    current_threads = None
    fastest = np.inf

    with open(_f) as f:
        for line in f:
            sp = line.split()
            if "threads" in line:
                # load old data
                if current_threads:
                    if T_PER_ROW:
                        fastest /= current_rows
                    data[current_threads][current_rows] = fastest
                current_rows = int(sp[1])
                current_threads = int(sp[3])
                fastest = np.inf
            elif algo in line:
                fastest = min(fastest, float(sp[-2]))
            
        if T_PER_ROW:
            fastest /= current_rows
        data[current_threads][current_rows] = fastest

PLOT_THREADS = [4, 9, 17, 25, 34]

for T in PLOT_THREADS:
    D = sorted(data[T].items())
    size, t = zip(*D)

    if T_PER_ROW:
        t = np.array(t) * 1e6 # microseconds

    plt.plot(size, t, '.-', label=f"{T} thr")

plt.legend()

if LOGLOG:
    if not T_PER_ROW:
        plt.yscale('log', base=10)
    plt.xscale('log', base=2)

if T_PER_ROW:
    plt.ylabel("time per row, µsec")
else:
    plt.ylabel("time, sec")

plt.xlabel("Input size")

if T_PER_ROW:
    plt.ylim(0, 13)
else:
    plt.ylim(0)

plt.title(f"Input scaling: {algo}")

plt.show()
input("press enter to continue...")