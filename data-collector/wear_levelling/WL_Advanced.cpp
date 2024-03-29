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
    // configure attributes, calculates flash_size for base WL
    // also allocated this->temp_buff
    result = WL_Flash::config(cfg, flash_drv);
    if (result != ESP_OK) {
        return result;
    }
    // invalidate base config
    this->configured = false;

    // need to allocate minimum 2 sectors for storing erase counts
    uint32_t flash_size_erase_counts = this->flash_size - 2 * this->cfg.sector_size;
    // now count the sectors which we will have to keep records on
    uint32_t sector_count = flash_size_erase_counts / this->cfg.sector_size;
    // erase counts will be saved in 16B (as the smallest write size for encrypted partition) structures like so
    // | sector number 2B | erase count 2B | sn 2B | ec 2B | sn 2B | ec 2B | crc (of all previous fields) 4B |
    // count how many (sector number,erase count) triplets will be saved + possible 1~2 sectors forming additional record; all groups will have 16B record
    // !!n, where n != 0, equals 1; so 1 or 2 additional sectors will add a single record
    // calculated size in bytes, not aligned in any way
    uint32_t erase_count_records_size = (sector_count / 3 + !!(sector_count % 3)) * sizeof(wl_erase_count_t);
    // count how many sectors will be needed to keep all records
    uint32_t erase_count_sectors = (erase_count_records_size + this->cfg.sector_size - 1) / this->cfg.sector_size;

    ESP_LOGD(TAG, "%s: require %u B => %u sectors for one copy of erase count records", __func__, erase_count_records_size, erase_count_sectors);

    // size aligned to sectors, in bytes
    this->erase_count_records_size = erase_count_sectors * this->cfg.sector_size;

    // allocate sectors for keeping erase count records, in two copies
    this->flash_size = this->flash_size - 2 * this->erase_count_records_size;
    // and save their addresses
    this->addr_erase_counts1 = this->addr_state1 - 2 * this->erase_count_records_size;
    this->addr_erase_counts2 = this->addr_state1 - this->erase_count_records_size;

    ESP_LOGD(TAG, "%s: new flash_size=0x%x, addr_erase_counts1=0x%x, addr_erase_counts2=0x%x",
             __func__, this->flash_size, this->addr_erase_counts1, this->addr_erase_counts2);

    this->configured = true;
    return ESP_OK;
}

