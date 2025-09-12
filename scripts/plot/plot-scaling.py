#!/usr/bin/env python3

import os
import argparse
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import matplotlib.colors as mcolors
from pathlib import Path

import plot_query_experiments as pq

SCALING_QUERIES = [
    'Q22', 'Q12', 'Q21', 
]


LABELS = {
    2: 'SH-DM',
    3: 'SH-HM',
    4: 'Mal-HM'
}

style = ['////', '|||', r'\\\\']

def gen_lan_scaling(d):
    q10 = d[d['sf'] == 10].set_index('query')
    q1 = d[d['sf'] == 1].set_index('query')
    merged = q10.join(q1, rsuffix="_1", lsuffix="_10")
    merged['scaling'] = merged["median_10"] / merged["median_1"]    
    
    merged = merged.sort_values('median_10')

    fig, ax = plt.subplots(layout='constrained')

    plt.xticks(rotation=90)
    plt.ylabel("SF 10 / SF 1 ratio")
    plt.ylim(0, 15)
    plt.grid(axis="y", linestyle="--", alpha=0.6)

    ax.locator_params(nbins=3)

    b = plt.bar(merged.index, merged['scaling'], color=pq.PROTO_COLORS['SH-DM'])
    # merged['scaling'] = round(merged['scaling'], 1)
    # plt.bar_label(b, merged['scaling'])
    # plt.tight_layout()

def gen_scaling_plot(lan_data, wan_data):
    fig, ax = plt.subplots(1, 3, layout='constrained', sharey=True)

    # gen_scaling_subplot(lan_data, "LAN", ax[0])
    gen_scaling_subplot(wan_data[0], "SH-DM", ax[0])
    gen_scaling_subplot(wan_data[1], "SH-HM", ax[1])
    gen_scaling_subplot(wan_data[2], "Mal-HM", ax[2])
    ax[0].set_ylabel('Time (min)')

    # fig.text(0.01, 0.5, "SF10 / SF1 ratio", va='center', rotation='vertical')
    # fig.text(0.01, 0.5, "Time (sec)", va='center', rotation='vertical')

    yl = plt.ylim()
    plt.ylim(yl[0], yl[1] * 2.5)

    # plt.tight_layout()
    
def print_for_paper(title, table):
    print(title)

    QUERY_ORDER = SCALING_QUERIES
    
    with pd.option_context('display.float_format', '& {:.1f}'.format):
        print(table.loc[QUERY_ORDER].transpose())


def gen_scaling_subplot(data, title, ax):
    x = []
    y = []

    d = data[data['query'].isin(SCALING_QUERIES)]
    q10 = d[d['sf'] == 10].set_index('query')
    q1 = d[d['sf'] == 1].set_index('query')
    merged = q10.join(q1, rsuffix="_1", lsuffix="_10")

    merged = merged.loc[SCALING_QUERIES]

    sf10_data = merged['median_10'] / 60

    merged['scaling'] = merged["median_10"] / merged["median_1"]    
    merged = merged.drop(columns=['median_10', 'median_1', 'sf_10', 'sf_1'])
    print_for_paper(f"{title}", merged)

    N = len(SCALING_QUERIES)
    y = merged['scaling']

    b = ax.bar(merged.index, sf10_data, color=pq.PROTO_COLORS[title], edgecolor='w', linewidth=0.5,
            hatch=style[0])
    # labels = [f"{round(s)} min" for s in sf10_data]
    labels = [f"{round(s)}$\\times$" for s in y]
    ax.bar_label(b, labels, )

    # ax.set_xticks(x)
    # ax.set_xticklabels(np.tile(SCALING_QUERIES, len(data)), rotation=90)
    ax.set_yscale('log')
    
    # if title == "LAN":
    # ax.legend(loc='upper left', ncol=3)

    ax.set_title(" " + title, loc='left', y=1, pad=-15)

    # ax.set_yticks([0, 10])
    # ax.set_title(" " + title, loc='left', y=1, pad=-20)

    # ax.axhline(11.5, linestyle='--', color='k')

if __name__ == "__main__":
    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('-L2', type=Path, help='2PC LAN data directory')
    parser.add_argument('-L3', type=Path, help='3PC LAN data directory')
    parser.add_argument('-L4', type=Path, help='4PC LAN data directory')
    parser.add_argument('-W2', type=Path, help='2PC WAN data directory')
    parser.add_argument('-W3', type=Path, help='3PC WAN data directory')
    parser.add_argument('-W4', type=Path, help='4PC WAN data directory')
    parser.add_argument('-s', action='store_true', help='plot selected queries, or just 2PC LAN')
    parser.add_argument('title', type=str, help='Title/filename')
    args = parser.parse_args()

    # Directory paths
    result_dir = args.L2.parent.expanduser()

    # Process results
    L2 = pq.parse_files(args.L2, False, twopc=True)
    L3 = pq.parse_files(args.L3, False)
    L4 = pq.parse_files(args.L4, False)
    W2 = pq.parse_files(args.W2, False, twopc=True)
    W3 = pq.parse_files(args.W3, False)
    W4 = pq.parse_files(args.W4, False)

    plt.rcParams.update(pq.PLOT_PARAMS)

    if args.s:
        l2, l3, l4, w2, w3, w4 = (P[P['query'].isin(SCALING_QUERIES)].groupby(['query', 'sf']).agg(
            median=('overall_time', 'median')
        ).reset_index() for P in (L2, L3, L4, W2, W3, W4))

        gen_scaling_plot((l2, l3, l4), (w2, w3, w4))
    else:
        L2 = L2.groupby(['query', 'sf']).agg(
            median=('overall_time', 'median')
        ).reset_index()
        gen_lan_scaling(L2)

    plt.savefig(result_dir / args.title, dpi=300)
    plt.close()