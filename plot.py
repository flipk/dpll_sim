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
COL_1_IDX = 4  # 'adjust'
COL_2_IDX = 2  # 'accum error'
COL_3_IDX = 5  # 'standard deviation' (of adjust)
COL_4_IDX = 6  # 'average' of adjust

# Plot labels
COL_1_NAME = "adjustments"
COL_2_NAME = "accumulated error"
COL_3_NAME = "adjustment stddev"
COL_4_NAME = "adjustment average"
# ----------------------------

# Create 2 vertically stacked subplots that share the X axis
fig, (ax_top_left, ax_bot_left) = plt.subplots(2, 1, sharex=True)

# Create secondary right Y axes
ax_top_right = ax_top_left.twinx()
ax_bot_right = ax_bot_left.twinx()

# Plot lines
# adjustments, top left
line1, = ax_top_left.plot([], [], color='blue',
                          marker='o', markersize=2, linestyle='None')
# accum error, bot left
line2, = ax_bot_left.plot([], [], color='blue')
# adjustment standard deviation, bot right
line3, = ax_bot_right.plot([], [], color='red')
# adjustment average, top right
line4, = ax_top_right.plot([], [], color='red')

# Legends
ax_top_left .legend([line1], [COL_1_NAME],
                    loc='upper left' , framealpha=0.9)
ax_top_right.legend([line4], [COL_4_NAME],
                    loc='upper right' , framealpha=0.9)
ax_bot_left .legend([line2], [COL_2_NAME],
                    loc='upper left' , framealpha=0.9)
ax_bot_right.legend([line3], [COL_3_NAME],
                    loc='upper right', framealpha=0.9)


# only the left Y axes have grids
ax_top_left.grid(True)
ax_bot_left.grid(True)

# emphasized zero lines for accumulated error and adj average
ax_top_left.axhline(0, color='black', linewidth=2)
ax_bot_left.axhline(0, color='black', linewidth=2)

# engineering notation (powers of 10 that are multiples of 3)
for ax in (ax_top_left, ax_top_right, ax_bot_left, ax_bot_right):
    ax.yaxis.set_major_formatter(ticker.EngFormatter(places=1,
                                                     useMathText=True))

def update_plot(frame):
    # Core read logic directly implemented
    try:
        with open(DATA_FILE, 'r') as f:
            tail_lines = deque(f, maxlen=MAX_LINES)
    except:
        # if file doesn't exist, keep polling and maybe
        # it will soon appear.
        return None

    if len(tail_lines) == 0:
        # file is empty, bail out
        return None

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
    for ax in (ax_top_left, ax_top_right, ax_bot_left, ax_bot_right):
        ax.relim()
        ax.autoscale_view(scalex=False, scaley=True)
        ax.set_xlim(0, max(1, len(x_data)))

    # Align zero line on the top subplots
    l_min = min(y1_data)
    l_max = max(y1_data)
    l_bound = max(l_max, -l_min)
    ax_top_left.set_ylim(-l_bound, l_bound)

    r_min = min(y4_data)
    r_max = max(y4_data)
    r_bound = max(r_max, -r_min)
    ax_top_right.set_ylim(-r_bound, r_bound)

    # stddev plot always should have 0 as the bottom
    # Handle the case where max(y3_data) might be 0 to avoid a UserWarning
    max_y3 = max(y3_data) if y3_data else 1
    ax_bot_right.set_ylim(0, max_y3 if max_y3 > 0 else 1)

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
    plt.tight_layout()
    plt.show()
