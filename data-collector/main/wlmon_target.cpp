#include "wlmon.h"

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