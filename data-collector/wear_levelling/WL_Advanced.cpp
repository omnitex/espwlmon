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
    // erase counts will be saved in 16B (as the smallest write size for encrypted partition) structures like so
    // | sector number 2B | erase count 2B | sn 2B | ec 2B | sn 2B | ec 2B | crc (of all previous fields) 4B |
    // count how many sector triplets will be saved + possible 1 or 2 sectors forming additional record; all groups will have 16B record
    // !!n, where n != 0, equals 1; so 1 or 2 additional sectors will add a single record
    // size in bytes, not aligned in any way
    uint32_t erase_count_records_size = (sector_count / 3 + !!(sector_count % 3)) * sizeof(wl_erase_count_t);
    ESP_LOGD(TAG, "%s: erase_count_records_size %u", __func__, erase_count_records_size);

    // count how many sectors will be needed to keep all records
    uint32_t erase_count_sectors = (erase_count_records_size + this->cfg.sector_size - 1) / this->cfg.sector_size;

    ESP_LOGD(TAG, "%s: require %u sectors for erase count records", __func__, erase_count_sectors);

    // size aligned to sectors, in bytes
    this->erase_count_records_size = erase_count_sectors * this->cfg.sector_size;

    // allocate sectors for keeping erase count records, in two copies
    this->flash_size = this->flash_size - 2 * this->erase_count_records_size;
    // and save their addresses
    this->addr_erase_counts1 = this->addr_state1 - 2 * this->erase_count_records_size;
    this->addr_erase_counts2 = this->addr_state1 - this->erase_count_records_size;

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

    // load existing erase counts to buffer
    this->readEraseCounts();

    return ESP_OK;
}

void WL_Advanced::fillOkBuff(int n)
{
    int sector = n;
    ESP_LOGD(TAG, "%s: begin - sector=%i", __func__, sector);

    wl_sector_erase_record_t *record_buff = (wl_sector_erase_record_t *)this->temp_buff;

    size_t physical_sector = this->calcAddr(sector * this->cfg.sector_size) / this->cfg.sector_size;
    ESP_LOGD(TAG, "%s: sector %u maps to %u", __func__, sector, physical_sector);

    record_buff->device_id = this->state.device_id;
    record_buff->pos = this->state.pos;
    record_buff->sector = physical_sector;
    record_buff->crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)record_buff, offsetof(wl_sector_erase_record_t, crc));

    ESP_LOGD(TAG, "%s: device_id=%u, pos=%u, sector=%u, crc=%u", __func__, record_buff->device_id, record_buff->pos, record_buff->sector, record_buff->crc);
}

bool WL_Advanced::OkBuffSet(int n)
{
    int pos = n;

    wl_sector_erase_record_t *record_buff = (wl_sector_erase_record_t *)this->temp_buff;

    if (record_buff->device_id != this->state.device_id)
        return false;

    if (record_buff->pos != pos)
        return false;

    // record_buff->sector is the information gained from record, so it's not to be checked

    if (record_buff->crc != crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)record_buff, offsetof(wl_sector_erase_record_t, crc)))
        return false;

    ESP_LOGV(TAG, "%s: buffer OK at pos %i", __func__, pos);

    return true;
}

esp_err_t WL_Advanced::updateEraseCounts()
{
    ESP_LOGD(TAG, "%s: begin", __func__);

    esp_err_t result = ESP_OK;
    wl_sector_erase_record_t *record_buff = (wl_sector_erase_record_t *)this->temp_buff;

    // go through all pos update records and tally up erase counts to buffer, incrementing existing counts
    for (uint32_t i = 0; i < this->state.max_pos; i++) {
        result = this->flash_drv->read(this->addr_state1 + sizeof(wl_state_t) + i * this->cfg.wr_size, this->temp_buff, this->cfg.wr_size);
        WL_RESULT_CHECK(result);

        if (this->OkBuffSet(i)) {
            // increment erase count, indexing by sector number
            this->erase_count_buffer[record_buff->sector]++;
        } else {
            ESP_LOGD(TAG, "%s: found pos at %i", __func__, i);
            break;
        }
    }

    wl_erase_count_t *erase_count_buff = (wl_erase_count_t *)this->temp_buff;
    // for indexing (sector, erase count) pair in the array of 3 in wl_erase_count_t
    uint8_t pair_index = 0;
    // for indexing written wl_erase_count_t structures to flash
    uint32_t erase_count_index = 0;

    // erase sector(s) for storing first copy of updated erase counts
    // TODO erase has to be done by sectors
    result = this->flash_drv->erase_range(this->addr_erase_counts1, this->erase_count_records_size);
    WL_RESULT_CHECK(result);

    // save non zero erase counts to flash; here i == sector number as incrementing has been done in this manner
    for (uint32_t i = 0; i < this->state.max_pos; i++) {
        uint32_t sector = i;
        if (this->erase_count_buffer[sector] != 0) {
            ESP_LOGD(TAG, "%s: non zero erase count of sector %u => %u", __func__, sector, erase_count_buffer[sector]);

            // clear bufffer for assembling new triplet
            if (pair_index == 0) {
                memset(this->temp_buff, 0, this->cfg.temp_buff_size);
            }

            // save a pair of values to buffer
            erase_count_buff->pairs[pair_index].sector = sector;
            erase_count_buff->pairs[pair_index].erase_count = this->erase_count_buffer[sector];
            pair_index++;

            // if a triplet is assembled OR there won't be a full one
            if (pair_index >= 3) {
                ESP_LOGD(TAG, "%s: triplet ready (sector %u out of %u) for write at index %u", __func__, sector, this->state.max_pos, erase_count_index);
                erase_count_buff->crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)erase_count_buff, offsetof(wl_erase_count_t, crc));
                // write triplet with CRC to flash
                result = this->flash_drv->write(this->addr_erase_counts1 + erase_count_index * sizeof(wl_erase_count_t), erase_count_buff, sizeof(wl_erase_count_t));
                WL_RESULT_CHECK(result);
                // for next triplet, move the index to next position
                erase_count_index++;
                // and reset indexing for next triplet
                pair_index = 0;
                ESP_LOGD(TAG, "%s: triplet written", __func__);
            }

        }
    }

    // if an incomplete pair is formed, write it also
    if (pair_index > 0) {
        ESP_LOGD(TAG, "%s: incomplete triplet for write at index %u", __func__, erase_count_index);
        erase_count_buff->crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)erase_count_buff, offsetof(wl_erase_count_t, crc));
        result = this->flash_drv->write(this->addr_erase_counts1 + erase_count_index * sizeof(wl_erase_count_t), erase_count_buff, sizeof(wl_erase_count_t));
        WL_RESULT_CHECK(result);
        ESP_LOGD(TAG, "%s: incomplete triplet written", __func__);
    }

    //TODO second copy?

    ESP_LOGD(TAG, "%s: return", __func__);

    return ESP_OK;
}

