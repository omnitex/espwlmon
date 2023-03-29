#ifndef __WL_SIM_H__
#define __WL_SIM_H__

#include <cstdint>

// following values copied from what WLmon_Flash reconstructed
#define FULL_MEM_SIZE 0x100000 // 1MB
#define SECTOR_SIZE (2<<11)
#define PAGE_SIZE (SECTOR_SIZE)
#define STATE_SIZE 0x2000
#define CFG_SIZE 0x1000

#define FLASH_SIZE (((FULL_MEM_SIZE - STATE_SIZE * 2 - CFG_SIZE) / PAGE_SIZE - 1 ) * PAGE_SIZE)

#define UPDATERATE 0x10
#define WR_SIZE 0x10
#define TEMP_BUFF_SIZE 0x20

#define MAX_COUNT (UPDATERATE)
#define MAX_POS (1 + FLASH_SIZE / PAGE_SIZE)

#define SECTOR_COUNT (FLASH_SIZE / SECTOR_SIZE)

typedef struct WL_State_s {
    uint32_t pos;
    uint32_t max_pos;
    uint32_t move_count;
    uint32_t access_count;
    uint32_t max_count;
    uint32_t block_size;
    uint32_t version;
    uint32_t device_id;
    uint32_t reserved[7];
    uint32_t crc;
} wl_state_t;

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

#endif // __WL_SIM_H__