esp_err_t WL_Advanced::init()
{
    esp_err_t result = ESP_OK;

    if (this->configured == false) {
        ESP_LOGW(TAG, "%s: not configured, call config() first", __func__);
        return ESP_ERR_INVALID_STATE;
    }

    this->initialized = false;

    // calculate bits needed for sector addressing and in turn for Feistel network
    // feistel_bit_width = log2(sector_count)
    uint32_t sector_count = this->flash_size / this->cfg.sector_size;
    for (this->feistel_bit_width = 0; sector_count; this->feistel_bit_width++) {
        sector_count >>= 1;
    }

    // we are using 8 bit keys in Feistel network, where each key is XORed with half of sector address
    // so if sector address has more than 16 bits, it cannot be split to two parts where each would be at max 8 bits long
    // => such long sector addresses (for partition with more than 2^16 sectors) are currently not supported
    // sidenote: extending key bit length is presumably possible, currently using only 3*8bits (for simplicity) out of 32bits randomly generated and saved in feistel_keys
    if (this->feistel_bit_width > 16) {
        ESP_LOGE(TAG, "%s: Wear leveled partition contains too many sectors for WL_Advanced!", __func__);
        ESP_LOGE(TAG, "%s: Sector address cannot be properly randomized in current implementation of Feistel network.", __func__);
        return ESP_ERR_NOT_SUPPORTED;
    }

    /*
    * split bit width to | msb | lsb |
    * if bit width not even, make lsb 1 longer (e.g. | 3 bits | 4bits |)
    * lsb = ceil(feistel_bit_width / 2)
    * msb = feistel_bit_width - lsb
    * For usage of calculated bit widths see addressFeistelNetwork()
    */
    this->feistel_lsb_width = (this->feistel_bit_width + 1) / 2;
    this->feistel_msb_width = this->feistel_bit_width - this->feistel_lsb_width;

    ESP_LOGD(TAG, "%s: feistel_bit_width=%u, msb=%u, lsb=%u", __func__, this->feistel_bit_width, this->feistel_msb_width, this->feistel_lsb_width);

    wl_advanced_state_t *state_main = (wl_advanced_state_t *)&this->state;
    wl_advanced_state_t _state_copy;
    wl_advanced_state_t *state_copy = &_state_copy;

    result = this->flash_drv->read(this->addr_state1, state_main, sizeof(wl_advanced_state_t));
    result |= this->flash_drv->read(this->addr_state2, state_copy, sizeof(wl_advanced_state_t));
    WL_RESULT_CHECK(result);

    uint32_t crc1 = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)state_main, offsetof(wl_advanced_state_t, crc));
    uint32_t crc2 = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)state_copy, offsetof(wl_advanced_state_t, crc));

    uint8_t *keys = (uint8_t *)&state_main->feistel_keys;
    ESP_LOGD(TAG, "%s: max_count=%i, move_count=0x%x, cycle_count=0x%x, Feistel keys=(%u,%u,%u)",
             __func__,
             state_main->max_count,
             state_main->move_count,
             state_main->cycle_count,
             keys[0],
             keys[1],
             keys[2]);

    if ((crc1 == state_main->crc) && (crc2 == state_copy->crc)) {
        // states are individually valid
        if (crc1 != crc2) {
            // second copy was not updated, rewrite it based on main
            result = this->flash_drv->erase_range(this->addr_state2, this->state_size);
            WL_RESULT_CHECK(result);
            result = this->flash_drv->write(this->addr_state2, state_main, sizeof(wl_advanced_state_t));
            WL_RESULT_CHECK(result);

            // copy pos update records as well
            for (uint32_t i = 0; i < state_main->max_pos; i++) {
                // read pos update record
                result = this->flash_drv->read(this->addr_state1 + sizeof(wl_advanced_state_t) + i * this->cfg.wr_size, this->temp_buff, this->cfg.wr_size);
                WL_RESULT_CHECK(result);
                // check it is valid
                if (this->OkBuffSet(i) == true) {
                    // if so, write it as second copy
                    result = this->flash_drv->write(this->addr_state2 + sizeof(wl_advanced_state_t) + i * this->cfg.wr_size, this->temp_buff, this->cfg.wr_size);
                    WL_RESULT_CHECK(result);
                }
            }
        }

        ESP_LOGV(TAG, "%s: both state CRC checks OK", __func__);
        // here both copies of states and records should be valid
        result = this->recoverPos();
        WL_RESULT_CHECK(result);
    } else if ((crc1 != state_main->crc) && (crc2 != state_copy->crc)) {
        // both CRCs invalid => new instance of WL
        result = this->initSections();
        WL_RESULT_CHECK(result);
    } else {
        // recover broken state (one CRC invalid)
        if (crc1 == state_main->crc) {
            // state main valid, rewrite copy
            this->flash_drv->erase_range(this->addr_state2, this->state_size);
            WL_RESULT_CHECK(result);
            this->flash_drv->write(this->addr_state2, state_main, sizeof(wl_advanced_state_t));
            WL_RESULT_CHECK(result);

            for (uint32_t i = 0; i < state_main->max_pos; i++) {
                // read pos update record
                result = this->flash_drv->read(this->addr_state1 + sizeof(wl_advanced_state_t) + i * this->cfg.wr_size, this->temp_buff, this->cfg.wr_size);
                WL_RESULT_CHECK(result);
                // check it is valid
                if (this->OkBuffSet(i) == true) {
                    // if so, write it as second copy
                    result = this->flash_drv->write(this->addr_state2 + sizeof(wl_advanced_state_t) + i * this->cfg.wr_size, this->temp_buff, this->cfg.wr_size);
                    WL_RESULT_CHECK(result);
                }
            }
        } else {
            // last case of only copy being valid => rewrite state main
            this->flash_drv->erase_range(this->addr_state1, this->state_size);
            WL_RESULT_CHECK(result);
            this->flash_drv->write(this->addr_state1, state_copy, sizeof(wl_advanced_state_t));
            WL_RESULT_CHECK(result);

            for (uint32_t i = 0; i < state_copy->max_pos; i++) {
                // read pos update record
                result = this->flash_drv->read(this->addr_state2 + sizeof(wl_advanced_state_t) + i * this->cfg.wr_size, this->temp_buff, this->cfg.wr_size);
                WL_RESULT_CHECK(result);
                // check it is valid
                if (this->OkBuffSet(i) == true) {
                    // if so, write it as second copy
                    result = this->flash_drv->write(this->addr_state1 + sizeof(wl_advanced_state_t) + i * this->cfg.wr_size, this->temp_buff, this->cfg.wr_size);
                    WL_RESULT_CHECK(result);
                }
            }
        }
    }

    ESP_LOGI(TAG, "%s: pos=%u, max_pos=%u", __func__, state_main->pos, state_main->max_pos);

    // allocate buffer to store 2B number for each sector's erase count
    this->erase_count_buffer_size = state_main->max_pos * sizeof(uint16_t);
    this->erase_count_buffer = (uint16_t *)malloc(this->erase_count_buffer_size);
    if (this->erase_count_buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGD(TAG, "%s: allocated erase_count_buffer OK", __func__);

    // load existing erase counts to just allocated buffer
    result = this->readEraseCounts();
    WL_RESULT_CHECK(result);

    this->initialized = true;
    return ESP_OK;
}

