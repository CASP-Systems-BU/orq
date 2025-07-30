import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import sys

sys.path.append('..')
import plot_query_experiments as pq

plt.rcParams.update(pq.PLOT_PARAMS)
plt.rcParams['figure.figsize'] = (4, 2.5)

################################################
# Data
################################################
experiments = ['Q4', 'Q6', 'Q13', 'Comorb.', 'C.Diff', 'Aspirin', 'Credit', 'Pwd']

# Secrecy NSDI'23
nsdi_times = [7462.676, 2.8, 14062.969, 1524.7, 2022.3, 2921.0, 831.2, 787.5]

# For -1
# current_times = [0, 5.1, 0, 414.2, 207.1, 6.2, 149.1, 0]

# For 64k
# current_times = [19.8, 3.3, 42.61, 339.9, 189.1, 6.3, 130.1, 148.6]

# For 64k, multi-threaded ORQ
current_times = [12.9, 0.8, 17.0, 85.3, 47.6, 6.1, 34.2, 38.1]
################################################


################################################
# Reorder data
################################################
experiments_order = ['Q6', 'Pwd', 'Credit', 'Comorb.', 'C.Diff', 'Aspirin', 'Q4', 'Q13']

def reorder_list(lst, order):
    return [lst[experiments.index(o)] for o in order]

nsdi_times = reorder_list(nsdi_times, experiments_order)
current_times = reorder_list(current_times, experiments_order)
experiments = reorder_list(experiments, experiments_order)
################################################


################################################
# Convert to minutes
################################################
def to_minutes(lst):
    return [t / 60 for t in lst]

nsdi_times2 = to_minutes(nsdi_times)
current_times2 = to_minutes(current_times)
################################################

# Calculate speedup
speedup = [nsdi_times[i] / current_times[i] for i in range(len(experiments))]

# Bar positions
x = np.arange(len(experiments))

# Plotting
fig, ax = plt.subplots(layout='constrained')
bars1 = ax.bar(experiments, nsdi_times2, align='edge', width=-0.4, label='Secrecy', color=pq.TAB_COLORS['purple'])
bars2 = ax.bar(experiments, current_times2, align='edge', width=0.4, label='ORQ', color='dimgray') # pq.PROTO_COLORS['SH-HM'])

def f(r):
    if r < 10:
        return f"{r:.1f}$\\times$"
    else:
        return f"{round(r)}$\\times$"

speedups = [f(n/c) for (n, c) in zip(nsdi_times, current_times)]

ax.bar_label(bars1, speedups, padding=2)

# Labels and titles
ax.set_ylabel('Time (min)')
ax.set_xticks(x)
ax.set_xticklabels(experiments, rotation=45, ha='right')

ax.set_yscale('log')

# ax.set_yticklabels([-1, 0, 1, 2, 3])

# yl = plt.ylim()
# print(yl)
# plt.ylim(yl[0], yl[1] * 10)
plt.ylim(1e-2, 1e4)

def replace_minus(y, _):
    e = int(np.log10(y))
    if e < 0:
        e = f'–{-e}'

    return f"$\\mathdefault{{10^{{{e}}}}}$"

ax.yaxis.set_major_formatter(ticker.FuncFormatter(replace_minus))

# ax.yaxis.set_major_locator(loc)
# ax.yaxis.set_major_formatter(ticker.LogFormatterSciNotation())

# ticks = [-2, -1, 0, 1, 2, 3, 4]

# ax.set_yticks([10**t for t in ticks])
# ax.set_yticklabels([f"$10^{{{t}}}$" if t % 2 == 0 else "" for t in ticks])


ax.legend(ncols=2)

## PROBLEMATIC LINE
# https://github.com/matplotlib/matplotlib/issues/29284
# ax.set_yticklabels([i.get_text().replace("-", "–") for i in ax.get_yticklabels()])

# add_speedup_labels(bars1, bars2)
 
# Show plot
# plt.tight_layout()
plt.savefig('secrecy-comp.png', dpi=300)
