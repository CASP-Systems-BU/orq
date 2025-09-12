import os
import re
import argparse
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import matplotlib.ticker as ticker
from pathlib import Path

import plot_query_experiments as pq

os.chdir(os.path.dirname(os.path.abspath(__file__)))

pq.PLOT_PARAMS['figure.figsize'] = (6, 2.2)

PLOT_PARAMS = pq.PLOT_PARAMS

MINIMUM_INPUT_SIZE = 10000

# Data toggles
ORQ_WAN_BW_CONFIG="100Mbps"
INLCUDE_PERM_GEN=True

ORQ_MULTI_THREADED_PERM_TIME = {
    "32b": {
        "100000": 0.11828,
        "1000000": 0.97856,
        "10000000": 14.948,
    },
    "64b": {
        "100000": 0.26256,
        "1000000": 2.4386,
        "10000000": 33.67
    }
}

column_order = ['SCQL_SBK', 'ORQ_RS_64b', 'SCQL_SBK_valid', 'ORQ_RS_32b']

COLUMN_DETAILS = {
    "SCQL_SBK": (
        "SecretFlow: SBK",
        '#7c4d79', 
        ''
    ),
    "SCQL_SBK_valid": (
        "SecretFlow: SBK$\_$valid",
        pq.TAB_COLORS['purple'],
        '----'
    ),
    "ORQ_RS_64b": (
        "ORQ: RS (64b)",
        pq.PROTO_COLORS['SH-DM'],
        ''
    ),
    "ORQ_RS_32b": (
        "ORQ: RS (32b)",
        '#5fa2ce',
        '----'
    ),
}

def parse_files(data_dir):
    data = []
    
    for file_name in os.listdir(data_dir):
        if file_name.startswith("sf"):
            row_size = None
            file_path = os.path.join(data_dir, file_name)
            with open(file_path, 'r') as file:
                content = file.readlines()
                for i, line in enumerate(content):

                    row_size_match = re.search(r"Sort:\s*(\d+)", content[i])
                    if row_size_match:
                        row_size = int(row_size_match.group(1))

                    sbk_match = re.search(r"SortByKey time ([\d.]+)", content[i])
                    if sbk_match:
                        sbk_time = float(sbk_match.group(1))
                        perm_time = None

                        for offset in [1, 2, 3, 4, 5]:
                            match = re.search(r"Permutation_time:\s*(\d+)\s*ms", content[i + offset])
                            if match:
                                perm_time = int(match.group(1))
                                break

                        if perm_time is not None:
                            if not INLCUDE_PERM_GEN:
                                sbk_time -= perm_time
                            data.append([row_size, "SCQL_SBK", sbk_time / 1000])
                        else:
                            raise ValueError("Permutation time not found")
                    
                    sbk_valid_match = re.search(r"SortByKey with valid bits \(\d+\) time ([\d.]+)", content[i])
                    if sbk_valid_match:
                        sbk_valid_time = float(sbk_valid_match.group(1))
                        perm_time = None

                        for offset in [1, 2, 3, 4, 5]:
                            match = re.search(r"Permutation_time:\s*(\d+)\s*ms", content[i + offset])
                            if match:
                                perm_time = int(match.group(1))
                                break

                        if perm_time is not None:
                            if not INLCUDE_PERM_GEN:
                                sbk_valid_time -= perm_time
                            data.append([row_size, "SCQL_SBK_valid", sbk_valid_time / 1000])
                        else:
                            raise ValueError("Permutation time not found")
        
        if file_name.startswith("orq"):
            row_size = None
            hyphen_split = (file_name.split(".")[0]).split("-")

            bitwidth = hyphen_split[1]

            if len(hyphen_split) == 4:
                if not (hyphen_split[3] == ORQ_WAN_BW_CONFIG):
                    print(f"Skipping {hyphen_split[3]} results")
                    continue

            file_path = os.path.join(data_dir, file_name)
            with open(file_path, 'r') as file:
                content = file.readlines()
                
                for i, line in enumerate(content):

                    row_size_match = re.search(r"==== (\d+) rows;", content[i])
                    if row_size_match:
                        row_size = int(row_size_match.group(1))

                    radix_sort_match = re.search(r"\[ SW\]\s+Radix Sort ([\d.]+)\s+sec", line)
                    if radix_sort_match:
                        radix_time = float(radix_sort_match.group(1))
                        
                        if i + 1 < len(content):
                            dummyperm_match = re.search(r"dm-dummyperm ([\d.]+)\s+sec", content[i + 1])
                            if dummyperm_match:
                                dm_dummy_time = float(dummyperm_match.group(1))
                                
                                adjusted_time = radix_time - dm_dummy_time

                                if INLCUDE_PERM_GEN:
                                    adjusted_time += ORQ_MULTI_THREADED_PERM_TIME[bitwidth][str(row_size)]

                                data.append([row_size, f"ORQ_RS_{bitwidth}", adjusted_time])

                        # data.append([row_size, f"ORQ_RS_{bitwidth}", radix_time])
        
    df = pd.DataFrame(data, columns=["InputSize", "SortType", "Time"])
    return df


