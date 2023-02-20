#include <new>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "spi_flash_mmap.h"
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

