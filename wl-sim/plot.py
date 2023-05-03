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

def plot_cycle_walks(cycle_walks, name):
    block_sizes = ("1 sector", "5 sectors", "10 sectors", "20 sectors", "30 sectors", "40 sectors", "50 sectors")
    values = {
        "cycle_walks" : cycle_walks
    }

    x = np.arange(len(block_sizes))  # the label locations
    width = 0.35  # the width of the bars
    multiplier = 0

    fig, ax = plt.subplots(layout='constrained')

    for attribute, measurement in values.items():
        offset = width * multiplier
        rects = ax.bar(x, measurement, width, edgecolor="black" , label=attribute, hatch="//", color='C1')
        #ax.bar_label(rects, padding=3)
        multiplier += 1

    # Add some text for labels, title and custom x-axis tick labels, etc.
    ax.set_ylabel('Cycle Walks Out Of Total Erases (%)')
    #ax.set_title('Penguin attributes by species')
    ax.set_xticks(x, block_sizes)
    ax.set_yticks(np.linspace(0, 1.2, num=13))
    ax.legend(loc='upper left', ncols=2)
    ax.set_ylim(0.0, 1.2)
    ax.set_axisbelow(True)
    ax.yaxis.grid(color='gray')
    #ax.set_aspect('equal')

    #plt.show()
    plt.savefig(f"{OUTPUT_DIR}/{name}.pdf")


# results copied by hand
# base vs advanced for constant address, constant block sizes and no restarting
b_c_c_n_0 = (96.024, 96.0879, 96.1679, 96.3279, 96.4879, 96.6478, 96.8078)
f_c_c_n_0 = (96.0249, 98.0844, 98.8179, 99.5179, 99.6849, 99.7731, 99.7871)
plot(b_c_c_n_0, f_c_c_n_0, "NE_c_c_n_0")

# base vs advanced for zipf address, constant block sizes, no restarting
b_z_c_n_0 = (97.2214, 97.5189, 97.7243, 97.9864, 98.158, 98.4014, 98.59)
f_z_c_n_0 = (98.6313, 98.9037, 99.1171, 99.2359, 99.3122, 99.3343, 99.359)
plot(b_z_c_n_0, f_z_c_n_0, "NE_z_c_n_0")

# cycle walks in percent out of total number of erases performed in full simulation of memory lifetime
# obtained from advanced (feistel) with zipf address, const block sized, no restarting
CW_f_z_c_n_0 = (0.516029, 0.552327, 0.585436, 0.606155, 0.718522, 0.832719, 1.18257)
plot_cycle_walks(CW_f_z_c_n_0, "CW_f_z_c_n_0")