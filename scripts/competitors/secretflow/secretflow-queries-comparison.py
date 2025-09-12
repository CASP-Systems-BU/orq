import matplotlib.pyplot as plt
import numpy as np
import sys

sys.path.append('../../plot')
import plot_query_experiments as pq

plt.rcParams.update(pq.PLOT_PARAMS)

plt.rcParams['figure.figsize'] = (2.5, 2)

################################################
# Data
################################################

experiments = ['S1', 'S2', 'S3', 'S4', 'S5']
# experiments = ['Q6', 'Q1', 'Q14', 'Q12', 'Q1*']

# SecretFlow - 16M
secflow_times = np.array([69.53, 83, 167.8, 252.56, 963.59]) / 60

# ORQ - 16M
orq_times = np.array([1.195, 0.976, 146.790, 233.520, 624.050]) / 60

################################################

# Calculate speedup
speedup = [secflow_times[i] / orq_times[i] for i in range(len(experiments))]

# Bar positions
x = np.arange(len(experiments))

# Plotting
fig, ax = plt.subplots(layout='constrained')
# TODO: colors (using SBK hex for now, fix this)
# hatch = ['//////' if q in plaintext_queries else None for q in experiments]
bars1 = ax.bar(experiments, secflow_times, align='edge', width=-0.4, label='SecretFlow', color=pq.TAB_COLORS['purple'])
bars2 = ax.bar(experiments, orq_times, align='edge', width=0.4, label='ORQ', color='dimgray') # pq.PROTO_COLORS['SH-DM'])

def f(r):
    if r < 10:
        return f"{r:.1f}$\\times$"
    else:
        return f"{round(r)}$\\times$"

speedups = [f(n/c) for (n, c) in zip(secflow_times, orq_times)]

ax.bar_label(bars1, speedups, padding=2)

# Log scale for vertical axis
# ax.set_yscale('log')

# Labels and titles
# combine 
# ax.set_ylabel('Time (min)')
ax.set_xticks(x)
ax.set_xticklabels(experiments, rotation=45)
ax.legend(ncols=1)

# plt.ylim(None, yl[1] )
# print(plt.ylim())
# ax.set_yscale('log')

yl = plt.ylim()
print(yl)
plt.ylim(None, yl[1] * 1.2)

# add_speedup_labels(bars1, bars2)
 
# Show plot
# plt.tight_layout()
plt.savefig('secflow-comp.png', dpi=300)
