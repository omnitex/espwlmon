#include <new>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "spi_flash_mmap.h"
#include "esp32/rom/crc.h"

#include "Partition.h"
#include "wlmon.h"

static const char *TAG = "wlmon";

/**
 * @brief Check that WL config CRC matches its stored CRC
 *
 * @param state wl_config_t of which to check CRC
 *
 * @return
 *       - ESP_OK, if calculated CRC matches stored CRC
 *       - ESP_ERR_INVALID_CRC, if calculated CRC differs from stored CRC
*/
esp_err_t checkConfigCRC(wl_config_t *cfg)
{
    if ( cfg->crc == crc32_le(WL_CFG_CRC_CONST, (const uint8_t *)cfg, offsetof(wl_config_t, crc)) ) {
        return ESP_OK;
    } else {
        return ESP_ERR_INVALID_CRC;
    }
}

/**
 * @brief Obtain valid, if present, wear leveling config from given partition
 *
 * @param cfg Pointer to config which will be written
 * @param partition Partition from which to obtain valid WL config
 *
 * @return
 *       - ESP_OK, if config was read, is valid and written correctly;
 *       - ESP_ERR_NOT_SUPPORTED, if partition has encrypted flag set (TODO could we work with encrypted parititon?)
 *       - ESP_ERR_INVALID_CRC, if config CRC failed to match its stored CRC
*/
esp_err_t get_wl_config(wl_config_t *cfg, const esp_partition_t *partition)
{
    esp_err_t result = ESP_OK;

    if (partition->encrypted) {
        ESP_LOGE(TAG, "%s: cannot read config from encrypted partition!", __func__);
        return ESP_ERR_NOT_SUPPORTED;
    }

    size_t cfg_address = partition->size - SPI_FLASH_SEC_SIZE; // fixed position of config struct; last sector of partition

    esp_partition_read(partition, cfg_address, cfg, sizeof(wl_config_t));

    result = checkConfigCRC(cfg);
    if (result != ESP_OK) {
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

/**
 * @brief Find and return WL partition, if present (in flash or in partition image, target vs linux, TODO).
 *
 * @param[out] partition Pointer for passing found (or constructed) WL partition
 *
 * @return
 *       - ESP_OK, if WL partition is found and config is retrieved, including CRC check
 *       - ESP_ERR_NOT_FOUND, if no candidate for WL partition was found
 *       - ESP_ERR_NOT_SUPPORTED, if last processed candidate paritition was encrypted
 *       - ESP_ERR_INVALID_CRC, if last processed candidate partition failed config CRC check
 *       - ESP_ERR_NOT_FOUND, if no candidate partition is found
*/
esp_err_t get_wl_partition(const esp_partition_t **partition)
{
    wl_config_t test_cfg = {};
    // default to not found; any candidate partition overwrites this by get_wl_config()
    esp_err_t result = ESP_ERR_NOT_FOUND;

    const esp_partition_t *candidate = NULL;

    // subtype any for potential data partitions different than FAT
    esp_partition_iterator_t iterator = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);

    // iterate through all data partitions
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

/**
 * @brief Reconstructs WL status and stores it in created WLmon_Flash instance
 *
 * @param partition Partition to which to "attach"; from which to reconstruct WL status
 * @param[out] wlmon_instance Pointer for passing the created and filled Wlmon_Flash instance
 *
 * @return
 *       - ESP_OK, if attaching was successful and wlmon_instance points to created instance
 *       - ESP_ERR_NO_MEM, if memory allocation for instances or temp buffer failed
 *       - ESP_ERR_NOT_SUPPORTED, if attempting to reconstruct from encrypted partition
 *       - ESP_ERR_INVALID_CRC, if CRC check of either WL config or state failed
 *       - ESP_ERR_FLASH_OP_FAIL, if reading from flash when reconstructing status fails
*/
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

int write_error_json(char *s, size_t n, esp_err_t errcode)
{
    int retval = snprintf(s, n, "{\"error\":\"%s\"}\n", esp_err_to_name(errcode));

    // TODO add verbose message here? simple error name from above could be interpreted at FE
    // but the semantics could change and it would need to be updated across BE/FE
    // or print verbose message here and the potential changes that would need to be addressed stay within data-collector
    return retval;
}


esp_err_t wlmon_get_status(char **buffer)
{
    esp_err_t result = ESP_OK;
    const esp_partition_t *partition = NULL;
    WLmon_Flash *wl_instance;

    *buffer = (char *)malloc(WLMON_BUF_SIZE);
    if (*buffer == NULL) {
        result = ESP_ERR_NO_MEM;
    }
    WL_RESULT_CHECK(result);

    result = get_wl_partition(&partition);
    if (result != ESP_OK)
        write_error_json(*buffer, WLMON_BUF_SIZE, result);
    WL_RESULT_CHECK(result);

    result = wl_attach(partition, &wl_instance);
    if (result != ESP_OK)
        write_error_json(*buffer, WLMON_BUF_SIZE, result);
    WL_RESULT_CHECK(result);

    result = wl_instance->write_wl_status_json(*buffer, WLMON_BUF_SIZE);
    if (result != ESP_OK)
        write_error_json(*buffer, WLMON_BUF_SIZE, result);
    WL_RESULT_CHECK(result);

    result = ESP_OK;

    return result;
}
