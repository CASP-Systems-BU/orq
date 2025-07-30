#!/usr/bin/env python3

import os
import re
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.ticker import NullLocator
import glob
from pathlib import Path
import warnings

warnings.filterwarnings(action='ignore', message='Mean of empty slice')

import plot_query_experiments as pq

QUERY_ORDER = ["Q11", "Q22", "Q12", "Q8", "Q21"]

LABEL_2PC = 'SH-DM'
LABEL_3PC = 'SH-HM'
LABEL_4PC = 'Mal-HM'

PLOT_PARAMS = pq.PLOT_PARAMS

colors = [pq.PROTO_COLORS['SH-DM'], pq.PROTO_COLORS['SH-HM'], pq.PROTO_COLORS['Mal-HM']]


def plot_sublot_t(color, ax, p, simwan, wan):

    colors = [color for c in wan['query']]

    ratio = wan['median'] / simwan['median']

    max_ratio = ratio.max()
    print(f"Max ratio for {p}: {max_ratio}")

    # Plot the ratio
    b = ax.bar(wan['query'], wan['median'], color=colors, hatch='////')

    ax.set_yscale('log')

    ax.set_ylim(1, 400)
    
    if p == LABEL_2PC:
        ax.set_ylabel('Time (min)')
        ax.yaxis.set_ticks_position('left')
    else:
        ax.set_yticklabels([])

    ax.set_title(" " + p, loc='left', y=1, pad=-15)
    
    ax.tick_params(axis='x')

    ax.bar_label(b, [f"{r:.1f}$\\times$" for r in ratio], fontsize=11)
    

def plot_overall_time(S2, S3, S4, plotting_func, result_dir, title):
    plt.rcParams.update(PLOT_PARAMS)

    fig, ax = plt.subplots(1, 3, layout='constrained', sharex=True)

    plotting_func(colors[0], ax[0], LABEL_2PC, *S2)
    plotting_func(colors[1], ax[1], LABEL_3PC, *S3)
    plotting_func(colors[2], ax[2], LABEL_4PC, *S4)

    output_path = result_dir / f"{title}.png"
    plt.savefig(Path(output_path), dpi=300)
    plt.close()


def process_queries(df):
    df = df[df['query'].isin(QUERY_ORDER)].copy()

    df.loc[:, 'median'] = df['median'] / 60

    df['query'] = pd.Categorical(df['query'], categories=QUERY_ORDER, ordered=True)

    df.sort_values(by='query', inplace=True)

    return df


if __name__ == "__main__":
    # Parse arguments
    # parser = argparse.ArgumentParser()
    # parser.add_argument('-S2', type=Path, help='2PC SimWAN data directory')
    # parser.add_argument('-W2', type=Path, help='2PC WAN data directory')
    # parser.add_argument('-S3', type=Path, help='3PC SimWAN data directory')
    # parser.add_argument('-W3', type=Path, help='3PC WAN data directory')
    # parser.add_argument('-S4', type=Path, help='4PC SimWAN data directory')
    # parser.add_argument('-W4', type=Path, help='4PC WAN data directory')

    # args = parser.parse_args()

    # s2_path = args.S2
    # w2_path = args.W2
    # s3_path = args.S3
    # w3_path = args.W3
    # s4_path = args.S4
    # w4_path = args.W4

    base_dir = Path("../benchmarks/paper-results/geo-wan/data")
    s2_path = base_dir / "sim-wan-2pc"
    w2_path = base_dir / "wan-2pc"
    s3_path = base_dir / "sim-wan-3pc"
    w3_path = base_dir / "wan-3pc"
    s4_path = base_dir / "sim-wan-4pc"
    w4_path = base_dir / "wan-4pc"

    result_dir = Path(s2_path).parent.parent.expanduser()

    # Process results
    S2_stats = pq.parse_files(s2_path)
    W2_stats = pq.parse_files(w2_path)
    S3_stats = pq.parse_files(s3_path)
    W3_stats = pq.parse_files(w3_path)
    S4_stats = pq.parse_files(s4_path)
    W4_stats = pq.parse_files(w4_path)

    S2_stats = process_queries(S2_stats)
    W2_stats = process_queries(W2_stats)
    S3_stats = process_queries(S3_stats)
    W3_stats = process_queries(W3_stats)
    S4_stats = process_queries(S4_stats)
    W4_stats = process_queries(W4_stats)

    # plot_overall_time(
    #     [S2_stats, W2_stats],
    #     [S3_stats, W3_stats],
    #     [S4_stats, W4_stats],
    #     plot_sublot_r,
    #     result_dir, "RealWAN-Ratio"
    # )

    plot_overall_time(
        [S2_stats, W2_stats],
        [S3_stats, W3_stats],
        [S4_stats, W4_stats],
        plot_sublot_t,
        result_dir, "RealWAN-Time"
    )

