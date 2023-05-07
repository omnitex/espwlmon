# espwlmon

Tool for reading the status of wear leveling layer in flash memories in ESP32-xx SoCs, modules and boards.

Project includes an extended version of [base wear leveling](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/wear-levelling.html) which makes possible long term tracking of per sector erase counts and improves leveling evenness by address mapping randomization using format preserving encryption based on unbalanced 3-stage Feistel network.

## Installation and usage

### 0. Prerequisites and cloning

- Set up `ESP-IDF` on your system. Currently the **only tested version is `v5.0`**.  For instructions see Espressif's [get started guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
- Get yourself a `Python3` installation with `tkinter` for running the GUI part of this project. This will heavily depend on your system and current installations/configurations, so you're on your own for this one

Once ready to proceed, clone this repo

```
git clone https://github.com/omnitex/espwlmon.git && cd espwlmon
```

### 1. Get `WL_Advanced`

Copy the contents of `data-collector/wear_levelling` directory to `$IDF_PATH/components/wear_levelling` directory in your active ESP-IDF installation.

```
cp -r data-collector/wear_levelling/ $IDF_PATH/components/
```

### 2. Erase current flash contents
*`idf.py` requires to be run from ESP-IDF buildable project folder; for such the `erase_stress_example` from the next step can be used*
```
idf.py erase-flash
```

This will allow `WL_Advanced` to initialize all structures in flash from a clean state.

### 2a. Test with `erase_stress_example`

*Warning: Running this will repeatedly perform erases in flash memory of you device, (really) slowly wearing it out.*

```
cd erase_stress_example && idf.py flash monitor
```

The provided example is a simple project for testing FAT read/write + memory erase stressing which builds up `WL_Advanced` records about sector erases in flash.

Letting it run fully will take some time. Feel free to stop it after at least a run or two (see the logs while it runs) and move onto step 3. But keep in mind the longer it runs the better - at least for visualization in step 4.

### 2b. Run your own project

Instead of running the stress example, you can supply your own project for this step. Only requirement is that it uses the wear leveling layer and **has enabled the advanced version of WL in menuconfig**.

```
cd <FOLDER OF YOUR PROJECT>
idf.py menuconfig
Navigate to Component config/Wear Leveling => enable advanced monitoring mode
(or jump to symbol WL_ADVANCED_MODE using /)
```

### 3. Flash embedded data collector

After running some code on top of `WL_Advanced`, it's time to read out the WL status.

```
cd $IDF_PATH/components/wear_levelling/wlmon && idf.py app-flash
```

This will flash the embedded part of the monitoring tool to your device. Making it ready for the final step.

### 4. Run PC side Python GUI

Navigate back to the root of this repo, wherever you've cloned it and run the following.

```
python3 -m pip install -r requirements.txt
python3 espwlmon.py --port PORT
```

After installing required Python packages, run `espwlmon.py` while specifying the serial port your device is connected to.

And that's it; you should be greeted with a listing of internal structures used by WL and an erase count heatmap, as reconstructed from records in flash.

