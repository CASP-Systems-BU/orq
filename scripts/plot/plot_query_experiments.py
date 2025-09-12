#!/usr/bin/env python3

import os
import re
import argparse
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.ticker import NullLocator
import glob
from pathlib import Path
import warnings
import numpy as np

warnings.filterwarnings(action="ignore", message="Mean of empty slice")

TAB_COLORS = {
  'blue': '#5778a4',
  'orange': '#e49444',
  'red': '#d1615d',
  'teal': '#85b6b2',
  'green': '#6a9f58',
  'yellow': '#e7ca60',
  'purple': '#a87c9f',
  'pink': '#f1a2a9',
  'brown': '#967662',
  'grey': '#b8b0ac',
}

PLOT_PARAMS = {
    "figure.figsize": (6, 2),
    "figure.dpi": 300,
    "axes.titlesize": 13,
    "font.size": 12,
    "font.family": "cmss10",
    "hatch.linewidth": 1,
    "hatch.color": "white",
    "xtick.major.pad": 3,
    "lines.markersize": 5,
    "legend.fontsize": 11,
    "legend.columnspacing": 2
}

Q6_LABEL_SIZE = 10

NAME_MAP = {
    "aspirin": "Aspirin",
    "credit_score": "Credit",
    "custom_agg": "SYan",  # Secure Yannakakis
    "distinct_patients": "Patients",
    "market-share": "Market",
    "pwd-reuse": "Pwd",
    "rcdiff": "C.Diff",
    "secrecy_q2": "SecQ2",
    "comorbidity": "Comorb.",
}

PLOT_SORTING = False
PLOT_MINUTES = True

LAN_STYLE = None
WAN_STYLE = "////"

PROTO_COLORS = {
    'SH-DM': TAB_COLORS['blue'],
    'SH-HM': TAB_COLORS['green'],
    'Mal-HM': TAB_COLORS['red']
}

PLOT_ONLY = None


def parse_files(data_dir, compute_median=True, twopc=False):
    data_list = []

    if data_dir == None:
        return

    gl_path = str(data_dir.expanduser() / "**/*.txt")

    for file_name in glob.glob(gl_path, recursive=True):
        query_path = Path(file_name)

        query_name = query_path.stem

        if PLOT_ONLY:
            if PLOT_ONLY.startswith("t"):
                if not query_name.startswith("q"):
                    print("Skip Other", query_name)
                    continue
            elif PLOT_ONLY.startswith("o"):
                if query_name.startswith("q"):
                    print("Skip TPCH", query_name)
                    continue
            else:
                exit(-1)

        if query_name == "meta":
            # not a query, metadata file
            continue

        with open(query_path, "r") as file:
            lines = file.readlines()

        overall_time = None
        sort_time = None
        agg_time = None
        other_time = None
        sort_comm = 0
        agg_comm = 0
        other_comm = 0
        preproc = None
        sf = None
        threads = None

        for line in lines:
            spl = line.split()
            if "Overall" in line:
                overall_time = float(spl[2].strip())
            if "[=PROFILE]" in line and "table.sort" in line:
                sort_time = float(spl[2].strip())
            if "[=PROFILE]" in line and "table.aggregate" in line:
                agg_time = float(spl[2].strip())
            if "[=PROFILE]" in line and "other" in line:
                other_time = float(spl[2].strip())
            if "[=COMM]" in line and "table.sort" in line:
                sort_comm = float(spl[2].strip())
            if "[=COMM]" in line and "table.aggregate" in line:
                agg_comm = float(spl[2].strip())
            if "[=COMM]" in line and "other" in line:
                other_comm = float(spl[2].strip())
            if "SF;" in line:
                sf = int(spl[1].strip())
                threads = int(spl[3].strip())
            if "[=PREPROC]" in line:
                preproc = float(spl[-2].strip())

        if threads == None:
            continue

        if overall_time == None:
            continue

        if twopc:
            if preproc == None:
                print(f"WARNING: {query_name} has no preprocessing time.")
            else:
                overall_time -= preproc

        data_list.append(
            [
                query_name,
                overall_time,
                sort_time,
                agg_time,
                other_time,
                sort_comm / threads,
                agg_comm / threads,
                other_comm / threads,
                sf,
            ]
        )

    df = pd.DataFrame(
        data_list,
        columns=[
            "query",
            "overall_time",
            "sort",
            "agg",
            "other",
            "sort_comm",
            "agg_comm",
            "other_comm",
            "sf",
        ],
    )

    df["overall_time"] = df["overall_time"].astype(float)
    df.sort_values(by="query", inplace=True)

    df["query"] = df["query"].apply(lambda x: NAME_MAP.get(x, x.title()))

    if not compute_median:
        return df

    dfr = (
        df.groupby("query")
        .agg(
            median=("overall_time", "median"),
            med_sort=("sort", "median"),
            med_agg=("agg", "median"),
            range=("overall_time", lambda x: x.max() - x.min()),
            count=("overall_time", "count"),
        )
        .reset_index()
    )

    if not PLOT_SORTING:
        dfr["med_sort"] = 0
        dfr["med_agg"] = 0

    dfr["flag"] = dfr["count"].apply(lambda x: "###" if x < 3 else "")

    dfr["range_pct"] = dfr["range"] / dfr["median"]

    if any(dfr["range_pct"] > 0.05):
        print("==== WARNING: large range")

    dfr = sort_query_order(dfr)

    dfr["med_sort"] = dfr["med_sort"].fillna(0)
    dfr["med_agg"] = dfr["med_agg"].fillna(0)

    return dfr


