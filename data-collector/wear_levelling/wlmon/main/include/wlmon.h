#ifndef _WLMON_H_
#define _WLMON_H_

#include "WL_Flash.h"
#include "esp_partition.h"

#ifndef WL_CFG_CRC_CONST
#define WL_CFG_CRC_CONST UINT32_MAX
#endif

class WLmon_Flash: WL_Flash
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
     *       - ESP_ERR_FLASH_OP_FAIL, if recovering pos from flash fails
    */
    //TODO virtual or not?
    virtual esp_err_t reconstruct(wl_config_t *cfg, Flash_Access *flash_drv);

    void print_wl_status_json();

private:
    void print_wl_config_json();
    void print_wl_state_json();
};

/**
 * @brief Find and return WL partition, if present (in flash or in partition image, target vs linux, TODO).
 *
 * @param arg On target, unused, searching for partition in flash. TODO: On linux file containing partition image.
 * @param[out] partition Pointer for passing found (or constructed) WL partition
 *
 * @return
 *       - ESP_OK, if WL partition is found and config is retrieved, including CRC check
 *       - ESP_ERR_NOT_FOUND, if no candidate for WL partition was found
 *       - ESP_ERR_FLASH_PROTECTED, if last processed candidate paritition was encrypted
 *       - ESP_ERR_INVALID_CRC, if last processed candidate partition failed config CRC check
 *       - ESP_ERR_NOT_FOUND, if no candidate partition is found
*/
esp_err_t get_wl_partition(void *arg, const esp_partition_t **partition);

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
 * @param[out] wlmon_instance Pointer for passing the created and filled Wlmon_Flash instance
 *
 * @return
 *       - ESP_OK, if attaching was successful and wlmon_instance points to created instance
 *       - ESP_ERR_NO_MEM, if memory allocation for instances or temp buffer failed
 *       - ESP_ERR_FLASH_PROTECTED, if attempting to reconstruct from encrypted partition
 *       - ESP_ERR_INVALID_CRC, if CRC check of either WL config or state failed
 *       - ESP_ERR_FLASH_OP_FAIL, if reading from flash when reconstructing status fails
*/
esp_err_t wl_attach(const esp_partition_t *partition, WLmon_Flash **wlmon_instance);

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

/**
 * @brief Print (to STDOUT) JSON formatted message concerning error in getting WL partition or attaching to WL
 *
 * @param result esp_err_t code of error
*/
void print_error_json(esp_err_t result);

#endif
