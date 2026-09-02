#!/usr/bin/env python3

import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.ticker as ticker
from collections import deque

# --- GLOBAL CONFIGURATION ---
DATA_FILE = "plot.dat"
UPDATE_INTERVAL_SEC = 0.1
MAX_LINES = 2000

# Column indices (0-indexed)
COL_1_IDX = 8   # 'adjust'
COL_2_IDX = 4   # 'accum error'
COL_3_IDX = 10  # 'standard deviation' (of adjust)
COL_4_IDX = 12  # 'average' of adjust

# Plot labels
COL_1_NAME = "adjustments"
COL_2_NAME = "accumulated error"
COL_3_NAME = "adjustment stddev (right y axis)"
COL_4_NAME = "adjustment average (left y axis)"
# ----------------------------

# Create 2 vertically stacked subplots that share the X axis
fig, (ax_top, ax_bot_left) = plt.subplots(2, 1, sharex=True)

# Create a secondary right Y axis for the bottom subplot
ax_bot_right = ax_bot_left.twinx()

# Top Plot lines
line1, = ax_top.plot([], [], marker='o', markersize=2, linestyle='None')
line2, = ax_top.plot([], [])

# Bottom Plot lines
line4, = ax_bot_left.plot([], [], color='blue')
line3, = ax_bot_right.plot([], [], color='red')

# Legends
ax_top.legend([line1, line2], [COL_1_NAME, COL_2_NAME], loc='upper left')
ax_bot_left.legend([line4, line3], [COL_4_NAME, COL_3_NAME], loc='upper left')

ax_top.grid(True)
ax_bot_left.grid(True)

# Add this right after configuring your legends and grids
ax_bot_left.axhline(0, color='black', linewidth=2)

# Configure tick formatters for all three y-axes
for ax in (ax_top, ax_bot_left, ax_bot_right):
    formatter = ticker.ScalarFormatter(useMathText=True)
    formatter.set_scientific(True)
    formatter.set_powerlimits((-3, 3))
    ax.yaxis.set_major_formatter(formatter)

def update_plot(frame):
    # Core read logic directly implemented
    with open(DATA_FILE, 'r') as f:
        tail_lines = deque(f, maxlen=MAX_LINES)

    y1_data, y2_data, y3_data, y4_data = [], [], [], []

    for line in tail_lines:
        columns = line.split()
        if len(columns) > max(COL_1_IDX, COL_2_IDX, COL_3_IDX, COL_4_IDX):
            y1_data.append(float(columns[COL_1_IDX]))
            y2_data.append(float(columns[COL_2_IDX]))
            y3_data.append(float(columns[COL_3_IDX]))
            y4_data.append(float(columns[COL_4_IDX]))

    x_data = range(len(y1_data))

    line1.set_data(x_data, y1_data)
    line2.set_data(x_data, y2_data)
    line3.set_data(x_data, y3_data)
    line4.set_data(x_data, y4_data)

    # Recompute data limits and scale each axis independently
    for ax in (ax_top, ax_bot_left, ax_bot_right):
        ax.relim()
        ax.autoscale_view(scalex=False, scaley=True)
        ax.set_xlim(0, max(1, len(x_data)))

    return line1, line2, line3, line4


if __name__ == "__main__":
    ani = animation.FuncAnimation(
        fig,
        update_plot,
        interval=UPDATE_INTERVAL_SEC * 1000,
        blit=False,
        cache_frame_data=False
    )
    # Adjust layout padding so subplots don't overlap
    # plt.tight_layout()
    plt.subplots_adjust(left=0.12, right=0.88, top=0.92, bottom=0.08)
    plt.show()