esp_err_t WL_Advanced::initSections()
{
    esp_err_t result = ESP_OK;
    wl_advanced_state_t *advanced_state = (wl_advanced_state_t *)&this->state;
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

    advanced_state->cycle_count = 0;

    // will use only 3B for 3 stage Feistel network, each stage requires an 8bit (1B) key
    // generating full 32bit (4B) random value is convenient, no reason to mask out the not used byte
    // for usage of keys see addressFeistelNetwork()
    advanced_state->feistel_keys = esp_random();

    memset(advanced_state->reserved, 0, sizeof(advanced_state->reserved));

    this->state.max_pos = 1 + this->flash_size / this->cfg.page_size;

    this->state.crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)&this->state, offsetof(wl_advanced_state_t, crc));

    // states main state
    result = this->flash_drv->erase_range(this->addr_state1, this->state_size);
    WL_RESULT_CHECK(result);
    result = this->flash_drv->write(this->addr_state1, &this->state, sizeof(wl_state_t));
    WL_RESULT_CHECK(result);
    // write state copy
    result = this->flash_drv->erase_range(this->addr_state2, this->state_size);
    WL_RESULT_CHECK(result);
    result = this->flash_drv->write(this->addr_state2, &this->state, sizeof(wl_state_t));
    WL_RESULT_CHECK(result);

    // write config
    result = this->flash_drv->erase_range(this->addr_cfg, this->cfg_size);
    WL_RESULT_CHECK(result);
    result = this->flash_drv->write(this->addr_cfg, &this->cfg, sizeof(wl_config_t));
    WL_RESULT_CHECK(result);

    // prepare sectors for erase counts in two copies
    result = this->flash_drv->erase_range(this->addr_erase_counts1, this->erase_count_records_size);
    WL_RESULT_CHECK(result);
    result = this->flash_drv->erase_range(this->addr_erase_counts2, this->erase_count_records_size);
    WL_RESULT_CHECK(result);

    uint8_t *keys = (uint8_t *)&advanced_state->feistel_keys;
    ESP_LOGD(TAG, "%s: generated Feistel keys (%u, %u, %u) for bit with %u", __func__, keys[0], keys[1], keys[2], this->feistel_bit_width);

    return result;
}


/*
 * Go through pos update records until the first with invalid CRC.
 * Assume that is the current position of dummy block
 * (pos points to 'after' last pos update record, see updateWl() pos increment).
 *
 * (own recoverPos() needed to call WL_Advanced::OkBuffSet() as it implements different logic)
 */
esp_err_t WL_Advanced::recoverPos()
{
    esp_err_t result = ESP_OK;

    uint32_t position = 0;

    for (uint32_t i = 0; i < this->state.max_pos; i++) {
        position = i;
        result = this->flash_drv->read(this->addr_state1 + sizeof(wl_advanced_state_t) + i * this->cfg.wr_size, this->temp_buff, this->cfg.wr_size);
        WL_RESULT_CHECK(result);
        if (this->OkBuffSet(i) == false) {
            // found invalid record => found position
            break;
        }
    }

    this->state.pos = position;
    if (this->state.pos == this->state.max_pos) {
        this->state.pos--;
    }

    ESP_LOGV(TAG, "%s: recovered %u", __func__, this->state.pos);

    return result;
}

/*
 * Fills temp_buff with a pos update record:

 * |--------------------------------|
 * | device_id | pos | sector | crc |
 * |--------------------------------|
 *
 * Sector number that is saved is the resulting mapped physical sector from calcAddr()
 * (the same that would be actually accessed by read/write/erase)
 */
void WL_Advanced::fillOkBuff(int sector)
{
    wl_sector_erase_record_t *record_buff = (wl_sector_erase_record_t *)this->temp_buff;

    size_t physical_sector = this->calcAddr(sector * this->cfg.sector_size) / this->cfg.sector_size;
    ESP_LOGV(TAG, "%s: sector %u maps to %u", __func__, sector, physical_sector);

    record_buff->device_id = this->state.device_id;
    record_buff->pos = this->state.pos;
    record_buff->sector = physical_sector;
    record_buff->crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)record_buff, offsetof(wl_sector_erase_record_t, crc));

    ESP_LOGD(TAG, "%s: device_id=%u, pos=%u, sector=%u, crc=%u", __func__, record_buff->device_id, record_buff->pos, record_buff->sector, record_buff->crc);
}


/*
 * Checks temp_buff is filled with correct device_id and given pos, while also having correct CRC.
 */
