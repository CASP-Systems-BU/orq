#!/usr/bin/env python3
#
# Parse and graph stopwatch results, with error bars

import matplotlib.pyplot as plt
from pathlib import Path
import sys
from collections import defaultdict
import numpy as np
import matplotlib.ticker as ticker

input_files = [Path(p) for p in sys.argv[1:]]

plt.ion()
label="±1$\sigma$"

A = "Radix"

fastest = np.inf

for _f in input_files:
    # map: category -> thread -> data
    data = defaultdict(lambda: defaultdict(list))
    current_thread = -1
    with open(_f) as f:
        for line in f:
            sp = line.split()
            if "threads" in line:
                current_thread = int(sp[3])
            elif "[ SW]" in line:
                category = sp[2]
                t = float(sp[-2])
                data[category][current_thread].append(t)

    linestyle = "-"
    if "baseline" in _f.stem:
        linestyle = ":"

    for c, cat_data in data.items():
        if c != A:
            continue
        averages = np.zeros(len(cat_data))
        devs = np.zeros(len(cat_data))
        for i, (t, thread_data) in enumerate(cat_data.items()):
            averages[i] = np.mean(thread_data)
            devs[i] = np.std(thread_data)
        
        fastest=min(fastest, *averages)

        # thread numbers
        x = cat_data.keys()
        L = plt.plot(x, averages, label=f"{_f.stem}", linestyle=linestyle)
        error = (averages - devs, averages + devs)
        plt.fill_between(x, error[0], error[1], alpha=0.2, label=label)
        label=None

plt.ylim(0)

lim = plt.xlim()

plt.hlines(fastest, 1, lim[1], color='gray', linestyle='--', label=f"Best: {fastest:0.1f} sec")

plt.locator_params(axis='y', nbins=3) 

plt.title(f"{A} 1M (3PC LAN)")
plt.legend()
plt.xscale('log', base=2)

plt.xlabel("Threads")

plt.show()


input("Press enter to exit...")