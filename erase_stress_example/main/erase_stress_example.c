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

static const char *TAG = "erase_test_example";

// Mount path for the partition
const char *base_path = "/spiflash";

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

// whether to only FAT file test or full stress
#define FAT_ONLY false
// number of full runs of FAT test + erase stressing
#define FULL_RUNS 10

void app_main(void)
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };

    esp_err_t ret;
    int full_runs = 0;

start:
    /*
     * Part I: mount FAT, read file, write file
     * Tests correct mapping behavior not breaking file access
     */

    ret = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount fatfs (%s)", esp_err_to_name(ret));
        return;
    }

    FILE *f;

    // read from file only on consecutive runs where the file is already created and written to
    // (covers the very first run of this example)
    if (full_runs != 0) {
        // Open file for reading
        ESP_LOGI(TAG, "Reading file");
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
        ESP_LOGI(TAG, "Read from file: '%s'", line);
    }

    // write to file, creating if it does not exist
    ESP_LOGI(TAG, "Opening file");
    f = fopen("/spiflash/hello.txt", "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "written using ESP-IDF %s\n", esp_get_idf_version());
    fclose(f);
    ESP_LOGI(TAG, "File written");

    // Unmount FATFS
    ESP_LOGI(TAG, "Unmounting FAT filesystem");
    ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount_rw_wl(base_path, s_wl_handle));

#if !FAT_ONLY
    /*
     * Part II: erase stressing
     */

    // get wear leveled data partition
    const esp_partition_t *storage;
    storage = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");
    if (!storage) {
        ESP_LOGE(TAG, "Failed to find first data partition labeled 'storage'");
        return;
    }

    ESP_ERROR_CHECK(wl_mount(storage, &s_wl_handle));

    // perform iterations of erasing a single sector to artificially stress the memory
    #define ERASE_ITERATIONS (16*50)

    for (int i = 0; i < ERASE_ITERATIONS; i++) {
        if (i % 100 == 0) {
            ESP_LOGI(TAG, "%s: erase stressing iteration %u/%u", __func__, i, ERASE_ITERATIONS);

            uint32_t rand = esp_random();

            // random early break allows for some variance in erase counts
            if (rand % 100 < 5) {
                ESP_LOGI(TAG, "%s: random cycle break at iteration %u", __func__, i);
                break;
            }

            // or remounting will move dummy sector 'a lot', making move_count increment (and in turn mapping shift) occur sooner
            if (rand % 100 < 20) {
                ESP_LOGI(TAG, "%s: will remount cycle at iteration %u", __func__, i);
                for (int j = 0; j < (rand % 100 + 15); j++) {
                    // unmounting always moves dummy sector
                    ESP_ERROR_CHECK(wl_unmount(s_wl_handle));
                    ESP_ERROR_CHECK(wl_mount(storage, &s_wl_handle));
                }
            }
            // erase a sector around the middle of wear leveled partition
            wl_erase_range(s_wl_handle, wl_size(s_wl_handle)/2 + (i%10 + rand%10), CONFIG_WL_SECTOR_SIZE);
        } else {
            // erase a sector in the middle of wear leveled partition
            wl_erase_range(s_wl_handle, wl_size(s_wl_handle)/2, CONFIG_WL_SECTOR_SIZE);
        }

    }

    ESP_ERROR_CHECK(wl_unmount(s_wl_handle));
#endif

    full_runs++;
    ESP_LOGW(TAG, "Run %u/%u finished", full_runs, FULL_RUNS);

    if (full_runs < FULL_RUNS)
        goto start;

    ESP_LOGI(TAG, "===== END =====");
}
