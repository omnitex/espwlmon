#pragma once

#include <cstdint>

// following values copied from what WLmon_Flash reconstructed
// so they are not really made for flexible changes full mem size
#define FULL_MEM_SIZE 0x100000 // 1MB
#define SECTOR_SIZE (2<<11)
#define PAGE_SIZE (SECTOR_SIZE)
#define STATE_SIZE 0x2000
#define CFG_SIZE 0x1000

#define FLASH_SIZE (((FULL_MEM_SIZE - STATE_SIZE * 2 - CFG_SIZE) / PAGE_SIZE - 1 ) * PAGE_SIZE)

#define UPDATERATE 0x10
#define WR_SIZE 0x10
#define TEMP_BUFF_SIZE 0x20

#define ERASE_SIZE (SECTOR_SIZE)

#define MAX_COUNT (UPDATERATE)
#define MAX_POS (1 + FLASH_SIZE / PAGE_SIZE)

#define SECTOR_COUNT (FLASH_SIZE / SECTOR_SIZE)