def format_size(size):
    if size >= 1_000_000:
        return f'{size // 1_000_000}M'
    elif size >= 1_000:
        return f'{size // 1_000}k'
    else:
        return str(size)

def speedup_fmt(speed, timeout=False):
    if timeout:
        return [('Crash' if np.isnan(s) else '') for s in speed]
    
    return [('' if np.isnan(s) else f"{s:.1f}$\\times$") for s in speed]


def plot_sort(ax, df, show_y, plot_label):
    bar_width = 0.22
    x = np.arange(len(df["InputSize"]))

    df['speedup_64'] = df['SCQL_SBK'] / df['ORQ_RS_64b']
    df['speedup_32'] = df['SCQL_SBK_valid'] / df['ORQ_RS_32b']

    bars = []
    for i, col in enumerate(column_order):
        label = COLUMN_DETAILS.get(col)[0]
        color = COLUMN_DETAILS.get(col)[1]
        hatch = COLUMN_DETAILS.get(col)[2]
        bar = ax.bar(
            x + i * bar_width, df[col],
            width=bar_width, label=label, color=color, 
            edgecolor='white',
            hatch=hatch
        )

        if col == 'SCQL_SBK':
            speedup = df['speedup_64']
        else:
            speedup = df['speedup_32']
        
        if not label.startswith('ORQ'):
            labels = ax.bar_label(bar, speedup_fmt(speedup), size=10, padding=2, rotation=0)
            for b, d in zip(bar, df[col]):
                if np.isnan(d):
                    print(d)
                    # ax.text(b.get_x() + 0.055, 0, " Crash", rotation=90, size=9, va='bottom')
                    ax.text(b.get_x() + 0.12, 0.08, "Crash",
                            rotation=90, ha="center", va="bottom", size=12, transform=ax.get_xaxis_transform())

            for l in labels:
                lx, ly = l.get_position()
                l.set_position((lx + 1, ly))
        
    ax.set_yscale('log')

    if show_y:
        ax.set_ylabel("Time (s)")

    ax.set_xticks(x + 1.5 * bar_width)

    ax.set_xticklabels([format_size(size) for size in df["InputSize"]], rotation=0, ha='center')

    ax.grid(True, which='major', linestyle='--', linewidth=0.5, axis='y')


def plot(lan, wan, plot_type="lan"):
    plt.rcParams.update(PLOT_PARAMS)

    if plot_type == "lan":
        data = lan
    elif plot_type == "wan":
        data = wan

    fig, ax = plt.subplots(layout='constrained')
    plot_sort(ax, data, True, "")
    ax.margins(y=0.14)

    def replace_minus(y, _):
        e = int(np.log10(y))
        if e < 0:
            e = f'–{-e}'

        return f"$\\mathdefault{{10^{{{e}}}}}$"

    ax.yaxis.set_major_formatter(ticker.FuncFormatter(replace_minus))

    handles, labels = ax.get_legend_handles_labels()

    fig.legend(handles, labels, loc='center left', 
            bbox_to_anchor=(0.1, 0.78), ncol=1, fontsize=9)

    fig.text(0.49, 0.02, "Number of rows", ha='center', va='top')

    output_path = f"secretflow-sorting.png"
    plt.savefig(output_path, bbox_inches='tight', dpi=300)
    plt.close()


if __name__ == "__main__":

    # parser = argparse.ArgumentParser(description="Plot SCQL comparison data")
    # parser.add_argument('-L', type=Path, help='LAN data directory')
    # parser.add_argument('-W', type=Path, help='WAN data directory')
    # args = parser.parse_args()

    # lan_path = args.L
    # wan_path = args.W

    base_dir = Path("../../results/secretflow-sort/")
    # lan_path = base_dir / "lan"
    # wan_path = base_dir / "wan"
    
    lan_df = parse_files(base_dir)
    lan_df = lan_df.groupby(["InputSize", "SortType"])["Time"].mean().reset_index()
    lan_df = lan_df.pivot(index="InputSize", columns="SortType", values="Time").reset_index()
    lan_df = lan_df[lan_df["InputSize"] >= MINIMUM_INPUT_SIZE]

    # wan_df = parse_files(wan_path)
    # wan_df = wan_df.groupby(["InputSize", "SortType"])["Time"].mean().reset_index()
    # wan_df = wan_df.pivot(index="InputSize", columns="SortType", values="Time").reset_index()
    # wan_df = wan_df[wan_df["InputSize"] >= MINIMUM_INPUT_SIZE]
    
    print(lan_df)
    # print(wan_df)

    plot(lan_df, None, "lan")
    # plot(lan_df, wan_df, "wan")