def sort_query_order(df):
    def extract_number(query):
        match = re.match(r"^Q(\d+)$", query)
        return int(match.group(1)) if match else float("inf")

    numeric_queries = df[df["query"].str.match(r"^Q\d+$")]
    other_queries = df[~df["query"].str.match(r"^Q\d+$")]

    numeric_queries = (
        numeric_queries.assign(
            query_number=numeric_queries["query"].apply(extract_number)
        )
        .sort_values(by="query_number")
        .drop(columns=["query_number"])
    )

    other_queries = other_queries.sort_values(by="query")

    sorted_df = pd.concat([numeric_queries, other_queries], ignore_index=True)
    return sorted_df


def plot_sublot(ax, p, lan, wan):
    # lan_colors = [
    #     TPCH_LAN_COLOR if c.startswith("Q") else OTHER_LAN_COLOR for c in lan["query"]
    # ]
    # wan_colors = [
    #     TPCH_WAN_COLOR if c.startswith("Q") else OTHER_WAN_COLOR for c in wan["query"]
    # ]

    c = PROTO_COLORS[p]

    # print(p, "LAN")
    # print(lan)
    # print(p, "WAN")
    # print(wan)

    if PLOT_MINUTES:
        lan["med_sort"] /= 60
        lan["med_agg"] /= 60
        lan["median"] /= 60
        wan["med_sort"] /= 60
        wan["med_agg"] /= 60
        wan["median"] /= 60
        ax.locator_params(nbins=4)

    ax.tick_params(axis="x", rotation=90)

    x = np.arange(len(wan[wan['query'].str.startswith('Q')]))
    x = np.r_[x, (1 + np.arange(len(x), len(wan)))]

    ax.bar(x, wan["median"], hatch=WAN_STYLE, color=c)
    ax.bar(x, lan["median"], hatch=LAN_STYLE, color=c)

    # only label center plot
    if p == "SH-HM":
        if PLOT_MINUTES:
            ax.set_ylabel("Time (min)")
        else:
            ax.set_ylabel("Time (s)")

    ax.grid(axis="y", linestyle="--", alpha=0.6)

    orig_xlim = ax.get_xlim()

    max_L = lan["median"].max()
    max_W = wan["median"].max()
    ## Too cluttered to add these.
    # plt.hlines(max_t, *orig_xlim, 'k', '--', alpha=0.5, linewidth=1)

    median_L = lan["median"].median()
    # plt.hlines(median_L, *orig_xlim, 'k', ':', alpha=0.5, linewidth=1)

    median_W = wan["median"].median()
    # plt.hlines(median_W, *orig_xlim, 'k', ':', alpha=0.5, linewidth=1)

    print(f"{p} LAN: median {median_L}; max {max_L}")
    print(f"{p} WAN: median {median_W}; max {max_W}")

    ovh = wan["median"] / lan["median"]
    print(
        f"{p} WAN-LAN ovh: {ovh.min():.2f}x - {ovh.max():.2f}x; avg {ovh.median():.2f}x"
    )

    ax.set_title(" " + p, loc="left", y=1, pad=-13)

    # r = orig_xlim[1]
    # # plt.text(r, max_t * 1.05, f"Max (LAN): {max_t:.2f} ", va='bottom', ha='right')
    # plt.text(r, median_L * 1.5, f"Median (LAN): {median_L:.2f} ", va='bottom', ha='right')
    # plt.text(r, median_W * 1.5, f"Median (WAN): {median_W:.2f} ", va='bottom', ha='right')

    # Q6 clarification
    q6_lan_val = lan[lan["query"] == "Q6"]["median"].values
    q6_wan_val = wan[wan["query"] == "Q6"]["median"].values
    if len(q6_lan_val) and len(q6_wan_val):
        q6_lan_val = q6_lan_val[0]
        q6_wan_val = q6_wan_val[0]
        if PLOT_MINUTES:
            q6_lan_val *= 60
            q6_wan_val *= 60
        ax.text(
            lan[lan["query" ]== "Q6"].index[0],
            max_W * 0.02,
            f" {q6_lan_val:.1f}s / {q6_wan_val:.1f}s",
            ha="center",
            va="bottom",
            size=Q6_LABEL_SIZE,
            rotation=90,
        )

    if p.startswith("SH-DM"):
        ax.bar([0], [0], hatch=WAN_STYLE, color="dimgray", label="WAN")
        ax.bar([0], [0], hatch=LAN_STYLE, color="dimgray", label="LAN")
        ax.legend(loc="upper right")

    ax.set_xticks(list(x) + [-1])
    ax.set_xticklabels(list(lan["query"]) + ["TPCH"])
    ax.xaxis.get_major_ticks()[-1].tick1line.set_visible(False)

    ax.set_yticklabels([i.get_text().replace("-", "–") for i in ax.get_yticklabels()])
    # print(ax.get_yticklabels())

    # ax.tight_layout()
    ax.set_xlim(orig_xlim)


