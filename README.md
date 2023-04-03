# espwlmon

Tool for reading and monitoring the status of [Wear Leveling](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/wear-levelling.html) layer in flash memory of ESP32 chips.

Interest is taken mainly in per sector erase count or its estimate. As program-erase (PE) memory cell lifetime is finite, knowledge of wear induced by an application is important for assessing expected memory lifetime and reliability.

This tool is aimed at long term applications utilizing the wear-leveling layer.

The tool is split to `wlmon` embedded C++ and (TODO) Python FE.

## `wlmon`
Embedded part of the project, built and flashed as an `idf.py` application. Reads and reconstructs info about WL layer from structures and records kept in flash.
As the ESP-IDF version of wear-levelling does not make it possible to monitor erase operations long term, an improved version `WL_Advanced` is practically necessary to use.

## `WL_Advanced`
Extends base wear-leveling algorithm by tracking per sector erase counts and uses Feistel network sector address randomization for improving evenness of memory wear.

## Installation and usage

Firstly make sure there is not an existing instance of WL in your data partition, this may require erasing given portion of flash.
It is because there is no "update from base WL" functionality (should there be?)

### 1. Get and enable `WL_Advanced`

From `espwlmon/data-collector/wear_levelling` copy the following files to `esp-idf/components/wear_levelling` of your active installation of ESP-IDF

Implementation of extended version of WL:
- `WL_Advanced.cpp`
- `private_include/WL_Advanced.h`

Intergrating `WL_Advanced`:
- `Kconfig`  (adds a `menuconfig` option)
- `wear_levelling.cpp` (adds instantiating `WL_Advanced` if selected)

Now for your project of choice, run `idf.py menuconfig` in the project folder and navigate to `Component config/Wear Levelling` and enable the advanced wear leveling mode. Or jump to symbol `WL_ADVANCED_MODE` and enable.

### 2. Build, flash and run your project
While monitoring, you should see logs at various levels from `WL_Advanced`. Running your project which uses WL will create erase records in flash (only every 16th erase or any flush will trigger a new record).

Amassing the records in practical scenario means just letting it run. For testing, it might be beneficial to do repeated erase stressing. 

### 3. Read status with `wlmon`

Once sufficient time and/or operations have passed, we will flash the `wlmon` app to read out a status report.

Navigate to `espwlmon/data-collector/wear_levelling/wlmon` and run `idf.py app-flash monitor`

This is so far the end of the journey; JSON with status is printed repeatedly, ready for `espwlmon` (the Python will-be front-end).

