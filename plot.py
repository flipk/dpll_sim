#!/usr/bin/env python3

import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

# --- GLOBAL CONFIGURATION ---
DATA_FILE = "plot.dat"
UPDATE_INTERVAL_SEC = 0.45
MAX_LINES = 1400

# Column indices (0-indexed)
COL_1_IDX = 6  # 'adjust'
COL_2_IDX = 4  # 'accum error'
COL_3_IDX = 8  # 'standard deviation' (of adjust)

# Plot labels
COL_1_NAME = "adjustments"
COL_2_NAME = "accum error"
COL_3_NAME = "adjust stddev"
# ----------------------------

fig, ax = plt.subplots()
ax2 = ax.twinx()
line1, = ax.plot([], [],
                 marker='o',
                 markersize=2,
                 linestyle='None')
line2, = ax.plot([], [])
line3, = ax2.plot([], [])

lines = [line1, line2, line3]
labels = [COL_1_NAME, COL_2_NAME, COL_3_NAME]
ax.legend(lines, labels, loc='upper right')

ax.grid(True)
ax2.grid(True)

def update_plot(frame):
    try:
        with open(DATA_FILE, 'r') as f:
            # Efficiently grabs the last MAX_LINES
            if True:
                tail_lines = deque(f, maxlen=MAX_LINES)
            else:
                tail_lines = deque(f)
    except:
        # since we're using blit=False, we can just return
        # None until the file actually works.
        return None, None

    y1_data = []
    y2_data = []
    y3_data = []

    for line in tail_lines:
        columns = line.split()
        y1_data.append(float(columns[COL_1_IDX]))
        y2_data.append(float(columns[COL_2_IDX]))
        y3_data.append(float(columns[COL_3_IDX]))

    # Generating a simple sequential x-axis based on the number of lines read
    x_data = range(len(y1_data))

    line1.set_data(x_data, y1_data)
    line2.set_data(x_data, y2_data)
    line3.set_data(x_data, y3_data)

    ax.relim()
    ax2.relim()
    # Don't autoscale X margins dynamically
    ax.autoscale_view(scalex=False, scaley=True)
    ax2.autoscale_view(scalex=False, scaley=True)
    ax.set_xlim(0, len(y2_data))
    ax2.set_xlim(0, len(y2_data))

    return line1, line2, line3

# interval is expected in milliseconds
ani = animation.FuncAnimation(
    fig, 
    update_plot, 
    interval=UPDATE_INTERVAL_SEC * 1000, 
    blit=False, 
    cache_frame_data=False
)

if __name__ == "__main__":
    plt.show()



exit(0)
