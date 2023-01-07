# espwlmon

Tool for reading and monitoring the status of [Wear Leveling](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/wear-levelling.html) layer in flash memory of ESP32 chips.

Interest is taken mainly in: (?)
- flash access count - incremented on each erase operation (+ on flush when unmounting WL)
- dummy sector move count - dummy sector is shifted after update rate number of flash accesses

From said statistics TODO can be drawn.

The tool is split to `data-collector` and (TODO) Python FE.

## WIP: Data collector (not yet implemented as described)
Consists of an embedded application utilizing [booting from test firmware](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/bootloader.html#bootloader-boot-from-test-firmware) and a C library providing access to and disection of WL information from given wear levelled storage partition.
The library is also used for offline partition dump analysis in Python.

The app sends JSON formatted statistics and configurations to PC side for further processing and visualization.

## draft: library
### first route
implement all WL computations of sizes, counts, crcs that are needed for checking partition has WL and processing the info (e.g. traversing the update states as in `recoverPos()`). this approach effectively duplicates the implementation in `WL_Flash.cpp` and related.
on checking partition has WL, can Wl_mount to potentially repair state and ensure everything is ok.
then reconstruct and read what is necessary, again implementing own functions.

### second route
make a menuconfig option to make `s_instances[]` in `wear_levelling.cpp` not static => gain access to `WL_flash` object containing all necessary up-to-date info, including `recoverPos()` result.
only checking that partition has WL (otherwise mounting will initialize it if it's not present) has to be implemented, minimizing duplicite code.

### library usage in offline dump analysis in Python - PC side
`WL_Flash::config()` takes `Flash_Access *flash_drv` that implements `read/write/erase` functions. we can just implement our own functions that won't access flash like `Partition.cpp` does, but will simply read and write to memory where the partition dump is, erase will be NOP.

the idea is roughly like this:
1. python gets the partition dump file as arg, read it in binary as `bytes` or `bytearray`. 
2. TODO: allocate mem in C and copy the dump to it. we are on PC, so we can. this interfacing needs to be tested, how does allocation behave? does the Python process "keep it" after calling C func that mallocs so another C call can utilize it?
3. setup own implementation of `read/write/erase` functions
4. create `esp_partition_t` struct with `address` being the address of allocated mem with the copied dump
5. `wl_mount()` normally, should everything be mocked correctly, it will be transparent that it's not mounting from flash

TODO: what and how needs to be compiled? is the previously described even feasible?