esp_err_t WL_Advanced::readEraseCounts()
{
    ESP_LOGD(TAG, "%s: begin", __func__);

    esp_err_t result = ESP_OK;

    wl_erase_count_t *erase_count_buff = (wl_erase_count_t *)this->temp_buff;

    memset(this->erase_count_buffer, 0, this->erase_count_buffer_size);

    //TODO max_pos is not recalculated and can go beyond allocated space?
    // own max_something?

    // go through saved erase counts in flash in this format
    // | sector | erase count | sector | erase count | sector | erase count | crc |
    // and save the counts to buffer, indexing by sector
    for (uint32_t i = 0; i < this->state.max_pos; i++) {
        result = this->flash_drv->read(this->addr_erase_counts1 + i * sizeof(wl_erase_count_t), erase_count_buff, sizeof(wl_erase_count_t));
        WL_RESULT_CHECK(result);

        uint32_t crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)erase_count_buff, offsetof(wl_erase_count_t, crc));
        if (crc != erase_count_buff->crc) {
            // ignore invalid record ?
            continue;
        }

        // CRC of erase count record OK, save read erase counts to buffer
        for (int i = 0; i < 3; i++) {
            // only non-zero erase counts are valid records, as there's no need to record no erases for a sector
            if (erase_count_buff->pairs[i].erase_count != 0) {
                this->erase_count_buffer[erase_count_buff->pairs[i].sector] = erase_count_buff->pairs[i].erase_count;
            }
        }

        ESP_LOGD(TAG, "%s: read erase counts: %u->%u, %u->%u, %u->%u", __func__,
            erase_count_buff->pairs[0].sector, erase_count_buff->pairs[0].erase_count,
            erase_count_buff->pairs[1].sector, erase_count_buff->pairs[1].erase_count,
            erase_count_buff->pairs[2].sector, erase_count_buff->pairs[2].erase_count);
    }

    ESP_LOGD(TAG, "%s: return", __func__);
    return result;
}

esp_err_t WL_Advanced::updateWL(size_t sector)
{
    ESP_LOGD(TAG, "%s: begin - sector %lu", __func__, sector);

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

//TODO cycle count also in here?
esp_err_t WL_Advanced::initSections()
{
    ESP_LOGD(TAG, "%s: begin", __func__);
    esp_err_t result = ESP_OK;

    result = WL_Flash::initSections();
    WL_RESULT_CHECK(result);

    result = this->flash_drv->erase_range(this->addr_erase_counts1, this->erase_count_records_size);
    WL_RESULT_CHECK(result);
    result = this->flash_drv->erase_range(this->addr_erase_counts2, this->erase_count_records_size);
    WL_RESULT_CHECK(result);

    ESP_LOGD(TAG, "%s: OK", __func__);

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
    // passing pos as a fake sector to be erased
    // dummy sector IS indeed erased by updating with access_count = max_count - 1
    // so after full loop, counting that every sector was erased additionally once
    // is actually a realistic approach
    ESP_LOGD(TAG, "%s - updateWL(%u)", __func__, this->state.pos);
    result = this->updateWL(this->state.pos);

    ESP_LOGD(TAG, "%s - result= 0x%08x, cycle_count=0x%08x, move_count= 0x%08x", __func__, result, this->state.move_count, this->state.cycle_count);
    return result;
}
