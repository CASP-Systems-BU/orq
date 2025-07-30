import math
import matplotlib.pyplot as plt
import sys

sys.path.append('../../')
import plot_query_experiments as pq
pq.PLOT_PARAMS['figure.figsize'] = (6, 5 )
plt.rcParams.update(pq.PLOT_PARAMS)

# plt.rcParams.update({
#     'font.size': 14,  # Set the global font size
#     'axes.titlesize': 20,  # Font size of axes titles
#     'axes.labelsize': 20,  # Font size of x and y labels
#     'xtick.labelsize': 16,  # Font size of x-axis tick labels
#     'ytick.labelsize': 16,  # Font size of y-axis tick labels
#     'legend.fontsize': 18,  # Font size of legend
#     'figure.titlesize': 20  # Font size of figure title
# })

def read_data(file_path):
    results = {}
    current_size = None

    with open(file_path, 'r') as file:
        for line in file:
            line = line.strip()
            # Match lines that define input sizes
            if line.startswith('====') and 'rows' in line:
                current_size = int(line.split('rows')[0].strip('= ').strip())
                if current_size not in results:
                    results[current_size] = {'Ours': [], 'AHI+22': []}
            # Match lines for 'Ours'
            elif '[ SW]' in line and 'Ours' in line:
                time = float(line.split()[-2])  # Extract the second last element as time
                results[current_size]['Ours'].append(time)
            # Match lines for 'AHI+22'
            elif '[ SW]' in line and 'AHI+22' in line:
                time = float(line.split()[-2])  # Extract the second last element as time
                results[current_size]['AHI+22'].append(time)

    # Compute averages and map to log2(size)
    ours_map = {}
    ahi22_map = {}

    for size, times in results.items():
        log2_size = int(math.log2(size))
        ours_avg = sum(times['Ours']) / len(times['Ours']) if times['Ours'] else 0
        ahi22_avg = sum(times['AHI+22']) / len(times['AHI+22']) if times['AHI+22'] else 0

        ours_map[log2_size] = ours_avg
        ahi22_map[log2_size] = ahi22_avg

    return ours_map, ahi22_map

def plot_data(our_lan, ahi_lan, our_wan, ahi_wan, bitwidth):
    # Separate the keys (log input sizes) and values (average times)
    # x_values = sorted(our_lan.keys())
    s = min(len(ahi_lan), len(ahi_wan))
    x_values = list(range(20, 28))[:s]
    size_labels = [f"$2^{{{x}}}$" for x in x_values]
    
    our_lan_y_values = list(our_lan.values())[:s]
    our_wan_y_values = list(our_wan.values())[:s]

    ahi_lan_y_values = list(ahi_lan.values())[:s]
    ahi_wan_y_values = list(ahi_wan.values())[:s]

    # Plotting
    # lan values in dotted lines, wan values in solid lines
    plt.plot(x_values, our_lan_y_values, marker='o', linestyle='-', color=pq.PROTO_COLORS['SH-HM'], label='ORQ (LAN)')
    plt.plot(x_values, our_wan_y_values, marker='o', linestyle='--', color=pq.PROTO_COLORS['SH-HM'], label='ORQ (WAN)')
    plt.plot(x_values, ahi_lan_y_values, marker='x', linestyle='-', color='darkgreen', label='AHI+22 (LAN)')
    plt.plot(x_values, ahi_wan_y_values, marker='x', linestyle='--', color='darkgreen', label='AHI+22 (WAN)')
    
    # Logarithmic x-axis label as powers of two
    plt.xticks(x_values, labels=size_labels)
    #plt.ylim(bottom=0)  # Ensure y-axis starts at 0
    plt.yscale('log')
    #plt.xlabel('Input Size', labelpad=10)
    # plt.ylabel('Time (sec)')
    plt.title(f' {bitwidth} bit', loc='left', y=1, pad=-15)
    plt.grid(True, which='major', ls='--', linewidth=0.5)


import sys
if __name__ == '__main__':
    '''
    This file produces two graphs, one for 32 bit and one for 64 bit.
    You toggle them by passing 32 or 64 as a command line parameter (default is 32).
    '''

    lan_file = None
    wan_file = None
    
    fig, ax = plt.subplots(nrows=2, ncols=1, sharex=True)

    ours_lan, ahi22_lan = read_data('./64b-lan.txt')
    ours_wan, ahi22_wan = read_data('./64b-wan.txt')
    plt.sca(ax[1])
    plot_data(ours_lan, ahi22_lan, ours_wan, ahi22_wan, 64)

    ax[1].legend(ncols=2, loc='lower right')

    ours_lan, ahi22_lan = read_data('./32b-lan.txt')
    ours_wan, ahi22_wan = read_data('./32b-wan.txt')
    plt.sca(ax[0])
    plot_data(ours_lan, ahi22_lan, ours_wan, ahi22_wan, 32)

    fig.text(x=0.02, y=0.5, s="Time (s)", rotation=90)

    plt.tight_layout(rect=[0.03, 0, 1, 1])
    plt.savefig('ahi22-comp', dpi=300)


