# espwlmon

Tool for reading the status of wear leveling layer in flash memories in ESP32-xx SoCs, modules and boards.

Project includes an extended version of [base wear leveling](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/wear-levelling.html) which makes possible long term tracking of per sector erase counts and improves leveling evenness by address mapping randomization using format preserving encryption based on unbalanced 3-stage Feistel network.

## Installation and usage

### 0. Clone repo

```
$ git clone https://github.com/omnitex/espwlmon.git && cd espwlmon
```

### 1. Get `WL_Advanced`

Copy the contents of `data-collector/wear_levelling` directory to `$IDF_PATH/components/wear_levelling` directory in your active ESP-IDF installation.

```
$ cp -r data-collector/wear_levelling/ $IDF_PATH/components/
```

<!-- TODO base->advanced will have to investigate -->
### 2. Erase current flash contents

```
$ idf.py erase-flash
```

This will allow `WL_Advanced` to initialize all structures in flash from a clean state.

### 2a. Test with `erase_stress_example`

```
$ cd erase_stress_example && idf.py flash monitor
```

The provided example is a simple project for testing FAT read/write + memory erase stressing which builds up `WL_Advanced` records about sector erases in flash.

Letting it run fully will take some time. Feel free to stop it after at least a run or two (see the logs while it runs) and move onto step 3.

### 2b. Run your own project

Instead of running the stress example, you can supply your own project for this step. Only requirement is that it uses the wear leveling layer and **has enabled the advanced version of WL in menuconfig**.

```
$ cd <FOLDER OF YOUR PROJECT>
$ idf.py menuconfig
Navigate to Component config/Wear Leveling => enable advanced monitoring mode
(or jump to symbol WL_ADVANCED_MODE using /)
```

### 3. Flash embedded data collector

After running some code on top of `WL_Advanced`, it's time to read out the WL status.

```
$ cd $IDF_PATH/components/wear_levelling/wlmon && idf.py app-flash
```

This will flash the embedded part of the monitoring tool to your device. Making it ready for the final step.

### 4. Run PC side Python GUI

Navigate back to the root of this repo, wherever you've cloned it.

<!-- TODO pip vs pip3 -->
```
$ pip3 install -r requirements.txt
$ python3 espwlmon.py --port PORT
```

After installing required Python packages, run `espwlmon.py` while specifying the serial port your device is connected to.

And that's it; you should be greeted with a listing of internal structures used by WL and an erase count heatmap, as reconstructed from records in flash.

