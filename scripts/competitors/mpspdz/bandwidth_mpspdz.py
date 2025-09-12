import sys
import math
import re
from collections import defaultdict
import matplotlib.pyplot as plt
import math

sys.path.append('../../plot')
import plot_query_experiments as pq
pq.PLOT_PARAMS['figure.figsize'] = (6, 3)
plt.rcParams.update(pq.PLOT_PARAMS)

# colors = ['darkgreen', 'orangered', 'blue']
# plt.rcParams.update({
#     'font.size': 14,  # Set the global font size
#     'axes.titlesize': 20,  # Font size of axes titles
#     'axes.labelsize': 20,  # Font size of x and y labels
#     'xtick.labelsize': 14,  # Font size of x-axis tick labels
#     'ytick.labelsize': 14,  # Font size of y-axis tick labels
#     'legend.fontsize': 18,  # Font size of legend
#     'figure.titlesize': 20  # Font size of figure title
# })


# equal spacing but show the max
def get_labels(L):
    step = len(L) // 3
    s = L[::-1][::step][::-1]

    if s[0] != L[0]:
        s.insert(0, L[0])

    return [f"$2^{{{x}}}$" for x in s], s

    # return ([L[0], L[-1]], [0, len(L)])

def read_secrecy_data(filename, num_parties):
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
                    last_algo = 'Quicksort'
                if 'Radix Sort' in line:
                    rows_to_radix[current_rows].append(time)
                    last_algo = 'Radix Sort'
            
            if num_parties == 2:
                preproccessing_match = preprocessing_pattern.match(line)
                if preproccessing_match and current_rows is not None:
                    time = float(preproccessing_match.group(1))
                    if last_algo == 'Radix Sort':
                        rows_to_radix[current_rows][-1] -= time
                    
    
    avg_times_radix = {int(math.log2(rows)): sum(times) / len(times) for rows, times in rows_to_radix.items()}
    return avg_times_radix

def read_secrecy_preprocessed_perms(filename):
    exp_to_total = defaultdict(list)
    current_exponent = None

    with open(filename, 'r') as file:
        for line in file:
            line = line.strip()
            if line.startswith("---"):
                # Reset for the next exponent
                current_exponent = None
            elif line.startswith("Iteration"):
                current_exponent = int(line.split()[-1])  # Extract exponent value
            elif line.startswith("Total execution time"):
                time = float(line.split()[-2])  # Extract execution time
                exp_to_total[current_exponent].append(time)

    avg_times = {exp: sum(times) / len(times) for exp, times in exp_to_total.items()}
    return avg_times

def read_secrecy_preprocessed_triples(filename):
    exp_to_quick = defaultdict(list)
    exp_to_radix = defaultdict(list)
    current_exponent = None

    # Regular expressions to capture rows and TotalOnline times
    rows_pattern = re.compile(r"^==== (\d+) rows;")
    
    with open(filename, 'r') as file:
        for line in file:
            # Check if line indicates the numbr of rows
            rows_match = rows_pattern.match(line)
            if rows_match:
                current_exponent = int(rows_match.group(1))
                continue
            
            if line.startswith("[ SW] Radix-Preprocessing"):
                time = float(line.split()[-2])  # Extract execution time
                exp_to_radix[current_exponent].append(time)
            elif line.startswith("[ SW] Quick-Preprocessing"):
                time = float(line.split()[-2])  # Extract execution time
                exp_to_quick[current_exponent].append(time)

    avg_times_radix = {int(math.log2(rows)): sum(times) / len(times) for rows, times in exp_to_radix.items()}
    return avg_times_radix

def read_mpspdz_data(filename):
    rows_to_bandwidth = defaultdict(list)

    # Regular expressions to match exponents and online times
    exponent_regex = re.compile(r"Iteration \d+: Exponent (\d+)")
    online_time_regex = re.compile(r"Spent ([0-9.]+) seconds")
    bandwidth_regex = re.compile(
        r"Spent ([0-9.e+-]+) seconds \(([\d.e+-]+) MB, .*?\) on the online phase and ([0-9.e+-]+) seconds(?: \(([\d.e+-]+) MB, .*?\))? on the preprocessing/offline phase"
    )

    current_exponent = None

    with open(filename, 'r') as file:
        for line in file:
            # Check for a new exponent
            exponent_match = exponent_regex.search(line)
            if exponent_match:
                current_exponent = int(exponent_match.group(1))
                continue

            # Check for an online time (only if we are in an exponent's context)
            if current_exponent is not None:
                bandwidth_match = bandwidth_regex.search(line)
                if bandwidth_match:
                    value = float(bandwidth_match.group(2))
                    rows_to_bandwidth[current_exponent].append(value)

    avg_bandwidths = {rows: sum(times) / len(times) for rows, times in rows_to_bandwidth.items()}
    return avg_bandwidths

if __name__ == '__main__':
    log = True

    data_dir = '../../../benchmarks/mpspdz/'
    file_us_2pc = data_dir + '2pc-multi.txt'
    file_us_3pc = data_dir + '3pc-multi.txt'
    file_us_4pc = data_dir + '4pc-multi.txt'
    files_us = [file_us_2pc, file_us_3pc, file_us_4pc]

    file_mp_2pc = data_dir + '2pc.txt'
    file_mp_3pc = data_dir + '3pc.txt'
    file_mp_4pc = data_dir + '4pc.txt'
    files_mp = [file_mp_2pc, file_mp_3pc, file_mp_4pc]

    x_values_2pc = list(range(14, 22 + 1))
    x_values_3pc = list(range(14, 25 + 1))
    x_values_4pc = list(range(14, 20 + 1))
    x_values = [x_values_2pc, x_values_3pc, x_values_4pc]

    # fig, axs = plt.subplots(1, 3, figsize=(24, 4), sharey=True)
    fig, axs = plt.subplots(1, 3, layout='constrained', sharey=True)

    for num_parties in [2, 3, 4]:
        secrecy_data_radix = read_secrecy_data(files_us[num_parties-2], num_parties)
        mpspdz_data = read_mpspdz_data(files_mp[num_parties-2])
        print("| Input Size (2^k) | Bandwidth (MB) |")
        print("|------------------|----------------|")

        for k in sorted(mpspdz_data):
            bw = mpspdz_data[k] * num_parties
            print(f"| {k} | {bw:.3f}         |")

    exit()

    # Collect unique handles and labels
    handles, labels = [], []
    for ax in axs:
        for h, l in zip(*ax.get_legend_handles_labels()):
            if l not in labels:  # Avoid duplicates
                handles.append(h)
                labels.append(l)

    axs[1].plot([], [], marker='x', linestyle=':', color='dimgray', label='MP-SPDZ')
    # axs[2].plot([], [], marker='s', linestyle='-', color='dimgray', label='ORQ Quicksort')
    axs[1].plot([], [], marker='o', linestyle='--', color='dimgray', label='ORQ')
    axs[1].legend(loc='lower right')

    axs[0].set_yticklabels([i.get_text().replace("-", "–") for i in axs[0].get_yticklabels()])


    axs[0].set_ylabel('Time (s)')
    # plt.tight_layout(rect=[0,0,1,0.9])
    plt.savefig('mpspdz-compare.png', dpi=300)
