from cffi import FFI
ffibuilder = FFI()

ffibuilder.cdef("float pi_approx(int n);");

ffibuilder.set_source("_pi",
"""
    #include "pi.h"
""",
    sources=['pi.c'],
    libraries=[])

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
