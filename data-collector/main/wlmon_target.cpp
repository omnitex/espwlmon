#include "esp_log.h"
#include "esp32/rom/crc.h"
#include "wlmon.h"

static const char *TAG = "wlmon";

esp_err_t get_wl_partition(void *arg, const esp_partition_t **partition)
{
    wl_config_t test_cfg = {};
    // default to not found; any candidate partition overwrites this by get_wl_config()
    esp_err_t result = ESP_ERR_NOT_FOUND;

    const esp_partition_t *candidate = NULL;

    // subtype any for potential data partitions different than FAT
    esp_partition_iterator_t iterator = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);

    // iterate throught all data partitions
    while (iterator != NULL)
    {
        candidate = esp_partition_get(iterator);

        if (candidate != NULL) {
            ESP_LOGD(TAG, "WL partition candidate: '%s' at address 0x%x of size 0x%x", candidate->label, candidate->address, candidate->size);

            // getting config checks the CRC, which is valid only in WL partition, otherwise it's random data
            result = get_wl_config(&test_cfg, candidate);
            if (result == ESP_OK) {
                *partition = candidate;
                ESP_LOGV(TAG, "partition '%s' at address 0x%x of size 0x%x has correct WL config CRC", (*partition)->label, (*partition)->address, (*partition)->size);
                break;
            }
        }

        iterator = esp_partition_next(iterator);
    }

    esp_partition_iterator_release(iterator);

    return result;
}

esp_err_t get_wl_config(wl_config_t *cfg, const esp_partition_t *partition)
{
    esp_err_t result = ESP_OK;

    if (partition->encrypted) {
        ESP_LOGE(TAG, "%s: cannot read config from encrypted partition!", __func__);
        return ESP_ERR_FLASH_PROTECTED;
    }

    size_t cfg_address = partition->size - SPI_FLASH_SEC_SIZE; // fixed position of config struct; last sector of partition

    esp_partition_read(partition, cfg_address, cfg, sizeof(wl_config_t));

    result = checkConfigCRC(cfg);
    if (result != ESP_OK) {
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}