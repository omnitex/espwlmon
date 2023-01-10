#ifndef _WLMON_H_
#define _WLMON_H_

#include "esp_log.h"
#include "esp_partition.h"

typedef struct WL_Config_s {
    size_t   start_addr;
    uint32_t full_mem_size;
    uint32_t page_size;
    uint32_t sector_size;
    uint32_t updaterate;
    uint32_t wr_size;
    uint32_t version;
    size_t   temp_buff_size;
    uint32_t crc;
} wl_config_t;

extern const char *TAG;

void print_config_json(const esp_partition_t *partition);

const esp_partition_t *get_wl_partition(const char *arg);

#endif