bool WL_Advanced::OkBuffSet(int pos)
{
    wl_sector_erase_record_t *record_buff = (wl_sector_erase_record_t *)this->temp_buff;

    if (record_buff->device_id != this->state.device_id) {
        return false;
    }

    if (record_buff->pos != pos) {
        return false;
    }

    // record_buff->sector is the information gained from record, so it's not to be checked

    if (record_buff->crc != crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)record_buff, offsetof(wl_sector_erase_record_t, crc))) {
        return false;
    }

    return true;
}

/*
 * Write per sector erase counts FROM BUFFER TO FLASH, transforming the structure and format in between
 *
 * Will go through buffer where index == sector_number and buffer[sector_number] == erase_count
 * and assemble 16 B records with 3 pairs of (sector number, erase count) + CRC and write them to flash
 * See readEraseCounts() for description of the format, as this is the reverse operation
 *
 * Writing sparsely => only non-zero erase counts
 * Zero erase count is then a special 'no value' marker
 *
 * For an incomplete triplet - 1~2 remaining pairs of (sector number, erase count) - fill the rest of record with 0
 * => as mentioned above, 0 erase count marks 'no value' record, used in readEraseCounts()
 */
esp_err_t WL_Advanced::writeEraseCounts(size_t erase_counts_addr)
{
    if (erase_counts_addr != this->addr_erase_counts1 && erase_counts_addr != this->addr_erase_counts2) {
        ESP_LOGE(TAG, "%s: erase counts address 0x%x is invalid", __func__, erase_counts_addr);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;

    wl_erase_count_t *erase_count_buff = (wl_erase_count_t *)this->temp_buff;
    // for indexing (sector, erase count) pair in the array of 3 in wl_erase_count_t
    uint8_t pair_index = 0;
    // for indexing written wl_erase_count_t structures to flash
    uint32_t erase_count_index = 0;

    // erase sector(s) for storing first copy of updated erase counts
    result = this->flash_drv->erase_range(erase_counts_addr, this->erase_count_records_size);
    WL_RESULT_CHECK(result);

    // save non zero erase counts to flash; here i == sector number as incrementing has been done in this manner
    for (uint32_t i = 0; i < this->state.max_pos; i++) {
        uint32_t sector = i;
        if (this->erase_count_buffer[sector] != 0) {
            ESP_LOGV(TAG, "%s: non zero erase count of sector %u => %u", __func__, sector, erase_count_buffer[sector]);

            // clear buffer for assembling new triplet
            if (pair_index == 0) {
                memset(this->temp_buff, 0, this->cfg.temp_buff_size);
            }

            // save a pair of values to buffer
            erase_count_buff->pairs[pair_index].sector = sector;
            erase_count_buff->pairs[pair_index].erase_count = this->erase_count_buffer[sector];
            pair_index++;

            // if a triplet is assembled
            if (pair_index >= 3) {
                ESP_LOGV(TAG, "%s: triplet ready (sector %u out of %u) for write at index %u", __func__, sector, this->state.max_pos, erase_count_index);
                erase_count_buff->crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)erase_count_buff, offsetof(wl_erase_count_t, crc));
                // write triplet with CRC to flash
                result = this->flash_drv->write(erase_counts_addr + erase_count_index * sizeof(wl_erase_count_t), erase_count_buff, sizeof(wl_erase_count_t));
                WL_RESULT_CHECK(result);
                // for next triplet, move the index to next position
                erase_count_index++;
                // and reset indexing for next triplet
                pair_index = 0;
                ESP_LOGV(TAG, "%s: triplet written", __func__);
            }

        }
    }

    // if an incomplete pair is formed, write it also
    if (pair_index > 0) {
        ESP_LOGV(TAG, "%s: incomplete triplet for write at index %u", __func__, erase_count_index);
        erase_count_buff->crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)erase_count_buff, offsetof(wl_erase_count_t, crc));
        result = this->flash_drv->write(this->addr_erase_counts1 + erase_count_index * sizeof(wl_erase_count_t), erase_count_buff, sizeof(wl_erase_count_t));
        WL_RESULT_CHECK(result);
        ESP_LOGV(TAG, "%s: incomplete triplet written", __func__);
    }

    return ESP_OK;
}

/*
 * Update erase counts IN BUFFER with info about erases from pos update records
 *
 * Goes through pos update records similarly to recoverPos() but seeks the sector number in each record
 *
 * For every valid pos update record, increment the erase count for given sector in the buffer
 *      buffer[sector number]++
 *
 * Given that before calling this method the buffer contained erase counts as recovered by readEraseCounts(),
 * the buffer now contains the most up-to-date info about all-time per sector erase counts, ready for writeEraseCounts()
 *
 * As a pos update record is made only every updaterate erases, one occurrence of given sector in a record approximates
 * to updaterate erases of that sector. And by incrementing the erase count in buffer only once for each record,
 * we keep the erase count 'updaterate times' smaller. This allows for a smaller numeric type to be used, as we need to
 * save PER SECTOR erase counts, where every byte counts.
 *
 * This results in having to multiply the value in buffer by updaterate to get the real approximate of erase count for that sector.
 */
