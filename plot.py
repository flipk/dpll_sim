#!/usr/bin/env python3

import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

# --- GLOBAL CONFIGURATION ---
DATA_FILE = "plot.dat"
UPDATE_INTERVAL_SEC = 0.2
MAX_LINES = 1500

# Column indices (0-indexed: 4 is the 5th column, 6 is the 7th)
COL_1_IDX = 6
COL_2_IDX = 4

# Plot labels
COL_1_NAME = "adjustments"
COL_2_NAME = "accum error"
# ----------------------------

fig, ax = plt.subplots()
line1, = ax.plot([], [],
                 marker='o',
                 markersize=2,
                 linestyle='None',
                 label=COL_1_NAME)
line2, = ax.plot([], [], label=COL_2_NAME)
ax.legend(loc='upper right')
ax.grid(True)

def update_plot(frame):
    with open(DATA_FILE, 'r') as f:
        # Efficiently grabs the last MAX_LINES
        if True:
            tail_lines = deque(f, maxlen=MAX_LINES)
        else:
            tail_lines = deque(f)

    y1_data = []
    y2_data = []

    for line in tail_lines:
        columns = line.split()
        y1_data.append(float(columns[COL_1_IDX]))
        y2_data.append(float(columns[COL_2_IDX]))

    # Generating a simple sequential x-axis based on the number of lines read
    x_data = range(len(y1_data))

    line1.set_data(x_data, y1_data)
    line2.set_data(x_data, y2_data)

    if False:
        ax.relim()
        ax.autoscale_view()
    else:
        ax.relim()
        # Don't autoscale X margins dynamically
        ax.autoscale_view(scalex=False, scaley=True)
        # Force X to exactly 0 to 499
        ax.set_xlim(0, len(y2_data))
    
    return line1, line2

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
