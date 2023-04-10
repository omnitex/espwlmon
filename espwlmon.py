#! /usr/bin/env python

__version__ = "0.3"

import argparse
import sys
import json
import serial
import math

import PySimpleGUI as sg
import matplotlib
import matplotlib.pyplot as plt

import plotly.express as px
import plotly.tools as tls
import plotly.io as pio

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import numpy as np

def monitor(port):
    print(f"Starting monitor on port {port}")

    try:
        serial_port = serial.Serial(port, baudrate=115200, timeout=20)
    except serial.SerialException as serial_exception:
        print(serial_exception)

    json_dict = None

    print("Will receive JSON, this can take few seconds...")

    while(serial_port.is_open):
        json_line = serial_port.readline().decode()
        try:
            if json_dict is None:
                # first JSON load
                json_dict = json.loads(json_line)
            else:
                # JSON already loaded, check next load produces same JSON
                if (json_dict != json.loads(json_line)):
                    # they differ, save the newly loaded
                    json_dict = json.loads(json_line)
                else:
                    # they are the same, break out of while
                    break

        except json.JSONDecodeError as json_decode_error:
            print(f"Error parsing received JSON: {json_decode_error.msg}")
            context_characters = 15
            # print the context of error with characters around and arrow on next line pointing to the exact position
            print(f"{json_line[json_decode_error.pos-context_characters:json_decode_error.pos+context_characters]}")
            print(' ' * context_characters, '^', ' ' * context_characters, sep='')
            serial_port.close()
            return

    print("Received JSON confirmed, closing serial port")
    serial_port.close()

    gui(json_dict)

# source https://matplotlib.org/stable/gallery/images_contours_and_fields/image_annotated_heatmap.html
def create_heatmap(data, row_labels, col_labels, ax=None,
            cbar_kw=None, cbarlabel="", **kwargs):
    """
    Create a heatmap from a numpy array and two lists of labels.

    Parameters
    ----------
    data
        A 2D numpy array of shape (M, N).
    row_labels
        A list or array of length M with the labels for the rows.
    col_labels
        A list or array of length N with the labels for the columns.
    ax
        A `matplotlib.axes.Axes` instance to which the heatmap is plotted.  If
        not provided, use current axes or create a new one.  Optional.
    cbar_kw
        A dictionary with arguments to `matplotlib.Figure.colorbar`.  Optional.
    cbarlabel
        The label for the colorbar.  Optional.
    **kwargs
        All other arguments are forwarded to `imshow`.
    """

    if ax is None:
        ax = plt.gca()

    if cbar_kw is None:
        cbar_kw = {}

    # Plot the heatmap
    im = ax.imshow(data, **kwargs)

    # Create colorbar
    cbar = ax.figure.colorbar(im, ax=ax, **cbar_kw)
    cbar.ax.set_ylabel(cbarlabel, rotation=-90, va="bottom")

    # Show all ticks and label them with the respective list entries.
    ax.set_xticks(np.arange(data.shape[1]), labels=col_labels)
    ax.set_yticks(np.arange(data.shape[0]), labels=row_labels)

    # Let the horizontal axes labeling appear on top.
    ax.tick_params(top=True, bottom=False,
                   labeltop=True, labelbottom=False)

    # Rotate the tick labels and set their alignment.
    plt.setp(ax.get_xticklabels(), rotation=-30, ha="right",
             rotation_mode="anchor")

    # Turn spines off and create white grid.
    ax.spines[:].set_visible(False)

    ax.set_xticks(np.arange(data.shape[1]+1)-.5, minor=True)
    ax.set_yticks(np.arange(data.shape[0]+1)-.5, minor=True)
    ax.grid(which="minor", color="w", linestyle='-', linewidth=0)
    ax.tick_params(which="minor", bottom=False, left=False)

    return im, cbar


