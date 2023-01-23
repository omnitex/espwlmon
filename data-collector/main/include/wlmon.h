#ifndef _WLMON_H_
#define _WLMON_H_

#include "Flash_Access.h"
#include "esp_partition.h"

#ifndef WL_CFG_CRC_CONST
#define WL_CFG_CRC_CONST UINT32_MAX
#endif

#define WL_RESULT_CHECK(result) \
    if (result != ESP_OK) { \
        ESP_LOGE(TAG,"%s(%d): result = 0x%08x", __FUNCTION__, __LINE__, result); \
        return (result); \
    }


/* Struct definitions taken from WL_Config.h and WL_State.h*/
typedef struct WL_Config_s {
    size_t   start_addr;    /*!< start address in the flash*/
    uint32_t full_mem_size; /*!< Amount of memory used to store data in bytes*/
    uint32_t page_size;     /*!< One page size in bytes. Page could be more then memory block. This parameter must be page_size >= N*block_size.*/
    uint32_t sector_size;   /*!< size of flash memory sector that will be erased and stored at once (erase)*/
    uint32_t updaterate;    /*!< Amount of accesses before block will be moved*/
    uint32_t wr_size;       /*!< Minimum amount of bytes per one block at write operation: 1...*/
    uint32_t version;       /*!< A version of current implementation. To erase and reallocate complete memory this ID must be different from id before.*/
    size_t   temp_buff_size;  /*!< Size of temporary allocated buffer to copy from one flash area to another. The best way, if this value will be equal to sector size.*/
    uint32_t crc;           /*!< CRC for this config*/
} wl_config_t;

typedef struct WL_State_s {
public:
    uint32_t pos;           /*!< current dummy block position*/
    uint32_t max_pos;       /*!< maximum amount of positions*/
    uint32_t move_count;    /*!< total amount of move counts. Used to calculate the address*/
    uint32_t access_count;  /*!< current access count*/
    uint32_t max_count;     /*!< max access count when block will be moved*/
    uint32_t block_size;    /*!< size of move block*/
    uint32_t version;       /*!< state id used to identify the version of current library implementation*/
    uint32_t device_id;     /*!< ID of current WL instance*/
    uint32_t reserved[7];   /*!< Reserved space for future use*/
    uint32_t crc;           /*!< CRC of structure*/
} wl_state_t;

// simplified WL_FLash class from WL_Flash.h
class WLmon_Flash
{
public :
    WLmon_Flash();
    ~WLmon_Flash();

    virtual esp_err_t reconstruct(wl_config_t *cfg, Flash_Access *flash_drv);
    esp_err_t recoverPos();

    void fillOkBuff(int n);
    bool OkBuffSet(int n);

    wl_state_t state;
    wl_config_t cfg;

    Flash_Access *flash_drv = NULL;

    size_t addr_state1;
    size_t addr_state2;

    uint32_t state_size;
    uint32_t cfg_size;
    uint8_t *temp_buff = NULL;
    size_t dummy_addr;
};

const esp_partition_t *get_wl_partition(const char *arg);

esp_err_t get_wl_config(wl_config_t *cfg, const esp_partition_t *part);

WLmon_Flash *wl_attach(const esp_partition_t *partition);

void print_wl_status_json(WLmon_Flash *wl);

#endif