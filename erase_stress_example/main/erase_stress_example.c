/*
 * Based on [wear levelling example](https://github.com/espressif/esp-idf/tree/master/examples/storage/wear_levelling)
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_random.h"

#include "esp_partition.h"
#include "esp_flash.h"

#include "esp_log.h"

#define WL_RESULT_CHECK(result) \
    if (result != ESP_OK) { \
        ESP_LOGE(TAG,"%s(%d): result = 0x%08x", __FUNCTION__, __LINE__, result); \
        return (result); \
    }

static const char *TAG = "erase_stress_example";

// Mount path for the partition
static const char *base_path = "/spiflash";

static const char *example_text = "Written in erase_stress_example testing WL_Advanced randomized mapping and erase count record keeping behavior.";

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

// number of erases performed in one stress run
static const unsigned int erase_iterations = 500;

// number of full runs of FATFS test + stressing to run
static const unsigned int full_runs = 20;

static esp_err_t run_erase_stressing(const esp_partition_t *storage)
{
    wl_handle_t wl_handle = WL_INVALID_HANDLE;

    // mount wear leveled partition directly
    WL_RESULT_CHECK(wl_mount(storage, &wl_handle));

    for (int i = 0; i < erase_iterations; i++) {
        // report progress and randomize the flow 10 times per loop
        if ((i % (erase_iterations / 10)) == 0) {
            ESP_LOGI(TAG, "%s: erase stressing iteration %u/%u", __func__, i, erase_iterations);

            // generate number 0-99 to use as random chance in % below
            uint32_t rand = esp_random() % 100;

            // 5% chance to break loop early
            if (rand < 5) {
                ESP_LOGI(TAG, "%s: random cycle break at iteration %u", __func__, i);
                break;
            }

            // 15% chance to cycle unmount-mount to introduce some variance
            if (rand < 15) {
                ESP_LOGI(TAG, "%s: will cycle unmount-mount at iteration %u", __func__, i);

                // use rand also for loop bound, it is <15
                for (int j = 0; j < rand; j++) {
                    // unmounting always moves dummy sector -> this loop will achieve move_count mapping shift artificially sooner
                    WL_RESULT_CHECK(wl_unmount(wl_handle));
                    WL_RESULT_CHECK(wl_mount(storage, &wl_handle));
                }
            }
        }

        // erase a sector in the middle of wear leveled partition
        WL_RESULT_CHECK(wl_erase_range(wl_handle, wl_size(wl_handle) / 2, CONFIG_WL_SECTOR_SIZE));
    }

    // unmount at the end
    WL_RESULT_CHECK(wl_unmount(wl_handle));

    return ESP_OK;
}

void app_main(void)
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };

    // get wear leveled data partition, will be used in stressing to access WL directly (not via file system as below in Part I)
    const esp_partition_t *storage = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");
    if (!storage) {
        ESP_LOGE(TAG, "Failed to find first data partition labeled 'storage'");
        return;
    }

    esp_err_t result;
    int finished_runs = 0;
    FILE *f;

    do {
        /*
        * Part I: mount FATFS, read file, write file
        * As ESP-IDF splits the flash 'storage stack' to wear leveling and a file system on top
        * and as FATFS is the main FS integrated to be used with WL, this part
        * tests that randomized mapping behavior of WL_Advanced behaves correctly
        * and does not break file access for FATFS.
        */

        result = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount fatfs (%s)", esp_err_to_name(result));
            return;
        }
        ESP_LOGI(TAG, "Mounted FAT filesystem");

        // read from file only on consecutive runs where the file is already created and written to
        // (covers the very first run of this example)
        if (finished_runs != 0) {
            // Open file for reading
            f = fopen("/spiflash/hello.txt", "rb");
            if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open file for reading");
                return;
            }
            char line[128];
            fgets(line, sizeof(line), f);
            fclose(f);
            // strip newline
            char *pos = strchr(line, '\n');
            if (pos) {
                *pos = '\0';
            }
            fclose(f);

            // if text read from file does not match what was written,
            // report error but continue
            if (strcmp(line, example_text) != 0) {
                ESP_LOGE(TAG, "Read incorrect text from file: '%s'", line);
            } else {
                ESP_LOGI(TAG, " Read from file: '%s'", line);
            }
        }

        // open file for writing, creating if it does not exist
        f = fopen("/spiflash/hello.txt", "wb");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return;
        }
        // and write example text to it
        fprintf(f, example_text);
        fclose(f);
        ESP_LOGI(TAG, "Written to file: '%s'", example_text);

        // Unmount FATFS
        ESP_LOGI(TAG, "Unmounting FAT filesystem");
        ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount_rw_wl(base_path, s_wl_handle));

        /*
        * Part II: erase stressing
        * Repeatedly erase few sectors with some randomly introduced mounting and unmounting of the partition
        * to shake up the spread of erases
        */
        result = run_erase_stressing(storage);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Erase stressing in run %u/%u failed: %s", finished_runs, full_runs, esp_err_to_name(result));
            return;
        }

        finished_runs++;
        ESP_LOGW(TAG, "Run %u/%u finished", finished_runs, full_runs);

    } while (finished_runs < full_runs);

    ESP_LOGI(TAG, "===== ERASE STRESS EXAMPLE END =====");
}
