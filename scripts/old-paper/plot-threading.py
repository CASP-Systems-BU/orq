#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import sys
import numpy as np
from collections import Counter, defaultdict
from pathlib import Path
from matplotlib.patches import Rectangle

DO_PLOT = False
PLOT_COMM = False

COLORS = ['k', 'b', 'g', 'r', 'c', 'm', 'y', 'orange']

P = Path(sys.argv[1])
df = pd.read_csv(P, comment='#', na_filter=False, keep_default_na=False)

main_thread = df[df["meta"] == "wait"]["tid"].values[0]
print(f"== Inferred main thread (0): {main_thread}")

# put main thread first
all_threads = [main_thread] + sorted(filter(lambda x: x != main_thread, (df["tid"].unique())))
n_threads = len(all_threads)

t_start = df[df["meta"] == ""]["t_start"].min()
df["t_start"] -= t_start

df.loc[df["t_start"] < 0, "elapsed"] = 0

# convert to ms
df["t_start"] /= 1e6
df["elapsed"] /= 1e6
df["t_end"] = df["t_start"] + df["elapsed"]

e2e = df["t_end"].max()
print(f"== e2e time: {e2e:.2f} ms")

total_time = 0

vlabel = "Main: Runtime Start"
mlabel = "Main: Runtime"
wlabel = "Main: Wait"

main_t = 0
main_wait_t = 0

thread_idx = {n: i for i, n in enumerate(all_threads)}
df["mapped"] = df["tid"].map(thread_idx)

if DO_PLOT:
    plt.ion()

def plot_timeline(df):
    _, ax = plt.subplots(figsize=(10, 6))

    runtime_starts = []

    comm_label = "W/Comm"
    wait_label = "M/Wait for W"
    vec_label = "Vec Copy"
    main_label = "M/Runtime"

    for index, row in df.iterrows():
        y = row["mapped"]

        t_start = row["t_start"]
        elapsed = row["elapsed"]

        c = COLORS[y % len(COLORS)]
        label = None

        m = row["meta"]
        if m == "comm":
            if not PLOT_COMM:
                continue
            y += 0.2
            c = "lightslategray"
            label = comm_label
            comm_label = None
        elif m == "wait":
            y += 0.2
            c = "silver"
            label = wait_label
            wait_label = None
        elif m == "vec":
            y -= 0.2
            c = "purple"
            label = vec_label
            vec_label = None
        elif row["tid"] == main_thread:
            runtime_starts.append(t_start)
            label = main_label
            main_label = None

        ax.add_patch(Rectangle((t_start, y), elapsed, 0.15, color=c, ec=None, lw=None, label=label))

    plt.vlines(runtime_starts, -0.5, len(all_threads) - 1 + 0.5,
                    lw=0.75, color='gray', linestyle=':', label="M/Runtime Enter")

    plt.xlim(-e2e*0.1, e2e * 1.1)
    plt.ylim(-1, n_threads)

    plt.yticks(range(n_threads), ["Main"] + list(f"W{i}" for i in range(n_threads - 1)))
    plt.xlabel("T (ms)")
    plt.legend(loc="upper right")
    plt.title(P.stem)

    # plt.savefig(P.with_suffix(".png"), dpi=300)
    plt.show()

if DO_PLOT:
    plot_timeline(df)

######### summary data #########

plt.figure(figsize=(8, 4))

summary_data = pd.DataFrame(index=sorted(df["mapped"].unique()))

for c in ["", "comm", "vec", "wait"]:
    summary_data["total_" + c] = df[df["meta"] == c].groupby("mapped")["elapsed"].sum()
    summary_data["count_" + c] = df[df["meta"] == c].groupby("mapped")["elapsed"].count()

summary_data["other"] = e2e - summary_data["total_"]
summary_data = summary_data.rename(columns={"total_": "total", "count_": "count_task"})

summary_data = summary_data.fillna(0)
summary_data["run"] = summary_data["total"] - summary_data["total_wait"] - summary_data["total_comm"].fillna(0)
summary_data.loc[1:, "total_wait"] = summary_data.loc[1:, "other"]
summary_data.loc[1:, "other"] = 0


with pd.option_context('display.max_rows', None, 'display.max_columns', None):
    print(summary_data)

print("== Worker averages")
print(summary_data[summary_data.index > 0].mean())

print("== Main Thread other:", e2e - summary_data["total"][0])

label_map = {
    "total_wait": ("Wait", "r"),
    "run": ("Runtime Exec", "green"),
    "total_comm": ("Communication", "orange"),
    "other": ("Non-Runtime Exec", "blue")
}

bottom = [0] * len(summary_data)
for c in ("run", "other", "total_comm", "total_wait"):
    label, color = label_map[c]

    hatch = None
    if c == "total_wait":
        hatch = "xx"

    plt.bar(summary_data.index, summary_data[c], color=color, bottom=bottom,
                label=label, width=0.7)
    bottom += summary_data[c]

plt.legend()

plt.ylabel("time (msec)")
plt.xlabel("Thread #")
plt.xticks(range(n_threads), ["Main"] + list(f"W{i}" for i in range(n_threads - 1)))

# plt.show()

plt.pause(0.1)
input("press enter to continue... ")