#! /usr/bin/env python3

import os
import matplotlib.pyplot as plt
import numpy as np

OUTPUT_DIR = "graphs"

if not os.path.exists(OUTPUT_DIR):
    os.mkdir(OUTPUT_DIR)

# based on https://matplotlib.org/stable/gallery/lines_bars_and_markers/barchart.html
def plot(base, advanced, name):

    block_sizes = ("1 sector", "5 sectors", "10 sectors", "20 sectors", "30 sectors", "40 sectors", "50 sectors")
    values = {
        "base": base,
        "advanced": advanced
    }

    x = np.arange(len(block_sizes))  # the label locations
    width = 0.35  # the width of the bars
    multiplier = 0

    fig, ax = plt.subplots(layout='constrained')

    for attribute, measurement in values.items():
        offset = width * multiplier
        rects = ax.bar(x + offset, measurement, width, edgecolor="black" , label=attribute, hatch="//" if attribute == "advanced" else "")
        #ax.bar_label(rects, padding=3)
        multiplier += 1

    # Add some text for labels, title and custom x-axis tick labels, etc.
    ax.set_ylabel('Normalized Endurance (%)')
    #ax.set_title('Penguin attributes by species')
    ax.set_xticks(x + width, block_sizes)
    ax.set_yticks(range(95, 101))
    ax.legend(loc='upper left', ncols=2)
    ax.set_ylim(95, 100)
    ax.set_axisbelow(True)
    ax.yaxis.grid(color='gray')
    ax.set_aspect('equal')

    #plt.show()
    plt.savefig(f"{OUTPUT_DIR}/{name}.pdf")


# results copied by hand
# base vs advanced for constant address, constant block sizes and no restarting
b_c_c_n_0 = (96.024, 96.0879, 96.1679, 96.3279, 96.4879, 96.6478, 96.8078)
f_c_c_n_0 = (96.0244, 98.032, 98.7949, 99.5187, 99.6869, 99.7703, 99.7909)
plot(b_c_c_n_0, f_c_c_n_0, "NE_c_c_n_0")

# base vs advanced for zipf address, constant block sizes, no restarting
b_z_c_n_0 = (97.2029, 97.5096, 97.7502, 98.0624, 98.1132, 98.3063, 98.5889)
f_z_c_n_0 = (98.6283, 98.9036, 99.0796, 99.2648, 99.3052, 99.3258, 99.3745)
plot(b_z_c_n_0, f_z_c_n_0, "NE_z_c_n_0")