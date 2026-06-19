import matplotlib.pyplot as plt
import numpy as np
import sys

sys.path.append('../../plot')
import plot_query_experiments as pq
pq.PLOT_PARAMS['figure.figsize'] = (6, 3)
plt.rcParams.update(pq.PLOT_PARAMS)

# --------------------
# Example data
# --------------------
bitwidth = np.array([8, 16, 32, 64, 128])

gilboa = np.array([6.92398, 6.11341, 6.05835, 6.4932, 12.09184])
crt = np.array([91.03035, 39.89353, 26.91915, 20.84233, 17.86915])
and_gate = np.array([11.44003, 7.68333, 8.07626, 8.35445, 8.10653])

gilboa_min = 6.1


# --------------------
# Plot
# --------------------
fig, ax = plt.subplots()

# Minimum Gilboa line (behind everything)
ax.axhline(
    gilboa_min,
    xmin=0.5,
    linestyle="--",
    linewidth=1,
    color="gray",
    zorder=0,
)

ms = 2

# Main curves
ax.plot(bitwidth, crt, label="MUL: CRT", color=pq.TAB_COLORS['red'])
ax.plot(bitwidth, gilboa, label="MUL: Gilboa", color=pq.TAB_COLORS['orange'])
ax.plot(bitwidth, and_gate, label="AND", color=pq.TAB_COLORS['teal'])

# --------------------
# Axes formatting
# --------------------
ax.set_xscale("log", base=2)
ax.set_xticks(bitwidth)
ax.get_xaxis().set_major_formatter(plt.ScalarFormatter())

ax.set_xlabel("Bitwidth")
ax.set_ylabel("Normalized Runtime (ns/bit)")

ax.set_xlim(8, 128)
ax.set_ylim(0, 100)

ax.legend(loc="upper right")


# --------------------
# Right-side annotations
# --------------------
x_text = 140  # just outside final tick

ax.text(
    x_text,
    crt[-1],
    f"{min(crt):.1f} ns ({1e3/min(crt):.1f} Mbps)",
    va="center",
    ha="left",
    fontsize=9,
)

ax.text(
    x_text,
    gilboa_min,
    f"{min(gilboa):.1f} ns ({1e3/min(gilboa):.1f} Mbps)",
    va="center",
    ha="left",
    fontsize=9,
)

plt.tight_layout()
plt.savefig('triple-runtime.png', dpi=600)
plt.show()
