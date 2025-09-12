#!/usr/bin/env python3

import argparse
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import matplotlib.colors as mcolors
from pathlib import Path

import plot_query_experiments as pq


PLOT_QUERIES = [
    "Q6",
    "Q8",
    "Q11",
    "Q12",
    "Q21",
    "Q22",
]

colors = ["tab:blue", "tab:green", "tab:orange"]

SF = 10
PLOT_REL = False
PLOT_LOG = False

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("dir", type=Path, help="data directory")
    parser.add_argument("name", type=Path, help="output filename")
    args = parser.parse_args()

    result_dir = args.dir.parent.expanduser()

    data = pq.parse_files(args.dir, False)

    data = (
        data[data["query"].isin(PLOT_QUERIES) & (data["sf"] == SF)]
        .dropna(subset=["overall_time"])
        .drop(columns=["sf"])
        .groupby("query")
        .first()
    )

    plt.figure(figsize=(11, 5))

    if PLOT_REL:
        data["sort"] /= data["overall_time"]
        data["agg"] /= data["overall_time"]
        data["sort_comm"] /= data["overall_time"]
        data["agg_comm"] /= data["overall_time"]

    data["agg_exec"] = data["agg"] - data["agg_comm"]
    data["sort_exec"] = data["sort"] - data["sort_comm"]
    data["other_exec"] = data["other"] - data["other_comm"]
    print(data)

    if not PLOT_REL:
        plt.bar(
            data.index,
            data["overall_time"],
            edgecolor="red",
            color="w",
            linewidth=1,
            width=0.9,
            label="E2E",
        )

    plt.bar(
        data.index,
        data["sort"],
        align="edge",
        width=0.4,
        color="teal",
        hatch="////",
        edgecolor="w",
        label="Sort Exec",
    )

    plt.bar(
        data.index,
        data["sort_comm"],
        align="edge",
        width=0.4,
        color="teal",
        edgecolor="w",
        label="Sort Comm",
    )

    plt.bar(
        data.index,
        data["agg"],
        align="edge",
        width=-0.4,
        color="darkorange",
        hatch="////",
        edgecolor="w",
        label="Agg Exec",
    )

    plt.bar(
        data.index,
        data["agg_comm"],
        align="edge",
        width=-0.4,
        color="darkorange",
        edgecolor="w",
        label="Agg Comm",
    )

    # plt.semilogy()
    plt.legend()

    plt.legend()
    plt.ylabel("ratio of overall time" if PLOT_REL else "t (sec)")
    plt.title(f"{args.name} {'[log]' if PLOT_LOG else '[linear]'} (SF{SF}){' - relative' if PLOT_REL else ''}")

    plt.savefig(result_dir / args.name, dpi=300)

    plt.show()
