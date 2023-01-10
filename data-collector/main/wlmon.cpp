#include <stdio.h>
#include <string.h>
#include "esp_spi_flash.h" // for SPI_FLASH_SEC_SIZE
#include "wlmon.h"

void print_config_json(const esp_partition_t *partition)
{
    if (partition->encrypted) {
        ESP_LOGE(TAG, "Cannot read config from enrypted partition!");
        return;
    }

    ESP_LOGI(TAG, "print_config() for partition @0x%x of size %u (0x%x)",
            partition->address,
            partition->size,
            partition->size);
    
    wl_config_t cfg;
    size_t cfg_address = partition->size - SPI_FLASH_SEC_SIZE; // fixed position of config struct; last sector of partition

#if CONFIG_IDF_TARGET_LINUX
    memcpy(&cfg, (const void *)cfg_address, sizeof(cfg));
#else
    esp_partition_read(partition, cfg_address, &cfg, sizeof(cfg));
#endif

    printf("{");
    printf("\"start_addr\":\"0x%x\",", cfg.start_addr);
    printf("\"full_mem_size\":\"0x%x\",", cfg.full_mem_size);
    printf("\"page_size\":\"0x%x\",", cfg.page_size);
    printf("\"sector_size\":\"0x%x\",", cfg.sector_size);
    printf("\"updaterate\":\"0x%x\",", cfg.updaterate);
    printf("\"wr_size\":\"0x%x\",", cfg.wr_size);
    printf("\"version\":\"0x%x\",", cfg.version);
    printf("\"temp_buff_size\":\"0x%x\",", cfg.temp_buff_size);
    printf("\"crc\":\"0x%x\"", cfg.crc);
    printf("}");
    fflush(stdout);
}