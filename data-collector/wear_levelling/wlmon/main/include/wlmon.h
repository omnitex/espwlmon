#ifndef _WLMON_H_
#define _WLMON_H_

#include "WL_Flash.h"
#include "WL_Advanced.h"
#include "esp_partition.h"

#ifndef WL_CFG_CRC_CONST
#define WL_CFG_CRC_CONST UINT32_MAX
#endif

#define WLMON_DEFAULT_BUF_SIZE 500
#define WLMON_BUF_SIZE 4096

#define WL_MODE_UNDEFINED 0x0
#define WL_MODE_BASE 0x1
#define WL_MODE_ADVANCED 0x2

#define WL_RESULT_CHECK(result) \
    if (result != ESP_OK) { \
        ESP_LOGE(TAG,"%s(%d): result = 0x%08x", __FUNCTION__, __LINE__, result); \
        return (result); \
    }

class WLmon_Flash: WL_Advanced
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

    // write a complete WL status as JSON to the given buffer
    esp_err_t write_wl_status_json(char *s, size_t n);

    esp_err_t resize_json_buffer(char **buffer, uint32_t *new_size);

private:
    int write_wl_config_json(char *s, size_t n);
    int write_wl_state_json(char *s, size_t n);
    int write_wl_mode_json(char *s, size_t n);
    int write_wl_erase_counts_json(char *s, size_t n);

    //bool OkBuffSet(int pos);
    esp_err_t recoverPos();

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

    unsigned int wl_mode;
};

esp_err_t wlmon_get_status(char **buffer);

#endif // #ifndef _WLMON_H_
