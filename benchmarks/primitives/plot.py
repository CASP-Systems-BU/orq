#!/usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import sys
import pathlib

PLOT_LOG = True
PLOT_SCALING = True
PLOT_DATAPOINT_LABELS = False

STYLES = {
    'AND': ('blue', '-'),
    # 'AND_EQ': ('skyblue', ':'),
    # 'AND_NO_ASSIGN': ('steelblue', '--'),

    'EQ': ('green', '-'),
    # 'EQ_NO_ASSIGN': ('limegreen', '--'),

    'GR': ('red', '-'),
    # 'LEQ': ('red', '-'),
    # 'GR_NO_ASSIGN': ('sandybrown', ':'),

    'RCA': ('black', '-'),
    # 'RCA_NO_ASSIGN': ('dimgray', '--'),
}

if len(sys.argv) != 2:
    print(f"usage: {sys.argv[0]} data.txt")
    exit(1)

data_file = pathlib.Path(sys.argv[1])


enviro = data_file.parent.stem.capitalize()
protocol = data_file.stem

df = pd.read_csv(data_file)

print(df)

all_ops = sorted(set(df['op']))

plt.figure(figsize=(10, 6))

time_per_elm = {}

for op in all_ops:
    if op not in STYLES:
        continue
    print(f"\nPlotting {op}")
    op_data = df[df['op'] == op]
    avg_time = op_data.groupby('size')['t'].mean()
    style = STYLES[op]
    marker = '.' if style[1] == '-' else None
    plt.plot(avg_time, color=style[0], linestyle=style[1], marker=marker, label=op)

    time_per_elm[op] = avg_time/avg_time.index

    if PLOT_DATAPOINT_LABELS:
        # plot above-point labels
        # skip the first one
        size_ratio = avg_time.index[1:] / avg_time.index[:-1]
        time_ratio = avg_time[1:].values / avg_time[:-1].values
        for i, (s, t) in enumerate(zip(size_ratio, time_ratio)):
            label = f" s={s:.1f}x / t={t:.1f}x"
            # multiplicative scaling bc log plot
            loc = (avg_time.index[i + 1], avg_time.values[i + 1])
            plt.text(*loc, label, fontsize=9, horizontalalignment='left', verticalalignment='top')


plt.legend()

if PLOT_LOG:
    plt.loglog()

plt.title(f"Primitives: {enviro} {protocol}" + (" (log/log)" if PLOT_LOG else ""))

plt.xlabel("Input size")
plt.ylabel("Execution time (sec)")

if PLOT_DATAPOINT_LABELS:
    plt.savefig(data_file.with_suffix('.labels.png'), dpi=300)
else:
    plt.savefig(data_file.with_suffix('.png'), dpi=300)

# plt.show()

if PLOT_SCALING:
    plt.figure(figsize=(10, 6))
    for op, data in time_per_elm.items():
        plt.plot(data * 1e9, label=op)
    plt.semilogx()
    plt.legend()

    max_plot = 1e9 * max(d.values[-1] for d in time_per_elm.values())
    plt.ylim(0, max_plot * 1.5)

    plt.title(f"Primitive scaling: {enviro} {protocol}")
    plt.xlabel("input size")
    plt.ylabel("time per row (µsec)")
    plt.savefig(data_file.with_suffix('.scaling.png'), dpi=300)
    # plt.show()