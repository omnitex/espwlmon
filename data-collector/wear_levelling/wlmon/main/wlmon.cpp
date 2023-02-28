#include <new>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "spi_flash_mmap.h"
#include "esp_flash_err.h"
#include "esp32/rom/crc.h"

#include "Partition.h"
#include "wlmon.h"

static const char *TAG = "wlmon";

void print_error_json(esp_err_t result)
{
    printf("{");

    printf("\"error\":");
    printf("\"%s\"", esp_err_to_name(result));

    // TODO add verbose message here? simple error name from above could be interpreted at FE
    // but the semantics could change and it would need to be updated across BE/FE
    // or print verbose message here and the potential changes that would need to be addressed stay within data-collector

    printf("}\n");
    fflush(stdout);
}

esp_err_t get_wl_partition(const esp_partition_t **partition)
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

esp_err_t checkStateCRC(wl_state_t *state)
{
    if ( state->crc == crc32_le(WL_CFG_CRC_CONST, (const uint8_t *)state, offsetof(wl_state_t, crc)) ) {
        return ESP_OK;
    } else {
        return ESP_ERR_INVALID_CRC;
    }
}

esp_err_t checkConfigCRC(wl_config_t *cfg)
{
    if ( cfg->crc == crc32_le(WL_CFG_CRC_CONST, (const uint8_t *)cfg, offsetof(wl_config_t, crc)) ) {
        return ESP_OK;
    } else {
        return ESP_ERR_INVALID_CRC;
    }
}

void wl_detach(Partition *part, WLmon_Flash *wlmon_flash)
{
    if (part) {
        part->~Partition();
        free(part);
    }
    if (wlmon_flash) {
        wlmon_flash->~WLmon_Flash();
        free(wlmon_flash);
    }
}

esp_err_t wl_attach(const esp_partition_t *partition, WLmon_Flash **wlmon_instance)
{
    void *wlmon_flash_ptr = NULL;
    WLmon_Flash *wlmon_flash = NULL;
    void *part_ptr = NULL;
    Partition *part = NULL;

    wl_config_t cfg;
    esp_err_t result = ESP_OK;

    part_ptr = malloc(sizeof(Partition));
    if (part_ptr == NULL) {
        ESP_LOGE(TAG, "%s: can't allocate Partition", __func__);
        return ESP_ERR_NO_MEM;
    }
    part = new (part_ptr) Partition(partition);

    wlmon_flash_ptr = malloc(sizeof(WLmon_Flash));
    if (wlmon_flash_ptr == NULL) {
        ESP_LOGE(TAG, "%s: can't allocate WLmon_Flash", __func__);

        wl_detach(part, wlmon_flash);

        return ESP_ERR_NO_MEM;
    }
    wlmon_flash = new (wlmon_flash_ptr) WLmon_Flash();

    // get_wl_config() verifies config CRC
    result = get_wl_config(&cfg, partition);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed getting WL config from flash");

        wl_detach(part, wlmon_flash);

        return result;
    }

    // TODO part implementing read for linux target
    result = wlmon_flash->reconstruct(&cfg, part);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed reconstructing WL info");

        wl_detach(part, wlmon_flash);

        return result;
    }

    *wlmon_instance = wlmon_flash;
    return ESP_OK;
}

