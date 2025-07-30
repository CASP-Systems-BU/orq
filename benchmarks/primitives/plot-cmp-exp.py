#!/usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import sys
import pathlib
import os
import math

PLOT_OPS = ['AND', 'RCA', 'EQ']
PLOT_LOG = False
PLOT_RELATIVE = True

LINE_STYLES = ['-', '--', ':', '-.']

if len(sys.argv) != 3:
    print(f"usage: {sys.argv[0]} exp1.txt exp2.txt")

exps = list(map(pathlib.Path, sys.argv[1:]))

common = pathlib.Path(os.path.commonpath(exps))

names = []
data = []
for e in exps:
    names.append(str(e.relative_to(common).with_suffix('')))
    data.append(pd.read_csv(e))

print(names)

plt.figure(figsize=(10, 6))
baseline = {}
for n, d in zip(names, data):
    c = None
    for op, st in zip(PLOT_OPS, LINE_STYLES):    
        print(f"Plotting {n}: {op}")

        op_data = d[d['op'] == op]
        avg_time = op_data.groupby('size')['t'].mean()

        do_plot = True

        if PLOT_RELATIVE:
            v = baseline.get(op, None)
            if v is not None:
                avg_time = v / avg_time
            else:
                baseline[op] = avg_time
                do_plot = False
                
        if do_plot:
            avg_time = avg_time.dropna()
            print(avg_time)
            if c == None and not PLOT_RELATIVE:
                p, = plt.plot(avg_time, linestyle=st, marker='.', label=f"{n} {op}")
                c = p.get_color()
            else:
                plt.plot(avg_time, linestyle=st, marker='.', color=c, label=f"{n} {op}")

if PLOT_LOG:
    plt.loglog()
else:
    plt.semilogx()
    plt.ylim(0, plt.ylim()[1] * 1.1)

plt.legend()
plt.title(' vs. '.join(names))
plt.xlabel("Input size")

if PLOT_RELATIVE:
    plt.ylabel("Speedup")
else:
    plt.ylabel("Exec time (sec)")

plt.show()