esp_err_t WL_Advanced::updateEraseCounts()
{
    esp_err_t result = ESP_OK;
    wl_sector_erase_record_t *record_buff = (wl_sector_erase_record_t *)this->temp_buff;

    // go through all pos update records and tally up erase counts to buffer, incrementing existing counts
    for (uint32_t i = 0; i < this->state.max_pos; i++) {
        result = this->flash_drv->read(this->addr_state1 + sizeof(wl_state_t) + i * sizeof(wl_sector_erase_record_t), record_buff, sizeof(wl_sector_erase_record_t));
        WL_RESULT_CHECK(result);

        if (this->OkBuffSet(i)) {
            // increment erase count, indexing by sector number
            this->erase_count_buffer[record_buff->sector]++;
            ESP_LOGV(TAG, "%s: buffer OK at pos %u, sector [%u]++ => %u", __func__, i, record_buff->sector, this->erase_count_buffer[record_buff->sector]);
        } else {
            ESP_LOGD(TAG, "%s: found pos at %i", __func__, i);
            break;
        }
    }

    return result;
}

/*
 * Erase counts IN FLASH are saved in reserved sectors (in two copies), with the following format:
 *
 * | sector number 2B | erase count 2B | sn 2B | ec 2B | sn 2B | ec 2B | crc (of all previous fields) 4B | => wl_erase_count_t (16 B)
 * | sector number 2B | erase count 2B | sn 2B | ec 2B | sn 2B | ec 2B | crc (of all previous fields) 4B |
 * | sector number 2B | erase count 2B | sn 2B | ec 2B | sn 2B | ec 2B | crc (of all previous fields) 4B |
 *          .                   .           .       .       .       .                   .
 *          .                   .           .       .       .       .                   .
 *          .                   .           .       .       .       .                   .
 * Sector number and its erase count form a pair. Three such pairs form a triplet.
 * With CRC of the triplet it forms wl_erase_count_t record, which is 16 B in total.
 *
 * Go through the records, read erase counts and write them to previously allocated uint16_t BUFFER like so:
 *      buffer[sector number] = erase count
 *
 * Erase counts handled here are updaterate smaller then real ones - see updateEraseCounts()
 *
 * UINT16_MAX * updaterate is for sure greater then single sector erase lifetime, so 2B number for an erase count should be OK
 */
esp_err_t WL_Advanced::readEraseCounts()
{
    esp_err_t result = ESP_OK;

    wl_erase_count_t *erase_count_buff = (wl_erase_count_t *)this->temp_buff;
    wl_advanced_state_t *advanced_state = (wl_advanced_state_t *)&this->state;

    memset(this->erase_count_buffer, 0, this->erase_count_buffer_size);

    if (this->state.move_count == 0 && advanced_state->cycle_count == 0) {
        ESP_LOGD(TAG, "%s: no erase counts in flash yet, as move_count=%u and cycle_count=%u", __func__, this->state.move_count, advanced_state->cycle_count);
        return ESP_OK;
    }

    // go through saved erase counts in flash, checking CRCs with second copy as fallback
    // on CRC OK, save the 3 erase counts from one wl_erase_count_t to buffer, indexing by sector
    for (uint32_t i = 0; i < this->state.max_pos; i++) {
        result = this->flash_drv->read(this->addr_erase_counts1 + i * sizeof(wl_erase_count_t), erase_count_buff, sizeof(wl_erase_count_t));
        WL_RESULT_CHECK(result);

        uint32_t crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)erase_count_buff, offsetof(wl_erase_count_t, crc));

        if (crc != erase_count_buff->crc) {
            ESP_LOGD(TAG, "%s: first copy of erase counts is invalid at pos %u", __func__, i);
            // first copy has invalid CRC, check the second copy
            result = this->flash_drv->read(this->addr_erase_counts2 + i * sizeof(wl_erase_count_t), erase_count_buff, sizeof(wl_erase_count_t));
            WL_RESULT_CHECK(result);

            crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)erase_count_buff, offsetof(wl_erase_count_t, crc));
            if (crc != erase_count_buff->crc) {
                ESP_LOGD(TAG, "%s: second copy also invalid at pos %u => assume the end of erase count records", __func__, i);
                return ESP_OK;
            }
        }

        // CRC of erase count record OK, save read erase counts to buffer
        for (int i = 0; i < 3; i++) {
            // only non-zero erase counts are valid records, as there's no need to record no erases for a sector
            if (erase_count_buff->pairs[i].erase_count != 0) {
                this->erase_count_buffer[erase_count_buff->pairs[i].sector] = erase_count_buff->pairs[i].erase_count;
            }
        }

        ESP_LOGV(TAG, "%s: read erase counts: | %u => %u | %u => %u | %u => %u |", __func__,
                 erase_count_buff->pairs[0].sector, erase_count_buff->pairs[0].erase_count,
                 erase_count_buff->pairs[1].sector, erase_count_buff->pairs[1].erase_count,
                 erase_count_buff->pairs[2].sector, erase_count_buff->pairs[2].erase_count);
    }

    ESP_LOGI(TAG, "%s: loaded erase counts to buffer", __func__);

    return result;
}

