#include "esp_log.h"
#include "esp32/rom/crc.h"
#include "wlmon.h"

static const char *TAG = "wlmon";

const esp_partition_t *get_wl_partition(const char *arg)
{
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "storage"
    );

    // TODO check fatfs boot sector size vs partition size

    return partition;
}

esp_err_t get_wl_config(wl_config_t *cfg, const esp_partition_t *partition)
{
    if (partition->encrypted) {
        ESP_LOGE(TAG, "%s: cannot read config from encrypted partition!", __func__);
        return ESP_ERR_FLASH_PROTECTED;
    }

    ESP_LOGI(TAG, "%s: for partition @0x%x of size %u (0x%x)",
            __func__,
            partition->address,
            partition->size,
            partition->size);

    size_t cfg_address = partition->size - SPI_FLASH_SEC_SIZE; // fixed position of config struct; last sector of partition

    esp_partition_read(partition, cfg_address, cfg, sizeof(wl_config_t));

    if (cfg->crc != crc32_le(WL_CFG_CRC_CONST, (const uint8_t *)cfg, offsetof(wl_config_t, crc))) {
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}