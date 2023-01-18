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

### Current `data-collector` status
The aim is to create own version of `WL_Flash` albeit only for reading the contents of a given partition and extracting wear leveling info which will be available in an instance.
