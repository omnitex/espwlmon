#include "wlmon.h"

const esp_partition_t *get_wl_partition(void *arg)
{
    ESP_LOGD(TAG, "linux get_wl_partition(%s)", arg);
    // TODO get the partition dump file, copy to allocated memory (?)
    // and allocate esp_partition_t and fill with needed values, return it
}

esp_err_t get_wl_config(wl_config_t *cfg, const esp_partition_t *part)
{
    ESP_LOGD(TAG, "linux get_wl_config()");
}