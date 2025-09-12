import sys
import math
import re
from collections import defaultdict
import matplotlib.pyplot as plt
import math

sys.path.append('../../plot')
import plot_query_experiments as pq
pq.PLOT_PARAMS['figure.figsize'] = (6, 2.4)
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
    rows_to_totalonline = defaultdict(list)
    rows_to_totaloffline = defaultdict(list)

    # Regular expressions to match exponents and online times
    exponent_regex = re.compile(r"Iteration \d+: Exponent (\d+)")
    online_time_regex = re.compile(r"Spent ([0-9.]+) seconds")
    time_regex = re.compile(r"Spent ([0-9.]+) seconds .*? on the online phase and ([0-9.]+) seconds .*? on the preprocessing/offline phase")

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
                time_match = time_regex.search(line)
                if time_match:
                    online_time = float(time_match.group(1))
                    rows_to_totalonline[current_exponent].append(online_time)
                    offline_time = float(time_match.group(2))
                    rows_to_totaloffline[current_exponent].append(offline_time)
                else:
                    online_time_match = online_time_regex.search(line)
                    if online_time_match:
                        online_time = float(online_time_match.group(1))
                        rows_to_totalonline[current_exponent].append(online_time)

    avg_online_times = {rows: sum(times) / len(times) for rows, times in rows_to_totalonline.items()}
    avg_offline_times = {rows: sum(times) / len(times) for rows, times in rows_to_totaloffline.items()}
    return avg_online_times, avg_offline_times

def plot_data(ax, num_parties, secrecy_radix, mpspdz_data, x_values=None, log=False):
    # Separate the keys (log input sizes) and values (average times)
    if x_values is None:
        x_values = sorted(secrecy_radix.keys())
    y_values_radix = [secrecy_radix[x] for x in x_values]

    # size_labels = ['16k', '33k', '65k', '131k','262k', '524k', '1M', '2M', '4M', '8M', '17M', '34M']
    # size_labels = [f"$2^{{{x}}}$" for x in range(14, 27 + 1)]
    # size_labels = size_labels[:len(x_values)]

    setting_labels = ['SH-DM', 'SH-HM', 'Mal-HM']
    color = pq.PROTO_COLORS[setting_labels[num_parties - 2]]
    spdz_color = ["darkorchid", pq.TAB_COLORS['teal'], "darkorange"][num_parties - 2]
    
    # Plotting
    # ax.plot(x_values, y_values_quick, marker='s', linestyle='-', color=color)
    ax.plot(x_values, y_values_radix, marker='o', linestyle='--', color=color)
    
    if mpspdz_data is not None:
        y_values_2 = [mpspdz_data[x] for x in x_values[:-1]]
        ax.plot(x_values[:-1], y_values_2, marker='x', linestyle=':', color=spdz_color)
        ax.set_ylim(0.1, ax.get_ylim()[1] * 2)
        '''    
        if num_parties == 2: # add "CRASH" text at the last point
            ax.annotate(
                "[Crash]", 
                (x_values[-1], y_values_2[-1] * 2),  # Coordinates of the point
                textcoords="offset points", 
                xytext=(4, 0),  # Offset the text slightly
                ha="right", 
                fontsize=9,
            )
        else: # add the "OOM" text at the last point
            ax.annotate(
                "[OOM]", 
                (x_values[-1], y_values_2[-1] * 2),  # Coordinates of the point
                textcoords="offset points", 
                xytext=(3, -3),  # Offset the text slightly
                ha="right", 
                fontsize=9,
            )
        '''

        our_speedup = y_values_2[-1] / y_values_radix[-2]
        speedup_txt = f"{our_speedup:.1f}$\\times$  "
        print(f"{num_parties}PC: {our_speedup:.1f}x")

        middle = math.sqrt(y_values_2[-1] * y_values_radix[-2])
        
        if our_speedup > 2:
            spacing = 0.4
            ax.vlines(x_values[-2], y_values_radix[-2] * (1 + spacing), y_values_2[-1] / (1 + spacing), linestyle='--', color='gray')
            ax.text(x_values[-2], middle * 0.5, speedup_txt, ha='right', va='center', fontsize='11')
        else:
            ax.text(x_values[-2], y_values_2[-1], speedup_txt, ha='right', va='center', fontsize='11')

    tick_val, tick_label = get_labels(x_values)

    # Logarithmic x-axis label as powers of two
    ax.set_xticks(tick_label, tick_val)  # Format as powers of 2
    ax.set_title(f' {setting_labels[num_parties-2]}', y=1, pad=-15, loc='left')
    if log:
        ax.set_yscale('log')
    ax.grid(True, which="major", ls="--", linewidth=0.5)


if __name__ == '__main__':
    log = True
    
    if len(sys.argv) < 2:
        print('Usage: python plot_mpspdz_sort.py <home-directory>')
        exit()

    data_dir = sys.argv[1] + '/results/'
    
    file_us_2pc = data_dir + 'orq-2pc.txt'
    file_us_3pc = data_dir + 'orq-3pc.txt'
    file_us_4pc = data_dir + 'orq-4pc.txt'
    files_us = [file_us_2pc, file_us_3pc, file_us_4pc]

    file_mp_2pc = data_dir + '2pc-mpspdz.txt'
    file_mp_3pc = data_dir + '3pc-mpspdz.txt'
    file_mp_4pc = data_dir + '4pc-mpspdz.txt'
    files_mp = [file_mp_2pc, file_mp_3pc, file_mp_4pc]

    # file_us_2pc = './2pc-multi.txt'
    # file_us_3pc = './3pc-multi.txt'
    # file_us_4pc = './4pc-multi.txt'
    # files_us = [file_us_2pc, file_us_3pc, file_us_4pc]

    # file_mp_2pc = './2pc.txt'
    # file_mp_3pc = './3pc.txt'
    # file_mp_4pc = './4pc.txt'
    # files_mp = [file_mp_2pc, file_mp_3pc, file_mp_4pc]

    x_values_2pc = list(range(16, 22 + 1))
    x_values_3pc = list(range(16, 25 + 1))
    x_values_4pc = list(range(16, 20 + 1))
    x_values = [x_values_2pc, x_values_3pc, x_values_4pc]

    # fig, axs = plt.subplots(1, 3, figsize=(24, 4), sharey=True)
    fig, axs = plt.subplots(1, 3, layout='constrained', sharey=True)

    for num_parties in [2, 3, 4]:
        secrecy_data_radix = read_secrecy_data(files_us[num_parties-2], num_parties)
        mpspdz_online, mpspdz_offline = read_mpspdz_data(files_mp[num_parties-2])
        mpspdz_data = mpspdz_online
        if num_parties != 2:
            mpspdz_data = {key: mpspdz_online.get(key, 0) + mpspdz_offline.get(key, 0) for key in set(mpspdz_online) | set(mpspdz_offline)}
        plot_data(axs[num_parties-2], num_parties, secrecy_data_radix, mpspdz_data, x_values[num_parties-2], log)

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

    # axs[0].set_yticklabels([i.get_text().replace("-", "–") for i in axs[0].get_yticklabels()])

    axs[0].set_ylabel('Time (s)')

    axs[1].set_xlabel('Number of rows')

    plt.ylim(0.3, 1e4)
    # plt.tight_layout(rect=[0,0,1,0.9])
    plt.savefig('mpspdz-compare.png', dpi=300)
