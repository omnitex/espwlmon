#include "WL_Advanced.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
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

    ESP_LOGD(TAG, "%s: require %u sectors for erase count records", __func__, erase_count_sectors);

    // allocate sectors for keeping erase count records, in two copies
    this->flash_size = this->flash_size - 2 * erase_count_sectors * this->cfg.sector_size;
    // and save their addresses
    this->addr_erase_counts1 = this->addr_state1 - 2 * erase_count_sectors * this->cfg.sector_size;
    this->addr_erase_counts2 = this->addr_state1 - erase_count_sectors * this->cfg.sector_size;

    ESP_LOGD(TAG, "%s: erase_counts1 at %p, erase_count2 at %p", __func__, this->addr_erase_counts1, this->addr_erase_counts2);

    return ESP_OK;
}

esp_err_t WL_Advanced::init()
{
    ESP_LOGD(TAG, "%s: begin", __func__);

    // base init() needed for making this->state.max_pos accessible
    esp_err_t result = WL_Flash::init();
    if (result != ESP_OK) {
        return result;
    }

    // allocate buffer big enough for 16bit number for each sector
    //TODO max_pos is 1 bigger than number of sectors, use the last 16bits for something?
    this->erase_count_buffer_size = this->state.max_pos * sizeof(uint16_t);
    this->erase_count_buffer = (uint16_t *)malloc(this->erase_count_buffer_size);
    if (this->erase_count_buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "%s: allocated erase_count_buffer OK", __func__);
    return ESP_OK;
}

void WL_Advanced::fillOkBuff(int n)
{
    int sector = n;
    ESP_LOGD(TAG, "%s: begin - sector=%i", __func__, sector);

    uint32_t *buff = (uint32_t *)this->temp_buff;

    buff[0] = this->state.device_id;
    buff[1] = this->state.pos;
    buff[2] = sector;
    // TODO use offsetof() instead? define a struct in WL_Advanced.h?
    buff[3] = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)buff, 3 * sizeof(uint32_t));

    ESP_LOGD(TAG, "%s: device_id=%u, pos=%u, sector=%u, crc=%u", __func__, buff[0], buff[1], buff[2], buff[3]);
}

bool WL_Advanced::OkBuffSet(int n)
{
    int pos = n;
    //ESP_LOGD(TAG, "%s: begin - pos=%i", __func__, pos);

    uint32_t *buff = (uint32_t *)this->temp_buff;

    if (buff[0] != this->state.device_id)
        return false;

    if (buff[1] != pos)
        return false;

    // buff[2] is sector number, that is not deterministic and cannot be checked

    if (buff[3] != crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)buff, 3 * sizeof(uint32_t)))
        return false;

    ESP_LOGD(TAG, "%s: buffer OK at pos %i", __func__, pos);

    return true;
}

esp_err_t WL_Advanced::updateEraseCounts()
{
    ESP_LOGD(TAG, "%s: begin", __func__);

    uint32_t *buff = (uint32_t *)this->temp_buff;

    memset(this->erase_count_buffer, 0, this->erase_count_buffer_size);

    // go through all pos update records and tally up erase counts to buffer
    for (size_t i = 0; i < this->state.max_pos; i++) {
        this->flash_drv->read(this->addr_state1 + sizeof(wl_state_t) + i * this->cfg.wr_size, this->temp_buff, this->cfg.wr_size);
        if (this->OkBuffSet(i)) {
            // increment erase count, indexing by sector
            this->erase_count_buffer[buff[2]]++;
            ESP_LOGD(TAG, "%s: erase count of %u ++", __func__, buff[2]);
        }
    }

    for (size_t i = 0; i < this->state.max_pos; i++) {
        if (this->erase_count_buffer[i] != 0) {
            ESP_LOGD(TAG, "%s: non zero erase count of sector %u => %u", __func__, i, erase_count_buffer[i]);
        }
    }

    //TODO write to flash in 2 copies with CRCs (?)

    return ESP_OK;
}

esp_err_t WL_Advanced::updateWL(size_t sector)
{
    ESP_LOGD(TAG, "%s: begin - sector %lu", __func__, sector);
    //TODO remove
    //this->updateEraseCounts();

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
    this->fillOkBuff(sector);
    // write state to mem. We updating only affected bits
    result |= this->flash_drv->write(this->addr_state1 + sizeof(wl_state_t) + byte_pos, this->temp_buff, this->cfg.wr_size);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "%s - update position 1 result= 0x%08x", __func__, result);
        this->state.access_count = this->state.max_count - 1; // we will update next time
        return result;
    }
    this->fillOkBuff(sector);
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

        this->updateEraseCounts();

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

//TODO if cycle_count will be separate of advanced, is this own impl needed?
esp_err_t WL_Advanced::initSections()
{
    ESP_LOGD(TAG, "%s: begin", __func__);

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
    ESP_LOGD(TAG, "%s: begin", __func__);

    esp_err_t result = ESP_OK;
    if (!this->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGD(TAG, "%s - sector= 0x%08x", __func__, (uint32_t) sector);
    // pass sector to updateWl() so it can use it in pos update record
    result = this->updateWL(sector);
    WL_RESULT_CHECK(result);
    size_t virt_addr = this->calcAddr(sector * this->cfg.sector_size);
    result = this->flash_drv->erase_sector((this->cfg.start_addr + virt_addr) / this->cfg.sector_size);
    WL_RESULT_CHECK(result);
    return result;
}

esp_err_t WL_Advanced::flush()
{
    esp_err_t result = ESP_OK;
    this->state.access_count = this->state.max_count - 1;
    //TODO this will skew erase counts and make a fake record
    // use smt like UINT32_MAX as an invalid value?
    result = this->updateWL(0);
    ESP_LOGD(TAG, "%s - result= 0x%08x, cycle_count=0x%08x, move_count= 0x%08x", __func__, result, this->state.move_count, this->state.cycle_count);
    return result;
}