def annotate_heatmap(im, data=None, valfmt="{x:.2f}",
                     textcolors=("black", "white"),
                     threshold=None, **textkw):
    """
    A function to annotate a heatmap.

    Parameters
    ----------
    im
        The AxesImage to be labeled.
    data
        Data used to annotate.  If None, the image's data is used.  Optional.
    valfmt
        The format of the annotations inside the heatmap.  This should either
        use the string format method, e.g. "$ {x:.2f}", or be a
        `matplotlib.ticker.Formatter`.  Optional.
    textcolors
        A pair of colors.  The first is used for values below a threshold,
        the second for those above.  Optional.
    threshold
        Value in data units according to which the colors from textcolors are
        applied.  If None (the default) uses the middle of the colormap as
        separation.  Optional.
    **kwargs
        All other arguments are forwarded to each call to `text` used to create
        the text labels.
    """

    if not isinstance(data, (list, np.ndarray)):
        data = im.get_array()

    # Normalize the threshold to the images color range.
    if threshold is not None:
        threshold = im.norm(threshold)
    else:
        threshold = im.norm(data.max())/2.

    # Set default alignment to center, but allow it to be
    # overwritten by textkw.
    kw = dict(horizontalalignment="center",
              verticalalignment="center")
    kw.update(textkw)

    # Get the formatter in case a string is supplied
    if isinstance(valfmt, str):
        valfmt = matplotlib.ticker.StrMethodFormatter(valfmt)

    # Loop over the data and create a `Text` for each "pixel".
    # Change the text's color depending on the data.
    texts = []
    for i in range(data.shape[0]):
        for j in range(data.shape[1]):
            kw.update(color=textcolors[int(im.norm(data[i, j]) > threshold)])
            text = im.axes.text(j, i, valfmt(data[i, j], None), **kw)
            texts.append(text)

    return texts

def draw_figure(canvas, figure):
    figure_canvas_agg = FigureCanvasTkAgg(figure, canvas)
    figure_canvas_agg.draw()
    figure_canvas_agg.get_tk_widget().pack(side='top', fill='both', expand=True, anchor='center')
    return figure_canvas_agg

def format_thousands(num, _):
    if num >= 1000:
        return '{:.0f}K'.format(num / 1000)
    else:
        return str(num)

#TODO launch GUI before getting JSON with a loading screen
def gui(json_dict):
    sg.theme('Gray Gray Gray')

    if 'erase_counts' in json_dict:
        erase_counts = json_dict.pop('erase_counts')
        print(f'erase_counts: {erase_counts}')

    wl_mode = json_dict.pop('wl_mode')
    config = json_dict.pop('config')
    state = json_dict.pop('state')
    sector_count = int(state['max_pos'], base=16) - 1
    print(f'sector_count: {sector_count}')

    sector_count = 257;

    # get heatmap side lengths for given sector_count
    X = 1
    Y = sector_count
    while X < Y:
        X += 1
        Y = math.ceil(sector_count / X)

    print(f'X = {X}, Y = {Y}')

    # 2D heatmap to contain all sector counts, init all zeros
    heatmap = np.zeros((X, Y) ,dtype=int)

    # fill heatmap with values from erase_counts JSON
    # index with sector num but in 2D
    for sector_num_str, erase_count_str in erase_counts.items():
        sector_num = int(sector_num_str)
        erase_count = int(erase_count_str)
        # every erase count means 16 erases, as that is the threshold for triggering
        # writing a record to flash
        heatmap[sector_num % X][sector_num // X] = 16 * erase_count

    left = [[sg.T(f'wl_mode: {wl_mode}')]]
    for key in config:
        left += [[sg.T(f'{key}: {config[key]}')]]
    for key in state:
        left += [[sg.T(f'{key}: {state[key]}')]]

    graph = [[sg.Canvas(key='-CANVAS-')]]

    layout = [[sg.Column(left), sg.Column(graph)]]

    window = sg.Window(f'espwlmon v{__version__}', layout, finalize=True, margins=(10,10))

    # plot the heatmap
    fig, ax = plt.subplots()
    im, _ = create_heatmap(heatmap, np.arange(X), np.arange(Y), ax=ax, cmap='viridis', cbarlabel='erase count')

    annotate_heatmap(im, valfmt=format_thousands, size=8, textcolors=('white', 'black'))
    fig.tight_layout()

    draw_figure(window['-CANVAS-'].TKCanvas, fig)

    # TODO plotly export as html, button?
    # df = px.data.tips()

    # fig = px.density_heatmap(df, x="total_bill", y="tip", text_auto=True)
    # fig.write_html('heatmap.html')

    while True:
        event, values = window.read()
        if event in (sg.WIN_CLOSED, 'Exit'):
            break
    window.close()

def main():
    """
    Main function for espwlmon
    """
    parser = argparse.ArgumentParser(
        description=f"espwlmon.py v{__version__} - Flash Wear Leveling Monitoring Utility for devices with Espressif chips",
        prog="espwlmon",
    )
    parser.add_argument(
        "--port",
        "-p",
        help="Serial port device",
        required=True
    )

    argv = sys.argv[1:]

    # just for checking args format, we do not need them parsed
    args = parser.parse_args(argv)
    print(f"espwlmon.py v{__version__}")

    monitor(args.port)

if __name__ == "__main__":
    main()
