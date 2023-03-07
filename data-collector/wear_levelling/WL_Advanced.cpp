#include "WL_Advanced.h"
#include <stdlib.h>
#include "esp_log.h"
#include "crc32.h"

static const char *TAG = "wl_advanced";
#ifndef WL_CFG_CRC_CONST
#define WL_CFG_CRC_CONST UINT32_MAX
#endif // WL_CFG_CRC_CONST

#define WL_RESULT_CHECK(result) \
    if (result != ESP_OK) { \
        ESP_LOGE(TAG,"%s(%d): result = 0x%08x", __FUNCTION__, __LINE__, result); \
        return (result); \
    }

WL_Advanced::WL_Advanced(): WL_Flash()
{
    this->erase_count_buffer = NULL;
}

WL_Advanced::~WL_Advanced()
{
    free(this->erase_count_buffer);
}

// 16bit number + 32bit CRC
#define ERASE_COUNT_RECORD_SIZE (sizeof(uint16_t) + sizeof(uint32_t))

esp_err_t WL_Advanced::config(wl_config_t *cfg, Flash_Access *flash_drv)
{
    esp_err_t result;
    // configure base attributes
    result = WL_Flash::config(cfg, flash_drv);
    if (result != ESP_OK) {
        return result;
    }

    // need to allocate minimum 2 sectors for storing erase counts
    uint32_t flash_size_erase_counts = this->flash_size - 2 * this->cfg.sector_size;
    // now count the sectors which we will have to keep records on
    uint32_t sector_count = flash_size_erase_counts / this->cfg.sector_size;
    // each sector will have a record, count how many sectors will be needed to keep all records
    uint32_t erase_count_sectors = ((sector_count * ERASE_COUNT_RECORD_SIZE) + this->cfg.sector_size - 1) / this->cfg.sector_size;

    ESP_LOGI(TAG, "%s - require %u sectors for erase counts records", __func__, erase_count_sectors);

    // allocate sectors for keeping erase count records, in two copies
    this->flash_size = this->flash_size - 2 * erase_count_sectors;
    // and save their addresses
    this->addr_erase_counts1 = this->addr_state1 - 2 * erase_count_sectors;
    this->addr_erase_counts1 = this->addr_state1 - erase_count_sectors;

    return ESP_OK;
}

esp_err_t WL_Advanced::init()
{
    ESP_LOGI(TAG, "%s: begin", __func__);

    // base init() needed for this->state.max_pos
    esp_err_t result = WL_Flash::init();
    if (result != ESP_OK) {
        return result;
    }

    // allocate buffer big enough for 2B number for each sector
    this->erase_count_buffer = (uint16_t *)malloc(this->state.max_pos * sizeof(uint16_t));
    if (this->erase_count_buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "%s: OK", __func__);
    return ESP_OK;
}

void WL_Advanced::fillOkBuff(int n)
{
    ESP_LOGI(TAG, "%s: begin", __func__);
    //TODO
    WL_Flash::fillOkBuff(n);
}

bool WL_Advanced::OkBuffSet(int n)
{
    ESP_LOGI(TAG, "%s: begin", __func__);
    //TODO
    return WL_Flash::OkBuffSet(n);
}

esp_err_t WL_Advanced::updateEraseCounts()
{
    ESP_LOGI(TAG, "%s: begin", __func__);
    return ESP_OK;
}

