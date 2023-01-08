#include <stdio.h>
#include "esp_partition.h"

void print_partition_info()
{
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");
    printf("part: %p\n", part);
}

