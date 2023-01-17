#include <new>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_spi_flash.h" // for SPI_FLASH_SEC_SIZE
#include "esp32/rom/crc.h"
#include "wlmon.h"

static const char *TAG = "WLmon";

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

    ESP_LOGI(TAG, "\ncalculated crc: 0x%x", crc32_le(UINT32_MAX, (const unsigned char *)cfg, offsetof(wl_config_t, crc)));

    fflush(stdout);
}

#if 0
    void *wlmon_flash_ptr = NULL;
    WLmon_Flash *wlmon_flash = NULL;

    wlmon_flash_ptr = malloc(sizeof(WLmon_Flash));
    if (wlmon_flash_ptr == NULL) {
        ESP_LOGE(TAG, "%s: can't allocate WLmon_Flash", __func__);
        return;
    }
    wlmon_flash = new (wlmon_flash_ptr) WLmon_Flash();

    wlmon_flash->config(&cfg, (Flash_Access *)partition);
#endif