/*
 * Every updaterate erases (which call this function) a new pos update record is written to flash
 * and dummy block moves by one position (pos increments)
 * See that we get the sector number causing the update as an argument
 * fillOkBuff(), which assembles the pos update record to be written, puts the sector number in the record
 *
 * This means that from the records in flash we can reconstruct which sectors caused the records to be written
 * And since pos update record is made every updaterate erases, one record => updaterate erases of sector in that record
 *
 * After dummy block gets to the end of the partition, it loops back (pos => 0) and move_count is incremented
 * Also on this loop, info about sector erases from all pos update records is aggregated and written to relevant reserved sector
 * so the records can be erased allowing for a new sequence of pos update records
 */
esp_err_t WL_Advanced::updateWL(size_t sector)
{
    esp_err_t result = ESP_OK;
    this->state.access_count++;
    // max_count == updaterate
    if (this->state.access_count < this->state.max_count) {
        return result;
    }

    // will move dummy sector by one
    this->state.access_count = 0;
    ESP_LOGV(TAG, "%s - sector=0x%x, access_count= 0x%08x, pos= 0x%08x", __func__, sector, this->state.access_count, this->state.pos);

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

    // copy by temp_buff chunks
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
    // done... dummy sector moved

    // will append a new pos update record to flash
    uint32_t byte_pos = this->state.pos * this->cfg.wr_size;

    // assemble a pos update record in temp_buff, using the sector number causing this update
    this->fillOkBuff(sector);

    // write at such offset to append after the last existing pos update record as specified by pos
    result = this->flash_drv->write(this->addr_state1 + sizeof(wl_state_t) + byte_pos, this->temp_buff, this->cfg.wr_size);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "%s - update position 1 result= 0x%08x", __func__, result);
        this->state.access_count = this->state.max_count - 1; // we will update next time
        return result;
    }
    // write a second copy
    this->fillOkBuff(sector);
    result = this->flash_drv->write(this->addr_state2 + sizeof(wl_state_t) + byte_pos, this->temp_buff, this->cfg.wr_size);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "%s - update position 2 result= 0x%08x", __func__, result);
        this->state.access_count = this->state.max_count - 1; // we will update next time
        return result;
    }

    wl_advanced_state_t *advanced_state = (wl_advanced_state_t *)&this->state;

    // only after both pos update record written, consider pos moved
    this->state.pos++;
    if (this->state.pos >= this->state.max_pos) {
        // one loop of dummy sector through partition finished
        this->state.pos = 0;
        // incrementing move count shifts mapping, see calcAddr()
        this->state.move_count++;
        if (this->state.move_count >= (this->state.max_pos - 1)) {
            this->state.move_count = 0;
            // full cycle achieved; as both pos and move_count are zeroed,
            // keep the information about passed cycle by incrementing cycle_count
            // this will NEVER be zeroed, thus allowing approximate calculation of total number of erases
            // and from that an estimate of sector wear-out
            advanced_state->cycle_count++;
        }
        // write main state
        this->state.crc = crc32::crc32_le(WL_CFG_CRC_CONST, (uint8_t *)&this->state, WL_STATE_CRC_LEN_V2);

        // tally up (aggregate) overall per sector erase counts
        result = this->updateEraseCounts();
        WL_RESULT_CHECK(result);
        // and write updated values to flash
        result = this->writeEraseCounts(this->addr_erase_counts1);
        WL_RESULT_CHECK(result);
        // also a second copy
        result = this->writeEraseCounts(this->addr_erase_counts2);
        WL_RESULT_CHECK(result);

        result = this->flash_drv->erase_range(this->addr_state1, this->state_size);
        WL_RESULT_CHECK(result);
        result = this->flash_drv->write(this->addr_state1, &this->state, sizeof(wl_state_t));
        WL_RESULT_CHECK(result);
        result = this->flash_drv->erase_range(this->addr_state2, this->state_size);
        WL_RESULT_CHECK(result);
        result = this->flash_drv->write(this->addr_state2, &this->state, sizeof(wl_state_t));
        WL_RESULT_CHECK(result);
        ESP_LOGD(TAG, "%s - cycle_count= 0x%08x, move_count= 0x%08x, pos= 0x%08x, ", __func__, advanced_state->cycle_count, this->state.move_count, this->state.pos);
    }
    // Save structures to the flash... and check result
    if (result == ESP_OK) {
        ESP_LOGV(TAG, "%s - result= 0x%08x", __func__, result);
    } else {
        ESP_LOGE(TAG, "%s - result= 0x%08x", __func__, result);
    }
    return result;
}

