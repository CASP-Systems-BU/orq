import sys
import math
import re
from collections import defaultdict
import matplotlib.pyplot as plt
import numpy as np

sys.path.append('../../')
import plot_query_experiments as pq

pq.PLOT_PARAMS['figure.figsize'] = (6, 2.5)

# plt.rcParams.update({
#     'font.size': 14,  # Set the global font size
#     'axes.titlesize': 20,  # Font size of axes titles
#     'axes.labelsize': 20,  # Font size of x and y labels
#     'xtick.labelsize': 16,  # Font size of x-axis tick labels
#     'ytick.labelsize': 16,  # Font size of y-axis tick labels
#     'legend.fontsize': 18,  # Font size of legend
#     'figure.titlesize': 20  # Font size of figure title
# })

plt.rcParams.update(pq.PLOT_PARAMS)

def read_2pc_data(filename):
    rows_to_quick = defaultdict(list)
    rows_to_radix = defaultdict(list)
    current_rows = None
    
    # Regular expressions to capture rows and sort times
    rows_pattern = re.compile(r"^==== (\d+) rows;")
    sort_pattern = re.compile(r"\[\s*SW\] +(?:(?:Quicksort)|(?:Radix Sort)) +([\d.]+) +sec")
    preprocessing_pattern = re.compile(r"\[=PREPROC] +dm-dummyperm ([\d.]+) +sec")
    
    last_algo = 'Quicksort'
    
    with open(filename, 'r') as file:
        for line in file:
            # Check if line indicates the number of rows
            rows_match = rows_pattern.match(line)
            if rows_match:
                current_rows = int(rows_match.group(1))
            
            time_match = sort_pattern.match(line)
            if time_match and current_rows is not None:
                time = float(time_match.group(1))
                if 'Quicksort' in line:
                    rows_to_quick[current_rows].append(time)
                    last_algo = 'Quicksort'
                if 'Radix Sort' in line:
                    rows_to_radix[current_rows].append(time)
                    last_algo = 'Radix Sort'
            
            preproccessing_match = preprocessing_pattern.match(line)
            if preproccessing_match and current_rows is not None:
                time = float(preproccessing_match.group(1))
                if last_algo == 'Quicksort':
                    rows_to_quick[current_rows][-1] -= time
                elif last_algo == 'Radix Sort':
                    rows_to_radix[current_rows][-1] -= time
                    
    avg_times_quick = {int(math.log2(rows)): sum(times) / len(times) for rows, times in rows_to_quick.items()}
    avg_times_radix = {int(math.log2(rows)): sum(times) / len(times) for rows, times in rows_to_radix.items()}
    return avg_times_quick, avg_times_radix
    
def read_hm_data(filename):
    rows_to_quick = defaultdict(list)
    rows_to_radix = defaultdict(list)
    current_rows = None
    
    # Regular expressions to capture rows and sort times
    rows_pattern = re.compile(r"^==== (\d+) rows;")
    sort_pattern = re.compile(r"\[\s*SW\] +(?:(?:Quicksort)|(?:Radix Sort)) +([\d.]+) +sec")
    
    with open(filename, 'r') as file:
        for line in file:
            # Check if line indicates the number of rows
            rows_match = rows_pattern.match(line)
            if rows_match:
                current_rows = int(rows_match.group(1))

            # Check if line has a TotalOnline time and current rows is set
            time_match = sort_pattern.match(line)
            if time_match and current_rows is not None:
                time = float(time_match.group(1))
                if 'Quicksort' in line:
                    rows_to_quick[current_rows].append(time)
                if 'Radix Sort' in line:
                    rows_to_radix[current_rows].append(time)
                    
    avg_times_quick = {int(math.log2(rows)): sum(times) / len(times) for rows, times in rows_to_quick.items()}
    avg_times_radix = {int(math.log2(rows)): sum(times) / len(times) for rows, times in rows_to_radix.items()}
    return avg_times_quick, avg_times_radix