esp_err_t WL_Advanced::updateWL(size_t sector)
{
    ESP_LOGI(TAG, "%s: begin - sector %lu", __func__, sector);

    esp_err_t result = ESP_OK;
    this->state.access_count++;
    if (this->state.access_count < this->state.max_count) {
        return result;
    }
    // Here we have to move the block and increase the state
    this->state.access_count = 0;
    ESP_LOGV(TAG, "%s - access_count= 0x%08x, pos= 0x%08x", __func__, this->state.access_count, this->state.pos);
    // copy data to dummy block
    size_t data_addr = this->state.pos + 1; // next block, [pos+1] copy to [pos]
    if (data_addr >= this->state.max_pos) {
        data_addr = 0;
    }
    data_addr = this->cfg.start_addr + data_addr * this->cfg.page_size;
    this->dummy_addr = this->cfg.start_addr + this->state.pos * this->cfg.page_size;
    result = this->flash_drv->erase_range(this->dummy_addr, this->cfg.page_size);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "%s - erase wl dummy sector result= 0x%08x", __func__, result);
        this->state.access_count = this->state.max_count - 1; // we will update next time
        return result;
    }

    size_t copy_count = this->cfg.page_size / this->cfg.temp_buff_size;
    for (size_t i = 0; i < copy_count; i++) {
        result = this->flash_drv->read(data_addr + i * this->cfg.temp_buff_size, this->temp_buff, this->cfg.temp_buff_size);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "%s - not possible to read buffer, will try next time, result= 0x%08x", __func__, result);
            this->state.access_count = this->state.max_count - 1; // we will update next time
            return result;
        }
        result = this->flash_drv->write(this->dummy_addr + i * this->cfg.temp_buff_size, this->temp_buff, this->cfg.temp_buff_size);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "%s - not possible to write buffer, will try next time, result= 0x%08x", __func__, result);
            this->state.access_count = this->state.max_count - 1; // we will update next time
            return result;
        }
    }
    // done... block moved.
    // Here we will update structures...
    // Update bits and save to flash:
    uint32_t byte_pos = this->state.pos * this->cfg.wr_size;
    this->fillOkBuff(this->state.pos);
    // write state to mem. We updating only affected bits
    result |= this->flash_drv->write(this->addr_state1 + sizeof(wl_state_t) + byte_pos, this->temp_buff, this->cfg.wr_size);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "%s - update position 1 result= 0x%08x", __func__, result);
        this->state.access_count = this->state.max_count - 1; // we will update next time
        return result;
    }
    this->fillOkBuff(this->state.pos);
    result |= this->flash_drv->write(this->addr_state2 + sizeof(wl_state_t) + byte_pos, this->temp_buff, this->cfg.wr_size);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "%s - update position 2 result= 0x%08x", __func__, result);
        this->state.access_count = this->state.max_count - 1; // we will update next time
        return result;
    }

    this->state.pos++;
    if (this->state.pos >= this->state.max_pos) {
        this->state.pos = 0;
        // one loop more
        this->state.move_count++;
        if (this->state.move_count >= (this->state.max_pos - 1)) {
            this->state.move_count = 0;
            // full cycle achieved; as both pos and move_count are zeroed,
            // keep the information about passed cycle by incrementing cycle_count
            // this will NEVER be zeroed, thus allowing approximate calculation of total number of erases
            // and from that an estimate of sector wear-out
            this->state.cycle_count++;
        }
        // write main state
        this->state.crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)&this->state, WL_STATE_CRC_LEN_V2);

        result = this->flash_drv->erase_range(this->addr_state1, this->state_size);
        WL_RESULT_CHECK(result);
        result = this->flash_drv->write(this->addr_state1, &this->state, sizeof(wl_state_t));
        WL_RESULT_CHECK(result);
        result = this->flash_drv->erase_range(this->addr_state2, this->state_size);
        WL_RESULT_CHECK(result);
        result = this->flash_drv->write(this->addr_state2, &this->state, sizeof(wl_state_t));
        WL_RESULT_CHECK(result);
        ESP_LOGD(TAG, "%s - cycle_count= 0x%08x, move_count= 0x%08x, pos= 0x%08x, ", __func__, this->state.cycle_count, this->state.move_count, this->state.pos);
    }
    // Save structures to the flash... and check result
    if (result == ESP_OK) {
        ESP_LOGV(TAG, "%s - result= 0x%08x", __func__, result);
    } else {
        ESP_LOGE(TAG, "%s - result= 0x%08x", __func__, result);
    }
    return result;
}

esp_err_t WL_Advanced::recoverPos()
{
    ESP_LOGI(TAG, "%s: begin", __func__);
    //TODO
    return WL_Flash::recoverPos();
}

esp_err_t WL_Advanced::initSections()
{
    ESP_LOGI(TAG, "%s: begin", __func__);

    esp_err_t result = ESP_OK;
    this->state.pos = 0;
    this->state.access_count = 0;
    this->state.move_count = 0;
    // max count
    this->state.max_count = this->flash_size / this->state_size * this->cfg.updaterate;
    if (this->cfg.updaterate != 0) {
        this->state.max_count = this->cfg.updaterate;
    }
    this->state.version = this->cfg.version;
    this->state.block_size = this->cfg.page_size;
    this->state.device_id = esp_random();
    this->state.cycle_count = 0;
    memset(this->state.reserved, 0, sizeof(this->state.reserved));

    this->state.max_pos = 1 + this->flash_size / this->cfg.page_size;

    this->state.crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)&this->state, WL_STATE_CRC_LEN_V2);

    result = this->flash_drv->erase_range(this->addr_state1, this->state_size);
    WL_RESULT_CHECK(result);
    result = this->flash_drv->write(this->addr_state1, &this->state, sizeof(wl_state_t));
    WL_RESULT_CHECK(result);
    // write state copy
    result = this->flash_drv->erase_range(this->addr_state2, this->state_size);
    WL_RESULT_CHECK(result);
    result = this->flash_drv->write(this->addr_state2, &this->state, sizeof(wl_state_t));
    WL_RESULT_CHECK(result);

    result = this->flash_drv->erase_range(this->addr_cfg, this->cfg_size);
    WL_RESULT_CHECK(result);
    result = this->flash_drv->write(this->addr_cfg, &this->cfg, sizeof(wl_config_t));
    WL_RESULT_CHECK(result);

    ESP_LOGD(TAG, "%s - this->state->max_count= 0x%08x, this->state->max_pos= 0x%08x", __func__, this->state.max_count, this->state.max_pos);
    ESP_LOGD(TAG, "%s - result= 0x%08x", __func__, result);
    return result;

}

esp_err_t WL_Advanced::erase_sector(size_t sector)
{
    ESP_LOGI(TAG, "%s: begin", __func__);

    esp_err_t result = ESP_OK;
    if (!this->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGD(TAG, "%s - sector= 0x%08x", __func__, (uint32_t) sector);
    result = this->updateWL(sector);
    WL_RESULT_CHECK(result);
    size_t virt_addr = this->calcAddr(sector * this->cfg.sector_size);
    result = this->flash_drv->erase_sector((this->cfg.start_addr + virt_addr) / this->cfg.sector_size);
    WL_RESULT_CHECK(result);
    return result;
}