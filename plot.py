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
                 linestyle='None',
                 label=COL_1_NAME)
line2, = ax.plot([], [], label=COL_2_NAME)
line3, = ax2.plot([], [], label=COL_3_NAME)

ax.legend(loc='upper right')
ax2.legend(loc='upper left')
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


# consider the below if i want to change the grid
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.ticker import MultipleLocator  # <-- ADD THIS IMPORT
from collections import deque

# ... keep your existing globals ...

fig, ax = plt.subplots()
line1, = ax.plot([], [], lw=1, label=COL_1_NAME)
line2, = ax.plot([], [], marker='o', linestyle='None', ms=3, label=COL_2_NAME)
ax.legend(loc='upper right')

# 1. Turn on the grid
ax.grid(True)

# 2. Control the spacing
# This forces an X grid line every 50 units, and a Y grid line every 10 units
ax.xaxis.set_major_locator(MultipleLocator(50))
ax.yaxis.set_major_locator(MultipleLocator(10))
