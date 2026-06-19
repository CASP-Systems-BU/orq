import numpy as np
import matplotlib.pyplot as plt

import sys

sys.path.append('../../plot')
import plot_query_experiments as pq
pq.PLOT_PARAMS['figure.figsize'] = (6, 4)
plt.rcParams.update(pq.PLOT_PARAMS)

# --------------------
# Example data
# --------------------
exp = np.arange(16, 25 + 1)
x = 2 ** exp

crt_64 = np.array([101.3, 82.3, 71.9, 66.3, 63.2, 61.6, 60.7, 60.3, 60.0, 60.0])
gilboa_64 = np.array([144.5, 137.9, 134.2, 132.3, 131.2, 130.6, 130.3, 130.2, 130.1, 130.1])

crt_128 = np.array([89.4, 72.5, 63.2, 58.2, 55.5, 54.0, 53.2, 52.8, 52.6, 52.6])
gilboa_128 = np.array([272.5, 265.8, 262.2, 260.3, 259.2, 258.6, 258.3, 258.2, 258.1, 258.1])

# --------------------
# Plot
# --------------------
fig, ax = plt.subplots()

# CRT (solid, circles)
ax.plot(x, crt_64, color=pq.TAB_COLORS['green'], marker="o", linewidth=1, label="CRT 64")
ax.plot(x, crt_128, color=pq.TAB_COLORS['purple'], marker="x", linewidth=1, label="CRT 128")

# Gilboa (dotted, squares)
ax.plot(x, gilboa_64, color=pq.TAB_COLORS['green'], marker="o",
        linestyle=(0, (1,1)), linewidth=1, label="Gilboa 64")
ax.plot(x, gilboa_128, color=pq.TAB_COLORS['purple'], marker="x",
        linestyle=(0, (1,1)), linewidth=1, label="Gilboa 128")

# --------------------
# Axes formatting
# --------------------
ax.set_xscale("log", base=2)
ax.set_xticks(x)
ax.set_xticklabels([rf"$2^{{{e}}}$" for e in exp])

ax.set_xlabel("Input Size")
ax.set_ylabel("Communication overhead\n(communicated bits / output bit)")

ax.set_ylim(0, 300)
ax.legend(loc="best", ncol=2, frameon=True)

# --------------------
# Improvement arrows
# --------------------
label_x_loc = x[-1]

spacer = " " * 2

# 128-bit arrow
ax.annotate(
    "",
    xy=(label_x_loc, gilboa_128[-1] - 1),
    xytext=(label_x_loc, crt_128[-1] + 1),
    arrowprops=dict(arrowstyle="<|-|>", linewidth=1, color='k'),
)
ax.text(
    label_x_loc,
    0.6 * (gilboa_128[-1] + crt_128[-1]),
    "128-bit: 4.9$\\times$" + spacer,
    va="center",
    ha="right"
)

label_x_loc *= 0.8

# 64-bit arrow
ax.annotate(
    "",
    xy=(label_x_loc, gilboa_64[-1] - 1),
    xytext=(label_x_loc, crt_64[-1] + 1),
    arrowprops=dict(arrowstyle="<|-|>", linewidth=1, color='k'),
)
ax.text(
    label_x_loc,
    0.5 * (gilboa_64[-1] + crt_64[-1]),
    "64-bit: 2.2$\\times$" + spacer,
    va="center",
    ha="right"
)

# ax.set_xlim(x[0], label_x_loc * 1.2)

plt.tight_layout()
plt.savefig('triple-comm.png', dpi=600)
plt.show()
