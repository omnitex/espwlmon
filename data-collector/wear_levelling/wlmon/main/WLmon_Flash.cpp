#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp32/rom/crc.h"
#include "spi_flash_mmap.h"
#include "wlmon.h"

static const char *TAG = "wlmon";

#define SNPRINTF_RETVAL_CHECK(retval, s, n) do{if(retval>=0&&retval<n){s+=retval;n-=retval;}else{return ESP_FAIL;}}while(0)

WLmon_Flash::WLmon_Flash()
{
    this->wl_mode = WL_MODE_UNDEFINED;
}

WLmon_Flash::~WLmon_Flash() {}

esp_err_t WLmon_Flash::checkStateCRC(wl_state_t *state)
{
    if ( state->crc == crc32_le(WL_CFG_CRC_CONST, (const uint8_t *)state, offsetof(wl_state_t, crc)) ) {
        return ESP_OK;
    } else {
        return ESP_ERR_INVALID_CRC;
    }
}

esp_err_t WLmon_Flash::reconstruct(wl_config_t *cfg, Flash_Access *flash_drv)
{
    esp_err_t result = ESP_OK;

    this->flash_drv = flash_drv;

    memcpy(&this->cfg, cfg, sizeof(wl_config_t));

    // calculating state_size 
    // first, assume only 1 sector is needed
    this->state_size = this->cfg.sector_size;
    if (this->state_size < (sizeof(wl_state_t) + (this->cfg.full_mem_size / this->cfg.sector_size) * this->cfg.wr_size)) {
        // memory needed to fit wl_state_t + pos updates for all sectors in partition exceeds 1 sector
        this->state_size = ((sizeof(wl_state_t) + (this->cfg.full_mem_size / this->cfg.sector_size) * this->cfg.wr_size) + this->cfg.sector_size - 1) / this->cfg.sector_size;
        this->state_size = this->state_size * this->cfg.sector_size;
    }

    // config should always fit into a single sector, but calculate size aligned to sector that is needed
    this->cfg_size = (sizeof(wl_config_t) + this->cfg.sector_size - 1) / this->cfg.sector_size;
    this->cfg_size = this->cfg_size * this->cfg.sector_size;

    // wl_state_t at the end of memory in two copies
    this->addr_state1 = this->cfg.start_addr + this->cfg.full_mem_size - this->state_size * 2 - this->cfg_size;
    this->addr_state2 = this->cfg.start_addr + this->cfg.full_mem_size - this->state_size * 1 - this->cfg_size;

    // read state structure
    result = this->flash_drv->read(this->addr_state1, &this->state, sizeof(wl_state_t));

    result = this->checkStateCRC(&this->state);
    if (result != ESP_OK) {
        return ESP_ERR_INVALID_CRC;
    }

    wl_advanced_state_t *advanced_state = (wl_advanced_state_t *)&this->state;
    // first WL version detection, second check in recoverPos()
    // in advanced, Feistel keys are obviously non-zero
    if (advanced_state->feistel_keys != 0) {
        this->wl_mode = WL_MODE_ADVANCED;
    // in base, they are in reserved space which is set to 0
    } else {
        this->wl_mode = WL_MODE_BASE;
    }

    this->temp_buff = (uint8_t *) malloc(this->cfg.temp_buff_size);
    if (this->temp_buff == NULL) {
        result = ESP_ERR_NO_MEM;
    }
    WL_RESULT_CHECK(result);

    // allocate buffer which will hold a 2B number for each sector in partition
    this->erase_count_buffer_size = this->state.max_pos * sizeof(uint16_t);
    this->erase_count_buffer = (uint16_t *)malloc(this->erase_count_buffer_size);
    if (this->erase_count_buffer == NULL) {
        result = ESP_ERR_NO_MEM;
    }
    WL_RESULT_CHECK(result);

    ESP_LOGD(TAG, "%s: erase_count_buffer=%p", __func__, this->erase_count_buffer);

    result = this->recoverPos();
    if (result != ESP_OK) {
        // non OK code returned by flash_drv->read(), don't know which one, override with own code
        return ESP_ERR_FLASH_OP_FAIL;
    }

    // erase counts are recorded only by WL_Advanced
    if (this->wl_mode == WL_MODE_ADVANCED) {
        result = this->readEraseCounts();
        WL_RESULT_CHECK(result);

        ESP_LOGD(TAG, "%s: read erase counts", __func__);

        result = this->updateEraseCounts();
        WL_RESULT_CHECK(result);

        ESP_LOGD(TAG, "%s: updated erase counts", __func__);
    }

    return ESP_OK;
}

