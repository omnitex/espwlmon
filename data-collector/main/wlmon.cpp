#include <new>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_spi_flash.h" // for SPI_FLASH_SEC_SIZE
#include "esp32/rom/crc.h"

#include "Partition.h"
#include "wlmon.h"

static const char *TAG = "wlmon";

void print_config_json(const wl_config_t *cfg)
{
    printf("{");
    printf("\"start_addr\":\"0x%x\",", cfg->start_addr);
    printf("\"full_mem_size\":\"0x%x\",", cfg->full_mem_size);
    printf("\"page_size\":\"0x%x\",", cfg->page_size);
    printf("\"sector_size\":\"0x%x\",", cfg->sector_size);
    printf("\"updaterate\":\"0x%x\",", cfg->updaterate);
    printf("\"wr_size\":\"0x%x\",", cfg->wr_size);
    printf("\"version\":\"0x%x\",", cfg->version);
    printf("\"temp_buff_size\":\"0x%x\",", cfg->temp_buff_size);
    printf("\"crc\":\"0x%x\"", cfg->crc);
    printf("}");
    fflush(stdout);
}

WLmon_Flash *wl_attach(const esp_partition_t *partition)
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
        return NULL;
    }
    part = new (part_ptr) Partition(partition);

    wlmon_flash_ptr = malloc(sizeof(WLmon_Flash));
    if (wlmon_flash_ptr == NULL) {
        ESP_LOGE(TAG, "%s: can't allocate WLmon_Flash", __func__);
        return NULL;
    }
    wlmon_flash = new (wlmon_flash_ptr) WLmon_Flash();

    result = get_wl_config(&cfg, partition);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed getting WL config from flash");
        //TODO wl_detach()
        return NULL;
    }

    result = wlmon_flash->reconstruct(&cfg, part);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed reconstructing WL info");
        //TODO cleanup
        return NULL;
    }

    return wlmon_flash;
}

void wl_detach()
{
    // TODO free allocated memory
}
