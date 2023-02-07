#! /usr/bin/env python

__version__ = "0.1"

import argparse
import os
import sys
import json
import serial

from math import ceil

import esptool
from esptool.util import (
    FatalError
)

# Source: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html
#TODO make this into some function?
idf_path = os.environ["IDF_PATH"]
parttool_dir = os.path.join(idf_path, "components", "partition_table")
sys.path.append(parttool_dir)
try:
    import gen_esp32part # type: ignore
    from gen_esp32part import ( # type: ignore
        InputError
    )
except ModuleNotFoundError as e:
    print("Cannot find gen_esp32part module", file=sys.stderr)
    sys.exit(2)
idftool_dir = os.path.join(idf_path, "tools")
sys.path.append(idftool_dir)
try:
    import idf # type: ignore
except ModuleNotFoundError as e:
    print(f"{e}", file=sys.stderr)
    sys.exit(2)

# TODO randomize filenames?
PARTITION_TABLE_OLD_BIN = "partition_table_old.bin"
PARTITION_TABLE_NEW_BIN = "partition_table_new.bin"
# TODO part table does not have to be at 0x8000
PARTITION_TABLE_ADDRESS = "0x8000"
PARTITION_TABLE_SIZE = "0xC00"

DATA_COLLECTOR_BIN = "data-collector/build/data-collector.bin"

def monitor(port):
    print(f"Starting monitor on port {port}")

    try:
        serial_port = serial.Serial(port, baudrate=115200, timeout=30)
    except serial.SerialException as serial_exception:
        print(serial_exception)

    json_dict = None

    print("Will receive JSON, this can take few seconds...")
    while(serial_port.is_open):
        if json_dict is None:
            # first JSON load
            json_dict = json.loads(serial_port.readline().decode())
        else:
            # JSON already loaded, check next load produces same JSON
            if (json_dict != json.loads(serial_port.readline().decode())):
                # they differ, save the newly loaded
                json_dict = json.loads(serial_port.readline().decode())
            else:
                # they are the same, break out of while
                break

    print("Received JSON confirmed, closing serial port")
    serial_port.close()

    print(json.dumps(json_dict, indent=4))


def flash():
    if not os.path.isfile(DATA_COLLECTOR_BIN):
        print("Please build data collector first: espwlmon.py build --chip <target-chip>")
        sys.exit(1)

    # skip progname and operation
    argv = sys.argv[2:]

    print("Reading existing partition table")
    args_read_partition_table = argv + ["read_flash", PARTITION_TABLE_ADDRESS, PARTITION_TABLE_SIZE, PARTITION_TABLE_OLD_BIN]
    try:
        esptool.main(args_read_partition_table)
    except FatalError as e:
        print(f"{e}")
        cleanup(2)

    with open(PARTITION_TABLE_OLD_BIN, "rb") as f:
        table, _ = gen_esp32part.PartitionTable.from_file(f)
    
    if table.find_by_name("test") is not None:
        print("Test app partition already exists, aborting...")
        cleanup(2)

    # we will add test partition right at the end of existing partitions
    test_partition_offset = table.flash_size()

    # determine test partition size, make it a multiple of 4K blocks
    data_collector_size = os.stat(DATA_COLLECTOR_BIN).st_size
    number_of_64k_blocks = ceil(data_collector_size/0x1000)
    test_partition_len = number_of_64k_blocks * 0x1000

    # construct a new CSV line that will be appended to existing partition table
    test_partition_csv = f"test,app,test,{test_partition_offset},{test_partition_len}"

    # append new line specifying test partition
    table.append(gen_esp32part.PartitionDefinition.from_csv(test_partition_csv, len(table)))

    # TODO check table.flash_size() after adding test is still <= overall flash size (get that from somewhere)

    print("Verifying altered partition table before writing it")
    try:
        table.verify()
    except InputError as e:
        print("Failed adding test app partition!")
        cleanup(2)

    with open(PARTITION_TABLE_NEW_BIN, "wb") as f:
        f.write(table.to_binary())
    
    print("Flashing new partition table")
    args_write_partition_table = argv + ["write_flash", PARTITION_TABLE_ADDRESS, PARTITION_TABLE_NEW_BIN]
    try:
        esptool.main(args_write_partition_table)
    except FatalError as e:
        print(f"{e}")
        cleanup(2)

    print("Flashing data collector to test partition")
    args_flash_data_collector = argv + ["write_flash", f"{test_partition_offset}", DATA_COLLECTOR_BIN]
    try:
        esptool.main(args_flash_data_collector)
    except FatalError as e:
        print(f"{e}")
        cleanup(2)

    print("\nSuccessfully flashed data collector")
    cleanup(0)

def main():
    """
    Main function for espwlmon
    """
    parser = argparse.ArgumentParser(
        description="espwlmon.py v%s - Flash Wear Leveling Monitoring Utility for devices with Espressif chips"
        % __version__,
        prog="espwlmon",
    )

    subparsers = parser.add_subparsers(
        dest="operation", help="Run espwlmon.py {command} -h for additional help"
    )

    parser_flash = subparsers.add_parser(
        "flash", help="Create test partition and flash built data collector to it"
    )
    parser_flash.add_argument(
        "--port",
        "-p",
        help="Serial port device",
        required=True
    )

    parser_monitor = subparsers.add_parser(
        "monitor", help="Start monitoring data collector output"
    )
    parser_monitor.add_argument(
        "--port",
        "-p",
        help="Serial port device",
        required=True
    )

    argv = sys.argv[1:]

    # just for checking args format, we do not need them parsed
    args = parser.parse_args(argv)
    print("espwlmon.py v%s" % __version__)

    if args.operation is None:
        parser.print_help()
        sys.exit(1)

    if args.operation == "flash":
        flash()
    elif args.operation == "monitor":
        monitor(args.port)

def cleanup(exit_code):
    if os.path.isfile(PARTITION_TABLE_OLD_BIN):
        os.remove(PARTITION_TABLE_OLD_BIN)
    if os.path.isfile(PARTITION_TABLE_NEW_BIN):
        os.remove(PARTITION_TABLE_NEW_BIN)

    sys.exit(exit_code)

def _main():
    try:
        main()
    except FatalError as e:
        print("\nA fatal error occurred: %s" % e)
        sys.exit(2)
    finally:
        cleanup(0)

if __name__ == "__main__":
    _main()
