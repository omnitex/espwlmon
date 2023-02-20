#include "esp_log.h"
#include "wlmon.h"

static const char *TAG = "wlmon";

esp_err_t get_wl_partition(void *arg, const esp_partition_t **partition)
{
    ESP_LOGD(TAG, "linux get_wl_partition(%s)", arg);
    // TODO get the partition dump file, copy to allocated memory (?)
    // and allocate esp_partition_t and fill with needed values, return it
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t get_wl_config(wl_config_t *cfg, const esp_partition_t *part)
{
    ESP_LOGD(TAG, "linux get_wl_config()");

    return ESP_ERR_NOT_SUPPORTED;
}