/*
 * F performed in every stage of Feistel network over msb and key for that stage
 * Current implementation: squaring of xor of inputs
 */
uint32_t WL_Advanced::feistelFunction(uint32_t msb, uint32_t key)
{
    ESP_LOGV(TAG, "%s: msb=0x%x, key=0x%x, xor=0x%x, return=0x%x",
             __func__, msb, key, (msb ^ key), ((msb ^ key) * (msb ^ key)));
    return (msb ^ key) * (msb ^ key);
}

/*
 * Perform a randomized 1-to-1 mapping of given address using unbalanced 3-stage Feistel network
 *
 * Given address A which we can split into two parts, msb and lsb (can differ in lengths => unbalanced)
 * and randomly generated key for each stage, we can perform the following:
 *
 *                    stage 1           stage 2             stage 3
 *           msb -------|------\   /------|-------\   /-------|------> final msb
 *          /          \/      | s |     \/       | s |      \/                 \
 *   input /      F(msb, key1) | w | F(msb, key2) | w | F(msb, key3)             \ randomized
 * address \           |       | a |      |       | a |      |                   / address
 *          \         \/       | p |     \/       | p |     \/                  /
 *           lsb --->(XOR)--->/    \--->(XOR)--->/    \--->(XOR)-----> final lsb
 *
 * 1. Split the address to msb and lsb
 * 2. msb remains intact => msb stage output
 * 3. Do F(msb, <key for given stage>)
 * 4. XOR the output of F with lsb => lsb stage output
 * 5. swap lsb <-> msb
 * 6. Assemble address stage output from lsb and msb
 * 7. Repeat steps 1-6 for stage 2 and 3
 * 8. Final address stage output is the randomized address
 */
size_t WL_Advanced::addressFeistelNetwork(size_t addr)
{
    wl_advanced_state_t *advanced_state = (wl_advanced_state_t *)&this->state;
    uint8_t *keys = (uint8_t *)&advanced_state->feistel_keys;

    // mask for only lower lsb bits
    uint32_t LSB_mask = ~( (~(size_t)0) << this->feistel_lsb_width );

    uint32_t msb, lsb, _msb, _lsb, randomized_sector_addr;

round:
    uint32_t sector_addr = addr / this->cfg.sector_size;
    ESP_LOGD(TAG, "%s: sector_addr=0x%x", __func__, sector_addr);

    // 3 stages
    for (uint8_t i = 0; i < 3; i++) {

        // get separated and correctly shifted and masked lsb, msb values from current sector address
        msb = sector_addr >> this->feistel_lsb_width;
        lsb = sector_addr & LSB_mask;

        // msb output of this stage, stays intact
        _msb = msb;

        // lsb output of this stage
        // perform F() on msb and key specific to this stage
        // mask output of function to be |lsb| for XORing with lsb
        _lsb = (lsb ^ (this->feistelFunction(msb, keys[i]) & LSB_mask));

        // assemble address, swapping msb and lsb
        // full output of this stage
        sector_addr = (_lsb << this->feistel_msb_width) | _msb;
        ESP_LOGV(TAG, "%s: msb=0x%x, lsb=0x%x, sector_addr=0x%x", __func__, _msb, _lsb, sector_addr);
    }

    // after 3 stages we get sufficiently randomized address
    randomized_sector_addr = sector_addr;
    ESP_LOGD(TAG, "%s: randomized_sector_addr=0x%x", __func__, randomized_sector_addr);

    uint32_t sector_count = this->flash_size / this->cfg.sector_size;
    /*
     * === IMPORTANT CHECK  ===
     * Feistel can generate addresses using full feistel_bit_width, so 0 to 2^feisteL_bit_width - 1
     * but last few addresses of the partition belong to reserved sectors => those are not valid outputs of mapping
     * If that happens, we need to "cycle walk" => do another round of 3-stage Feistel
     * with the invalid randomized sector address we got as new input
     *
     * From what I understand and tested, this should always lead to a valid output (usually a single cycle walk is enough)
     * Also the prevalence of cycle walks depends on how many sectors out of all in partition are reserved,
     * as the randomization needs to "hit" one of them to require cycle walk
     * Keys play a role in that, some key sets (3 keys for 3 stages) might not cycle walk at all
     */
    if (randomized_sector_addr >= sector_count) {
        ESP_LOGW(TAG, "%s: randomized sector addr 0x%x outside of domain => another round", __func__, randomized_sector_addr);
        addr = randomized_sector_addr * this->cfg.sector_size;
        goto round;
    }

    return randomized_sector_addr * this->cfg.sector_size;
}