def plot(data_2pc_quick, data_2pc_radix, data_3pc_quick, data_3pc_radix, data_4pc_quick, data_4pc_radix, x_values):
    # Combine data for 2PC, 3PC, and 4PC
    benchmark_data = {
        '2PC': [data_2pc_quick, data_2pc_radix],
        '3PC': [data_3pc_quick, data_3pc_radix],
        '4PC': [data_4pc_quick, data_4pc_radix],
    }

    # size_labels = [f"{round(2 ** (20 + i) / 1e6)}M" for i in range(7 + 1)]
    size_labels = [f"$2^{{{x}}}$" for x in range(20, 30)]

    # Algorithm names
    labels = ['Quicksort', 'Radix Sort']

    # Define custom colors for each algorithm
    colors = [
        pq.TAB_COLORS['teal'],
        pq.PROTO_COLORS['SH-DM'], # blue
        pq.PROTO_COLORS['SH-HM'], # green
        '#2b7c40',
        pq.TAB_COLORS['orange'],
        pq.PROTO_COLORS['Mal-HM'], # red
    ]

    # instead of 2pc, 3pc, 4pc
    setting_labels = ['SH-DM', 'SH-HM', 'Mal-HM']

    # Define textures
    textures = ['/', 'o']

    # Bar width and spacing
    bar_width = 0.35  # Smaller bars for interleaving
    spacing = 0.07  # Space between quicksort and radixsort bars

    # Create the figure and axes
    fig, axes = plt.subplots(1, 3, layout='constrained', sharey=True)

    # Plot each subgraph
    color_index = 0
    num_parties = 2
    for ax, (key, datasets) in zip(axes, benchmark_data.items()):
        x = np.arange(len(x_values))  # X positions for input sizes

        # Plot bars for quicksort and radixsort
        for i, data in enumerate(datasets):
            positions = x + i * (bar_width + spacing)  # Offset for interleaving bars
            values = [data.get(size, None) for size in x_values]
            marker = 'd'
            linestyle = '-'
            if labels[i].startswith('Radix'):
                marker = 'o'
                linestyle = '--'

            #ax.bar(positions, values, bar_width, label=f"{key} {labels[i]}", color=colors[color_index], hatch=textures[i])
            ax.plot(x_values, values, marker=marker, linestyle=linestyle, color=colors[color_index])
            color_index += 1

        # Set x-ticks and labels
        #ax.set_xticks(x + (bar_width + spacing) / 2)  # Centered ticks
        ax.set_xticks(x_values[1::2])
        ax.set_xticklabels(size_labels[1::2])
        ax.set_title(f' {setting_labels[num_parties-2]}', loc='left', y=1, pad='-15')
        #ax.yaxis.grid(True, which="major", linestyle='--', linewidth=0.5)
        ax.grid(True, which="major", ls="--", linewidth=0.5)

        num_parties += 1

    ax.plot([], [], marker='o', linestyle='--', color='dimgray', label='Radixsort')
    ax.plot([], [], marker='d', color='dimgray', label='Quicksort')
        
    # Add shared labels and legend
    axes[0].set_ylabel("Time (s)")
    #fig.suptitle("Comparison of Sorting Protocols", fontsize=14)
    #axes[1].set_xlabel("Number of Rows (powers of 2)", fontsize=12)
    axes[0].set_yscale('log')
    ax.legend(loc='lower right')

    axes[1].set_xlabel("Number of rows")

    # Adjust layout and show the plot
    plt.savefig('./plot-sort.png', dpi=300)


if __name__ == '__main__':
    data_dir = '../../../benchmarks/sorting/main-results/'
    file_2pc = data_dir + '2pc.txt'
    file_3pc = data_dir + '3pc.txt'
    file_4pc = data_dir + '4pc.txt'

    data_2pc_quick, data_2pc_radix = read_2pc_data(file_2pc)
    data_3pc_quick, data_3pc_radix = read_hm_data(file_3pc)
    data_4pc_quick, data_4pc_radix = read_hm_data(file_4pc)

    keys = data_2pc_quick.keys()
    print(f"2PC QS {min(keys)} - {max(keys)}")
    keys = data_2pc_radix.keys()
    print(f"2PC RS {min(keys)} - {max(keys)}")
    keys = data_3pc_quick.keys()
    print(f"3PC QS {min(keys)} - {max(keys)}")
    keys = data_3pc_radix.keys()
    print(f"3PC RS {min(keys)} - {max(keys)}")
    keys = data_4pc_quick.keys()
    print(f"4PC QS {min(keys)} - {max(keys)}")
    keys = data_4pc_radix.keys()
    print(f"4PC RS {min(keys)} - {max(keys)}")

    x_values = list(range(20, 30 + 1))

    plot(data_2pc_quick, data_2pc_radix, data_3pc_quick, data_3pc_radix, data_4pc_quick, data_4pc_radix, x_values)