esp_err_t WLmon_Flash::recoverPos()
{
    ESP_LOGD(TAG, "%s", __func__);
    esp_err_t result = ESP_OK;

    // recover position using both base and advanced WL
    result = WL_Flash::recoverPos();
    WL_RESULT_CHECK(result);
    uint32_t pos_base = this->state.pos;

    result = WL_Advanced::recoverPos();
    WL_RESULT_CHECK(result);
    uint32_t pos_advanced = this->state.pos;

    ESP_LOGI(TAG, "%s: base=%u, advanced=%u, Feistel keys chosen wl_mode=0x%x", __func__, pos_base, pos_advanced, this->wl_mode);

    // if one recovered non-zero and the other zero
    // assume the non-zero is the one used
    if ((pos_base != 0) && (pos_advanced == 0)) {
        // should be base WL
        this->state.pos = pos_base;
        // compare to previously obtained mode based on Feistel keys
        if (this->wl_mode != WL_MODE_BASE) {
            ESP_LOGE(TAG, "%s: wl_mode=0x%x does not match mode detected by Feistel keys (0x%x)", __func__, WL_MODE_BASE, this->wl_mode);
            return ESP_ERR_INVALID_STATE;
        }
    } else if ((pos_base == 0) && (pos_advanced != 0)) {
        // should be advanced WL
        this->state.pos = pos_advanced;
        // compare to previously obtained mode based on Feistel keys
        if (this->wl_mode != WL_MODE_ADVANCED) {
            ESP_LOGE(TAG, "%s: wl_mode=0x%x does not match mode detected by Feistel keys (0x%x)", __func__, WL_MODE_ADVANCED, this->wl_mode);
            return ESP_ERR_INVALID_STATE;
        }
    } else if ((pos_base != 0) && (pos_advanced != 0)) {
        ESP_LOGE(TAG, "%s: both base and advanced pos are recoverable, invalid state", __func__);
        return ESP_ERR_INVALID_STATE;
    } else {
        ESP_LOGW(TAG, "%s: base and advanced pos are both zero, cannot check mode detected by Feistel keys (0x%x)", __func__, this->wl_mode);
        this->state.pos = 0;
    }

    return ESP_OK;
}

int WLmon_Flash::write_wl_config_json(char *s, size_t n)
{
    int retval = snprintf(s, n,
        "{\"start_addr\":\"0x%x\",\"full_mem_size\":\"0x%x\",\"page_size\":\"0x%x\",\
\"sector_size\":\"0x%x\",\"updaterate\":\"0x%x\",\"wr_size\":\"0x%x\",\
\"version\":\"0x%x\",\"temp_buff_size\":\"0x%x\",\"crc\":\"0x%x\"}",
        this->cfg.start_addr, this->cfg.full_mem_size, this->cfg.page_size, this->cfg.sector_size,
        this->cfg.updaterate, this->cfg.wr_size, this->cfg.version, this->cfg.temp_buff_size, this->cfg.crc);

    return retval;
}

int WLmon_Flash::write_wl_state_json(char *s, size_t n)
{
    int retval;

    if (this->wl_mode == WL_MODE_ADVANCED) {

        wl_advanced_state_t *advanced_state = (wl_advanced_state_t *)&this->state;
        uint8_t *keys = (uint8_t *)&advanced_state->feistel_keys;

        retval = snprintf(s, n,
            "{\"pos\":\"0x%x\",\"max_pos\":\"0x%x\",\"move_count\":\"0x%x\",\
\"access_count\":\"0x%x\",\"max_count\":\"0x%x\",\"block_size\":\"0x%x\",\
\"version\":\"0x%x\",\"device_id\":\"0x%x\",\"cycle_count\":\"0x%x\",\"feistel_keys\":[\"0x%x\",\"0x%x\",\"0x%x\"],\"crc\":\"0x%x\"}",
            this->state.pos, this->state.max_pos, this->state.move_count, this->state.access_count,
            this->state.max_count, this->state.block_size, this->state.version, this->state.device_id,
            advanced_state->cycle_count, keys[0], keys[1], keys[2], this->state.crc);

    } else {

        retval = snprintf(s, n,
            "{\"pos\":\"0x%x\",\"max_pos\":\"0x%x\",\"move_count\":\"0x%x\",\
\"access_count\":\"0x%x\",\"max_count\":\"0x%x\",\"block_size\":\"0x%x\",\
\"version\":\"0x%x\",\"device_id\":\"0x%x\",\"crc\":\"0x%x\"}",
            this->state.pos, this->state.max_pos, this->state.move_count, this->state.access_count,
            this->state.max_count, this->state.block_size, this->state.version, this->state.device_id, this->state.crc);
    }

    return retval;
}

