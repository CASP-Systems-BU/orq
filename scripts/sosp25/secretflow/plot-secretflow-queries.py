#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np
import sys
from pathlib import Path
from glob import glob
import re
import os

os.chdir(os.path.dirname(os.path.abspath(__file__)))

sys.path.append('../..')
import plot_query_experiments as pq

plt.rcParams.update(pq.PLOT_PARAMS)

plt.rcParams['figure.figsize'] = (2.5, 2)

################################################
# Data
################################################

if len(sys.argv) != 2:
    print("Provide folder name to plot")
    sys.exit(1)

folder = Path(sys.argv[1])

experiments = ['S1', 'S2', 'S3', 'S4', 'S5']
# experiments = ['Q6', 'Q1', 'Q14', 'Q12', 'Q1*']

# SecretFlow - 16M
# Extract the SecretFlow results
secflow_times = []
for e in experiments:
    p = folder / (e.lower() + "*.txt")
    g = glob(str(p))[0]

    print(f"{e}: Opening {g}")

    with open(g) as f:
        for line in f:
            if "rows in set" in line:
                t = float(re.search(r"\(([\d.]+)s\)", line).group(1))
                secflow_times.append(t)
                print("\t", t)
                break
        else:
            print(f"Could not find time for {e}")
            sys.exit(1)

secflow_times = np.array(secflow_times) / 60
# secflow_times = np.array([69.53, 83, 167.8, 252.56, 963.59]) / 60

# ORQ - 16M
# Extract the ORQ results

orq_times = [0] * len(secflow_times)

query_indices = {
    'sf-q1': 1,
    'sf-q6': 0,
    'sf-q12': 3,
    'sf-q14': 2,
    'sf-q1-mod': 4
}

g = glob(str(folder / "orq*.txt"))[0]
with open(g) as f:
    ot = 0
    pt = 0
    idx = -1

    for line in f:
        line = line.strip()
        if line.startswith("Query"):
            query = line.split()[1]
            if idx >= 0:
                orq_times[idx] = ot - pt
                pt = 0

            idx = query_indices[query]
        elif "Overall" in line:
            ot = float(line.split()[-2])
            # print(" ot", ot)
        elif "dm-dummyperm" in line:
            pt = float(line.split()[-2])
            # print(" pt", pt)

    orq_times[idx] = ot - pt
# orq_times = np.array([1.195, 0.976, 146.790, 233.520, 624.050]) / 60

print(orq_times)
orq_times = np.array(orq_times) / 60

################################################

# Calculate speedup
speedup = [secflow_times[i] / orq_times[i] for i in range(len(experiments))]

# Bar positions
x = np.arange(len(experiments))

# Plotting
fig, ax = plt.subplots(layout='constrained')
# TODO: colors (using SBK hex for now, fix this)
# hatch = ['//////' if q in plaintext_queries else None for q in experiments]
bars1 = ax.bar(experiments, secflow_times, align='edge', width=-0.4, label='SecretFlow', color=pq.TAB_COLORS['purple'])
bars2 = ax.bar(experiments, orq_times, align='edge', width=0.4, label='ORQ', color='dimgray') # pq.PROTO_COLORS['SH-DM'])

def f(r):
    if r < 10:
        return f"{r:.1f}$\\times$"
    else:
        return f"{round(r)}$\\times$"

speedups = [f(n/c) for (n, c) in zip(secflow_times, orq_times)]

ax.bar_label(bars1, speedups, padding=2)

# Log scale for vertical axis
# ax.set_yscale('log')

# Labels and titles
# combine 
# ax.set_ylabel('Time (min)')
ax.set_xticks(x)
ax.set_xticklabels(experiments, rotation=45)
ax.legend(ncols=1)

# plt.ylim(None, yl[1] )
# print(plt.ylim())
# ax.set_yscale('log')

yl = plt.ylim()
print(yl)
plt.ylim(None, yl[1] * 1.2)

# add_speedup_labels(bars1, bars2)
 
# Show plot
# plt.tight_layout()
output_name = 'secretflow-queries.png'
plt.savefig(output_name, dpi=300)

print("Saved to", output_name)