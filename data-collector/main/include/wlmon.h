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

    /**
     * @brief Reconstruct WL status (values of counters, addresses etc.) from given config and from flash, saving it to instance.
     *
     * @param cfg Config previously obtained primarily from get_wl_config(), gets copied to instance
     * @param flash_drv Instance of Flash_Access needed for read function
     *
     * @return
     *       - ESP_OK, if WL status was reconstructed successfuly
     *       - ESP_ERR_INVALID_CRC, if CRC of wl_state_t that is read does not match its stored CRC
     *       - ESP_ERR_NO_MEM, if memory allocation for temp_buff fails
    */
    //TODO virtual or not?
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

/**
 * @brief Find and return WL partition, if present (in flash or in partition image, target vs linux, TODO).
 *
 * @param arg On target, unused, searching for partition in flash. TODO: On linux file containing partition image.
 *
 * @return Pointer to a found (or constructed) WL partition or NULL
*/
const esp_partition_t *get_wl_partition(void *arg);

/**
 * @brief Obtain valid, if present, wear leveling config from given partition
 *
 * @param cfg Pointer to config which will be written
 * @param partition Partition from which to obtain valid WL config
 *
 * @return
 *       - ESP_OK, if config was read, is valid and written correctly;
 *       - ESP_ERR_FLASH_PROTECTED, if partition has encrypted flag set
 *       - ESP_ERR_INVALID_CRC, if config CRC failed to match its stored CRC
*/
esp_err_t get_wl_config(wl_config_t *cfg, const esp_partition_t *partition);

/**
 * @brief Reconstructs WL status and stores it in created WLmon_Flash instance
 *
 * @param partition Partition to which to "attach"; from which to reconstruct WL status
 *
 * @return Created and filled WLmon_Flash instance or NULL
*/
WLmon_Flash *wl_attach(const esp_partition_t *partition);

/**
 * @brief Check that WL state CRC matches its stored CRC
 *
 * @param state wl_state_t of which to check CRC
 *
 * @return
 *       - ESP_OK, if calculated CRC matches stored CRC
 *       - ESP_ERR_INVALID_CRC, if calculated CRC differs from stored CRC
*/
esp_err_t checkStateCRC(wl_state_t *state);

/**
 * @brief Check that WL config CRC matches its stored CRC
 *
 * @param state wl_config_t of which to check CRC
 *
 * @return
 *       - ESP_OK, if calculated CRC matches stored CRC
 *       - ESP_ERR_INVALID_CRC, if calculated CRC differs from stored CRC
*/
esp_err_t checkConfigCRC(wl_config_t *cfg);

/**
 * @brief Print (to STDOUT) WL status contained in given instance as a JSON.
 *
 * @param wl Wlmon_Flash instance with reconstructed WL status
*/
void print_wl_status_json(WLmon_Flash *wl);

#endif