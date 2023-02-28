#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp32/rom/crc.h"
#include "spi_flash_mmap.h"
#include "wlmon.h"

static const char *TAG = "wlmon";

WLmon_Flash::WLmon_Flash() {}

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

    ESP_LOGV(TAG, "%s start_addr=0x%08x, full_mem_size=0x%08x, page_size=0x%08x, sector_size=0x%08x, updaterate=0x%08x, wr_size=0x%08x, version=0x%08x, temp_buff_size=0x%08x", __func__,
             (uint32_t) cfg->start_addr,
             cfg->full_mem_size,
             cfg->page_size,
             cfg->sector_size,
             cfg->updaterate,
             cfg->wr_size,
             cfg->version,
             (uint32_t) cfg->temp_buff_size);

    this->flash_drv = flash_drv;

    memcpy(&this->cfg, cfg, sizeof(wl_config_t));

    // calculating state_size 
    // first, assume only 1 sector is needed
    this->state_size = this->cfg.sector_size;
    if (this->state_size < (sizeof(wl_state_t) + (this->cfg.full_mem_size / this->cfg.sector_size) * this->cfg.wr_size)) {
        // memory needed to fit wl_state_t + pos updates for all sectors in partition exceedes 1 sector
        this->state_size = ((sizeof(wl_state_t) + (this->cfg.full_mem_size / this->cfg.sector_size) * this->cfg.wr_size) + this->cfg.sector_size - 1) / this->cfg.sector_size;
        this->state_size = this->state_size * this->cfg.sector_size;
    }

    this->cfg_size = (sizeof(wl_config_t) + this->cfg.sector_size - 1) / this->cfg.sector_size;
    this->cfg_size = this->cfg_size * this->cfg.sector_size;

    // wl_state_t at the end of memory in two copies
    this->addr_state1 = this->cfg.start_addr + this->cfg.full_mem_size - this->state_size * 2 - this->cfg_size;
    this->addr_state2 = this->cfg.start_addr + this->cfg.full_mem_size - this->state_size * 1 - this->cfg_size;

    ESP_LOGD(TAG, "will read state from %p", this->addr_state1);
    result = this->flash_drv->read(this->addr_state1, &this->state, sizeof(wl_state_t));
    ESP_LOGD(TAG, "state read returned %p (%s)", result, esp_err_to_name(result));

    result = this->checkStateCRC(&this->state);
    if (result != ESP_OK) {
        return ESP_ERR_INVALID_CRC;
    }

    ESP_LOGD(TAG, "%s - config ID=%i, stored ID=%i, access_count=%i, block_size=%i, max_count=%i, pos=%i, move_count=0x%8.8X",
             __func__,
             this->cfg.version,
             this->state.version,
             this->state.access_count,
             this->state.block_size,
             this->state.max_count,
             this->state.pos,
             this->state.move_count);

    this->temp_buff = (uint8_t *) malloc(this->cfg.temp_buff_size);
    if (this->temp_buff == NULL) {
        result = ESP_ERR_NO_MEM;
    }
    WL_RESULT_CHECK(result);

    result = this->recoverPos();
    if (result != ESP_OK) {
        // non OK code returned by flash_drv->read(), don't know which one, override with own code
        return ESP_ERR_FLASH_OP_FAIL;
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
    int retval = snprintf(s, n,
        "{\"pos\":\"0x%x\",\"max_pos\":\"0x%x\",\"move_count\":\"0x%x\",\
\"access_count\":\"0x%x\",\"max_count\":\"0x%x\",\"block_size\":\"0x%x\",\
\"version\":\"0x%x\",\"device_id\":\"0x%x\",\"crc\":\"0x%x\"}",
        this->state.pos, this->state.max_pos, this->state.move_count, this->state.access_count,
        this->state.max_count, this->state.block_size, this->state.version, this->state.device_id, this->state.crc);

    return retval;
}

esp_err_t WLmon_Flash::write_wl_status_json(char *s, size_t n)
{
    int retval, max_len = n;

    retval = snprintf(s, n, "{\"config\":");
    if (retval >= 0 && retval < n) {
        // OK
        s += retval;
        n -= retval;
    } else {
        return ESP_FAIL;
    }

    retval = write_wl_config_json(s, n);
    if (retval >= 0 && retval < n) {
        // OK
        s += retval;
        n -= retval;
    } else {
        return ESP_FAIL;
    }

    retval = snprintf(s, n, ",\"state\":");
    if (retval >= 0 && retval < n) {
        // OK
        s += retval;
        n -= retval;
    } else {
        return ESP_FAIL;
    }

    retval = write_wl_state_json(s, n);
    if (retval >= 0 && retval < n) {
        // OK
        s += retval;
        n -= retval;
    } else {
        return ESP_FAIL;
    }

    // TODO dummy addr in static analysis does not make much sense as it is written in updateWL()?

    retval = snprintf(s, n, "}\n");
    if (retval >= 0 && retval < n) {
        // OK
        s += retval;
        n -= retval;
    } else {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "%s: written JSON of length %d (0x%x)", __func__, max_len-n, max_len-n);

    return ESP_OK;
}
