import os
from cffi import FFI
ffibuilder = FFI()

ffibuilder.cdef("void print_partition_info();");

ffibuilder.set_source("_wl_analysis",
"""
    #include "wl_analysis.h"
""",
    sources=['wl_analysis.c'],
    include_dirs=[
        os.environ['IDF_PATH'] + '/components/spi_flash/include',
        os.environ['IDF_PATH'] + '/components/esp_common/include',
        os.environ['IDF_PATH'] + '/components/hal/include',
        '.'
    ])

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