def plot_overall_time(S2, S3, S4, result_dir, title):
    PLOT_PARAMS["figure.figsize"] = (6, 4)
    plt.rcParams.update(PLOT_PARAMS)

    fig, ax = plt.subplots(3, 1, layout="constrained", sharex=True)

    plot_sublot(ax[0], "SH-DM", *S2)
    plot_sublot(ax[1], "SH-HM", *S3)
    plot_sublot(ax[2], "Mal-HM", *S4)

    output_path = result_dir / f"plot-{title}.png"
    plt.savefig(Path(output_path), dpi=300)
    plt.close()


if __name__ == "__main__":
    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("-L2", type=Path, help="2PC LAN data directory")
    parser.add_argument("-W2", type=Path, help="2PC WAN data directory")
    parser.add_argument("-L3", type=Path, help="3PC LAN data directory")
    parser.add_argument("-W3", type=Path, help="3PC WAN data directory")
    parser.add_argument("-L4", type=Path, help="4PC LAN data directory")
    parser.add_argument("-W4", type=Path, help="4PC WAN data directory")
    parser.add_argument("title", type=str, help="Title/filename")
    args = parser.parse_args()

    # Directory paths
    result_dir = Path(args.L3).parent.parent.expanduser()

    print(args)

    # Process results
    L2_stats = parse_files(args.L2, twopc=True)
    W2_stats = parse_files(args.W2, twopc=True)

    assert len(L2_stats) > 0
    assert len(W2_stats) > 0

    L2_stats.to_csv(
        os.path.join(result_dir, f"stats lan-2 {args.title}.csv"), index=False
    )
    W2_stats.to_csv(
        os.path.join(result_dir, f"stats wan-2 {args.title}.csv"), index=False
    )

    L3_stats = parse_files(args.L3)
    W3_stats = parse_files(args.W3)

    L3_stats.to_csv(
        os.path.join(result_dir, f"stats lan-3 {args.title}.csv"), index=False
    )
    W3_stats.to_csv(
        os.path.join(result_dir, f"stats wan-3 {args.title}.csv"), index=False
    )

    L4_stats = parse_files(args.L4)
    W4_stats = parse_files(args.W4)

    L4_stats.to_csv(
        os.path.join(result_dir, f"stats lan-4 {args.title}.csv"), index=False
    )
    W4_stats.to_csv(
        os.path.join(result_dir, f"stats wan-4 {args.title}.csv"), index=False
    )

    plot_overall_time(
        [L2_stats, W2_stats],
        [L3_stats, W3_stats],
        [L4_stats, W4_stats],
        result_dir,
        args.title,
    )