int WLmon_Flash::write_wl_erase_counts_json(char *s, size_t n)
{
    int retval, total_retval = 0;
    // flag for writing 2nd and following erase counts with a separating comma
    bool written = false;

    retval = snprintf(s, n, "{");
    SNPRINTF_RETVAL_CHECK(retval, s, n);
    total_retval += retval;

    // write all non-zero erase counts as JSON in the format:
    // "sector_num":"erase_count",...
    for (uint32_t i = 0; i < this->state.max_pos; i++) {
        if (this->erase_count_buffer[i] != 0) {
            retval = snprintf(s, n, "%s\"%u\":\"%u\"", written ? "," : "", i, this->erase_count_buffer[i]);
            SNPRINTF_RETVAL_CHECK(retval, s, n);
            total_retval += retval;
            // first pair written, from now on append with comma
            written = true;
        }
    }

    retval = snprintf(s, n, "}");
    SNPRINTF_RETVAL_CHECK(retval, s, n);
    total_retval += retval;

    return total_retval;
}

int WLmon_Flash::write_wl_mode_json(char *s, size_t n)
{
    int retval;

    switch(this->wl_mode) {
        case WL_MODE_BASE:
            retval = snprintf(s, n, "\"base\"");
            break;
        case WL_MODE_ADVANCED:
            retval = snprintf(s, n, "\"advanced\"");
            break;
        default:
            retval = snprintf(s, n, "\"undefined\"");
            break;
    }

    return retval;
}

esp_err_t WLmon_Flash::resize_json_buffer(char **buffer, uint32_t *new_size)
{
    // max_pos is calculated as a 1 + flash_size / sector_size; sector count is then one less
    uint32_t sector_count = this->state.max_pos - 1;
    uint32_t _sector_count = sector_count;
    uint32_t _new_size;
    uint8_t ascii_digits;

    // calculate how many ASCII digits can sector number be at max
    for (ascii_digits = 0; _sector_count; ascii_digits++)
        _sector_count = _sector_count / 10;

    // max len of one sector_num:erase_count pair in JSON
    uint8_t single_erase_count_len = sizeof("\"n\":\"100000\",") + ascii_digits;

    // max len for all sectors
    uint32_t erase_counts_len = single_erase_count_len * sector_count;

    ESP_LOGV(TAG, "%s: sector_count=%u, ascii_digits=%u, ascii_erase_count=%u, erase_counts_json_max_len=%u",
            __func__, sector_count, ascii_digits, single_erase_count_len, erase_counts_len);

    // default buff size for mode, config, state + enough for all erase counts
    _new_size = WLMON_DEFAULT_BUF_SIZE + erase_counts_len;

    // try to realloc to new size; so buffer fits whole WL status JSON including all erase counts
    char *new_buffer = (char *) realloc((void*)*buffer, _new_size);
    // on failure, origin buffer ptr remains valid
    if (new_buffer == NULL) {
        ESP_LOGE(TAG, "%s: realloc to %u B failed", __func__, _new_size);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "%s: reallocated: %p of size %u", __func__, *buffer, _new_size);

    *buffer = new_buffer;
    *new_size = _new_size;

    return ESP_OK;
}

uint8_t WLmon_Flash::get_wl_mode()
{
    return this->wl_mode;
}

esp_err_t WLmon_Flash::write_wl_status_json(char *s, size_t n)
{
    int retval, max_len = n;

    retval = snprintf(s, n, "{\"wl_mode\":");
    SNPRINTF_RETVAL_CHECK(retval, s, n);

    retval = write_wl_mode_json(s, n);
    SNPRINTF_RETVAL_CHECK(retval, s, n);

    retval = snprintf(s, n, ",\"config\":");
    SNPRINTF_RETVAL_CHECK(retval, s, n);

    retval = write_wl_config_json(s, n);
    SNPRINTF_RETVAL_CHECK(retval, s, n);

    retval = snprintf(s, n, ",\"state\":");
    SNPRINTF_RETVAL_CHECK(retval, s, n);

    retval = write_wl_state_json(s, n);
    SNPRINTF_RETVAL_CHECK(retval, s, n);

    if (this->wl_mode == WL_MODE_ADVANCED) {
        retval = snprintf(s, n, ",\"erase_counts\":");
        SNPRINTF_RETVAL_CHECK(retval, s, n);

        retval = write_wl_erase_counts_json(s, n);
        SNPRINTF_RETVAL_CHECK(retval, s, n);
    }

    retval = snprintf(s, n, "}\n");
    SNPRINTF_RETVAL_CHECK(retval, s, n);

    ESP_LOGI(TAG, "%s: written JSON of length %d (0x%x)", __func__, max_len-n, max_len-n);

    return ESP_OK;
}
