#! /usr/bin/env python

__version__ = "0.3"

import argparse
import sys
import json
import serial

import PySimpleGUI as sg
#import matplotlib
import matplotlib.pyplot as plt

import plotly.express as px
import plotly.tools as tls

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

def draw_figure(canvas, figure):
    figure_canvas_agg = FigureCanvasTkAgg(figure, canvas)
    figure_canvas_agg.draw()
    figure_canvas_agg.get_tk_widget().pack(side="top", fill="both", expand=1)
    return figure_canvas_agg

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

    left = [[sg.T(f'wl_mode: {wl_mode}')]]
    for key in config:
        left += [[sg.T(f'{key}: {config[key]}')]]
    for key in state:
        left += [[sg.T(f'{key}: {state[key]}')]]

    graph = [[sg.Canvas(key='-CANVAS-')]]

    layout = [[sg.Column(left), sg.Column(graph)]]

    window = sg.Window(f'espwlmon v{__version__}', layout, finalize=True, margins=(10,10))

    #TODO placeholder heatmap
    fig = plt.figure()
    a = np.random.random((16, 16))
    plt.imshow(a, cmap='hot', interpolation='nearest')

    draw_figure(window['-CANVAS-'].TKCanvas, fig)

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
