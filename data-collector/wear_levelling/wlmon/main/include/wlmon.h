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
     *       - ESP_OK, if WL status was reconstructed successfully
     *       - ESP_ERR_INVALID_CRC, if CRC of wl_state_t that is read does not match its stored CRC
     *       - ESP_ERR_NO_MEM, if memory allocation for temp_buff fails
     *       - ESP_ERR_FLASH_OP_FAIL, if recovering pos from flash fails
    */
    virtual esp_err_t reconstruct(wl_config_t *cfg, Flash_Access *flash_drv);

    /**
     * @brief Writes a complete WL status in JSON format to the given buffer of given length
     *
     * @param s Buffer for writing the JSON WL status into
     * @param n Length in bytes of the buffer
     *
     * @return
     *       - ESP_OK, if full write was successful
     *       - ESP_FAIl, if JSON could not be written in full
    */
    virtual esp_err_t write_wl_status_json(char *s, size_t n);

    /**
     * @brief Reallocates given buffer to additionally fit erase counts from WL_Advanced
     *
     * @param[out] buffer Buffer to realloc. On failure it remains valid and of size as before calling this method
     * @param[out] new_size New size of buffer after successful realloc
     *
     * @return
     *       - ESP_OK, if buffer could be reallocated to fit erase counts
     *       - ESP_ERR_NO_MEM, if realloc failed
    */
    virtual esp_err_t resize_json_buffer(char **buffer, uint32_t *new_size);

    /**
     * @brief Get detected WL mode
     *
     * @return {WL_MODE_ADVANCED,WL_MODE_BASE,WL_MODE_UNDEFINED}
    */
    //TODO typedef enum for wl_mode_* type?
    virtual uint8_t get_wl_mode();

private:
    int write_wl_config_json(char *s, size_t n);
    int write_wl_state_json(char *s, size_t n);
    int write_wl_mode_json(char *s, size_t n);
    int write_wl_erase_counts_json(char *s, size_t n);

    esp_err_t recoverPos();
    esp_err_t checkStateCRC(wl_state_t *state);

    unsigned int wl_mode;
};

/**
 * @brief All-in-one convenience function.
 * Allocates required buffers, instantiates wlmon, reconstructs status and writes JSON to buffer
 *
 * @param[out] buffer Buffer for JSON status output. Will be allocated to fit the JSON.
 *
 * @return
 *       - ESP_OK, if all steps in getting WL status succeed
 *       - various ESP_ERR_* otherwise
*/
esp_err_t wlmon_get_status(char **buffer);

#endif // #ifndef _WLMON_H_