size_t WL_Advanced::calcAddr(size_t addr)
{
    // firstly randomize incoming address by using Feistel network
    // it does 1-to-1 mapping of addresses in available sector address space
    size_t intermediate_addr = this->addressFeistelNetwork(addr);

    // with the randomized intermediate address, perform algebraic mapping based on move count
    size_t result = (this->flash_size - this->state.move_count * this->cfg.page_size + intermediate_addr) % this->flash_size;
    // and finally shift resulting physical sector address by one depending on current dummy sector position
    size_t dummy_addr = this->state.pos * this->cfg.page_size;
    if (result >= dummy_addr) {
        result += this->cfg.page_size;
    }
    ESP_LOGV(TAG, "%s - addr= 0x%08x, intermediate_addr=0x%08x -> result= 0x%08x, dummy_addr= 0x%08x",
             __func__, (uint32_t) addr, (uint32_t)intermediate_addr, (uint32_t) result, (uint32_t)dummy_addr);
    return result;
}

esp_err_t WL_Advanced::erase_sector(size_t sector)
{
    esp_err_t result = ESP_OK;
    if (!this->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGV(TAG, "%s - sector= 0x%08x", __func__, (uint32_t) sector);
    // pass sector to updateWl() so it can use it in pos update record
    result = this->updateWL(sector);
    WL_RESULT_CHECK(result);
    size_t virt_addr = this->calcAddr(sector * this->cfg.sector_size);
    result = this->flash_drv->erase_sector((this->cfg.start_addr + virt_addr) / this->cfg.sector_size);
    WL_RESULT_CHECK(result);
    return result;
}

esp_err_t WL_Advanced::write(size_t dest_addr, const void *src, size_t size)
{
    esp_err_t result = ESP_OK;
    if (!this->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGD(TAG, "%s - dest_addr= 0x%08x, size= 0x%08x", __func__, (uint32_t) dest_addr, (uint32_t) size);
    uint32_t count = (size - 1) / this->cfg.page_size;
    for (size_t i = 0; i < count; i++) {
        size_t virt_addr = this->calcAddr(dest_addr + i * this->cfg.page_size);
        result = this->flash_drv->write(this->cfg.start_addr + virt_addr, &((uint8_t *)src)[i * this->cfg.page_size], this->cfg.page_size);
        WL_RESULT_CHECK(result);
    }
    size_t virt_addr_last = this->calcAddr(dest_addr + count * this->cfg.page_size);
    result = this->flash_drv->write(this->cfg.start_addr + virt_addr_last, &((uint8_t *)src)[count * this->cfg.page_size], size - count * this->cfg.page_size);
    WL_RESULT_CHECK(result);
    return result;
}

esp_err_t WL_Advanced::read(size_t src_addr, void *dest, size_t size)
{
    esp_err_t result = ESP_OK;
    if (!this->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGD(TAG, "%s - src_addr= 0x%08x, size= 0x%08x", __func__, (uint32_t) src_addr, (uint32_t) size);
    uint32_t count = (size - 1) / this->cfg.page_size;
    for (size_t i = 0; i < count; i++) {
        size_t virt_addr = this->calcAddr(src_addr + i * this->cfg.page_size);
        ESP_LOGV(TAG, "%s - real_addr= 0x%08x, size= 0x%08x", __func__, (uint32_t) (this->cfg.start_addr + virt_addr), (uint32_t) size);
        result = this->flash_drv->read(this->cfg.start_addr + virt_addr, &((uint8_t *)dest)[i * this->cfg.page_size], this->cfg.page_size);
        WL_RESULT_CHECK(result);
    }
    size_t virt_addr_last = this->calcAddr(src_addr + count * this->cfg.page_size);
    result = this->flash_drv->read(this->cfg.start_addr + virt_addr_last, &((uint8_t *)dest)[count * this->cfg.page_size], size - count * this->cfg.page_size);
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
    result = this->updateWL(this->state.pos);

    wl_advanced_state_t *advanced_state = (wl_advanced_state_t *)&this->state;

    ESP_LOGD(TAG, "%s - result= 0x%08x, cycle_count=0x%08x, move_count= 0x%08x", __func__, result, advanced_state->cycle_count, advanced_state->move_count);
    return result;
}
