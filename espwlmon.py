#! /usr/bin/env python

__version__ = "0.3"

import argparse
import sys
import json
import math
import serial

import PySimpleGUI as sg
import matplotlib
import matplotlib.pyplot as plt

import plotly.express as px

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
            #TODO this does not seem to work?
            print(f"{json_line[json_decode_error.pos-context_characters:json_decode_error.pos+context_characters]}")
            print(' ' * context_characters, '^', ' ' * context_characters, sep='')
            serial_port.close()
            return

    print("Received JSON confirmed, closing serial port")
    serial_port.close()

    gui(json_dict)

# Helper function for creating a Matplotlib heatmap
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

# Helper function for annotating a Matplotlib heatmap
# source https://matplotlib.org/stable/gallery/images_contours_and_fields/image_annotated_heatmap.html
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

def popup_toast(window, message, duration=2000):
    toast_layout = [[sg.Text(message, pad=(20,10))]]
    toast_window = sg.Window('', toast_layout, keep_on_top=True, no_titlebar=True, alpha_channel=.8, finalize=True)
    # one-shot centering of toast to the center of parent window, does not track movement after that
    toast_window.move(window.current_location()[0] + window.size[0] // 2 - toast_window.size[0] // 2, window.current_location()[1] + window.size[1] // 2 - toast_window.size[1] // 2)
    toast_window.TKroot.after(duration, toast_window.close)

def make_text_vertical(text):
    vertical = ''
    for l in text:
        vertical += f'{l}\n'
    return vertical

#TODO global list, better solution?
#TODO feitel keys empty space in input, why?
selectable_texts_keys = list()
def selectable_text(text):
    # create unique key for the text from current length of list and the text itself, ensuring identical texts will have different keys
    selectable_texts_keys.append(f'{len(selectable_texts_keys)}:{text}')
    # return disabled input element with given text, appropriate size and indexable by created key
    return [[sg.InputText(text, size=(len(text), 1), use_readonly_for_disable=True, disabled=True, key=selectable_texts_keys[-1])]]

def create_mode_config_state_layout(wl_mode, config, state):
    layout = [[]]
    layout += selectable_text(f'wl_mode: {wl_mode}')
    layout += [[sg.HorizontalSeparator()]]

    config_header = [[sg.T(make_text_vertical('CONFIG'))]]

    config_content = [[]]
    for key in config:
        config_content += selectable_text(f'{key}: {config[key]}')

    config_layout = [[sg.Column(config_header), sg.Column(config_content)]]

    layout += config_layout
    layout += [[sg.HorizontalSeparator()]]

    state_header = [[sg.T(make_text_vertical('STATE'))]]

    state_content = [[]]
    for key in state:
        state_content += selectable_text(f'{key}: {state[key]}')

    state_layout = [[sg.Column(state_header), sg.Column(state_content)]]

    layout += state_layout

    return layout

def calculate_heatmap_dimensions(sector_count):
    # calculate heatmap side lengths for given sector_count
    # e.g. 248 sector fit in a X=16, Y=16 square
    # this introduces invalid positions in the heatmap, from sector count up to X*Y
    X = 1
    Y = sector_count
    while X < Y:
        X += 1
        Y = math.ceil(sector_count / X)

    return X, Y

def create_erase_count_heatmap(sector_count):

    X, Y = calculate_heatmap_dimensions(sector_count)

    # 2D heatmap to contain all integer sector counts, init with zeros
    heatmap = np.zeros((X, Y), dtype=int)
    # write an invalid erase count of -1 to positions that are in the heatmap
    # yet are not valid sectors
    for i in range(sector_count, X*Y):
        heatmap[i // X][i % X] = -1

    return heatmap

TOGGLE_ERASE_COUNT_ANNOTATIONS = 'Toggle erase counts'
EXPORT_PLOTLY_HTML = 'Export Plotly'

def create_advanced_layout(json_dict):
    wl_mode = json_dict.pop('wl_mode')
    erase_counts = json_dict.pop('erase_counts')
    print(f'erase_counts: {erase_counts}')

    config = json_dict.pop('config')
    state = json_dict.pop('state')
    sector_count = int(state['max_pos'], base=16) - 1
    print(f'sector_count: {sector_count}')

    # create a layout for left column listing info from config and state structs
    left_layout = create_mode_config_state_layout(wl_mode, config, state)

    # layout for graph, will draw later
    graph_layout = [[sg.Canvas(key='-CANVAS-')]]

    # layout for buttons, use constants for names as they become action names also
    buttons_layout = [[sg.B(TOGGLE_ERASE_COUNT_ANNOTATIONS)],[sg.B(EXPORT_PLOTLY_HTML)]]

    # overall layout with three columns
    layout = [[sg.Column(left_layout), sg.Column(graph_layout), sg.Column(buttons_layout)]]

    heatmap, fig, ax = plot_erase_count_heatmap(erase_counts, sector_count)

    return layout, heatmap, fig, ax

def create_base_layout(json_dict):
    wl_mode = json_dict.pop('wl_mode')
    config = json_dict.pop('config')
    state = json_dict.pop('state')

    layout = create_mode_config_state_layout(wl_mode, config, state)

    return layout

def plot_erase_count_heatmap(erase_counts, sector_count):
    # create and initialize heatmap to fit given sector count
    heatmap = create_erase_count_heatmap(sector_count)
    X, Y = calculate_heatmap_dimensions(sector_count)

    # fill heatmap with values from erase_counts JSON
    # index with sector num but in 2D
    for sector_num_str, erase_count_str in erase_counts.items():
        sector_num = int(sector_num_str)
        erase_count = int(erase_count_str)
        # every erase count means 16 erases, as that is the threshold for triggering writing a record to flash
        heatmap[sector_num // X][sector_num % X] = 16 * erase_count
    # create matplotlib figure
    fig, ax = plt.subplots()
    # choose plasma color palette with extremes (set below using vmin)
    palette = plt.cm.plasma.with_extremes(under='black')

    # create lists of hex labels for ticks for both rows (Y) and cols (Y)
    row_labels = [hex(x) for x in range(X)]
    col_labels = [hex(y) for y in range(Y)]
    # plot the heatmap, set vmin, vmax limits for extremes that will be colored as set above in with_extremes()
    im, _ = create_heatmap(heatmap, row_labels, col_labels, ax=ax, cbarlabel='erase count', cmap=palette, vmin=0)

    # annotate individual positions with erase counts formatted to display thousands as multiple of K
    annotate_heatmap(im, valfmt=format_thousands, size=8, textcolors=('white', 'black'))
    # improves spacing of stuff in fig a bit
    fig.tight_layout()

    return heatmap, fig, ax

def create_error_layout(json_dict, message='WLmon reports'):
    layout = [[sg.T(f'{message}: {json_dict}\nRun idf.py monitor on wlmon (with verbose logging enabled) to learn more')]]
    return layout

#TODO launch GUI before getting JSON with a loading screen
def gui(json_dict):
    sg.theme('Gray Gray Gray')
    sg.set_options(font=('Roboto', 11))

    wl_mode = 'undefined'

    if 'wl_mode' in json_dict:
        wl_mode = json_dict['wl_mode']
        if wl_mode == 'advanced':
            layout, heatmap, fig, ax = create_advanced_layout(json_dict)
        else:
            layout = create_base_layout(json_dict)
    elif 'error' in json_dict:
        layout = create_error_layout(json_dict)
    else:
        layout = create_error_layout(json_dict, 'Unknown JSON received')

    # create window
    window = sg.Window(f'espwlmon v{__version__}', layout, finalize=True, margins=(10,10))

    if wl_mode == 'advanced':
        # draw the heatmap
        canvas = draw_figure(window['-CANVAS-'].TKCanvas, fig)

    # set zero border width for all inputs that are read only to mock selectable texts
    for key in selectable_texts_keys:
        window[key].Widget.config(borderwidth=0)

    # main event loop for the window
    while True:
        event, values = window.read()
        if event in (sg.WIN_CLOSED, 'Exit'):
            break
        if event == TOGGLE_ERASE_COUNT_ANNOTATIONS:
            for annotation in ax.texts:
                annotation.set_visible(not annotation.get_visible())
            canvas.draw()
        if event == EXPORT_PLOTLY_HTML:
            # disable button and change text to reflect heatmap is being generated
            window[EXPORT_PLOTLY_HTML].update(disabled=True, text='Generating heatmap...')
            window.refresh()

            px_heatmap = px.imshow(heatmap, text_auto=True)
            px_heatmap.update_layout(xaxis=dict(tickmode='linear'), yaxis=dict(tickmode='linear'))

            X, Y = heatmap.shape
            x_values = np.arange(X)
            y_values = np.arange(Y)
            hover_labels = [[f'sector_num={y*X + x}, erase_count={heatmap[y][x]}' for x in x_values] for y in y_values]
            px_heatmap.update_traces(hovertemplate='%{customdata}<extra></extra>', customdata=hover_labels, text=heatmap)

            # once heatmap is ready to be saved, revert button to original state
            window[EXPORT_PLOTLY_HTML].update(disabled=False, text=EXPORT_PLOTLY_HTML)
            window.refresh()

            filename = sg.popup_get_file('Save as', save_as=True, file_types=[('HTML Files', '*.html')], default_path='./heatmap.html')
            if filename is not None:
                px_heatmap.write_html(filename)
                popup_toast(window, 'Plotly heatmap exported successfully')

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
