#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp32/rom/crc.h"
#include "wlmon.h"

static const char *TAG = "wlmon";

WLmon_Flash::WLmon_Flash()
{

}

WLmon_Flash::~WLmon_Flash()
{
    free(this->temp_buff);
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

    this->flash_drv->read(this->addr_state1, &this->state, sizeof(wl_state_t));

    result = checkStateCRC(&this->state);
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

    return result;
}

esp_err_t WLmon_Flash::recoverPos()
{
    esp_err_t result = ESP_OK;
    size_t position = 0;
    ESP_LOGV(TAG, "%s start", __func__);
    for (size_t i = 0; i < this->state.max_pos; i++) {
        bool pos_bits;
        position = i;
        result = this->flash_drv->read(this->addr_state1 + sizeof(wl_state_t) + i * this->cfg.wr_size, this->temp_buff, this->cfg.wr_size);
        pos_bits = this->OkBuffSet(i);
        WL_RESULT_CHECK(result);
        ESP_LOGV(TAG, "%s - check pos: result=0x%08x, position= %i, pos_bits= 0x%08x", __func__, (uint32_t)result, (uint32_t)position, (uint32_t)pos_bits);
        if (pos_bits == false) {
            break; // we have found position
        }
    }

    this->state.pos = position;
    if (this->state.pos == this->state.max_pos) {
        this->state.pos--;
    }

    ESP_LOGD(TAG, "%s - this->state.pos= 0x%08x, position= 0x%08x, result= 0x%08x, max_pos= 0x%08x", __func__, (uint32_t)this->state.pos, (uint32_t)position, (uint32_t)result, (uint32_t)this->state.max_pos);
    ESP_LOGV(TAG, "%s done", __func__);

    // added for WLmon_Flash
    this->dummy_addr = this->cfg.start_addr + this->state.pos * this->cfg.page_size;
    return result;
}

void WLmon_Flash::fillOkBuff(int n)
{
    uint32_t *buff = (uint32_t *)this->temp_buff;

    for (int i = 0 ; i < 4 ; i++) {
        buff[i] = this->state.device_id + n * 4 + i;
        buff[i] = crc32_le(WL_CFG_CRC_CONST, (uint8_t *)&buff[i], sizeof(uint32_t));
    }
}

bool WLmon_Flash::OkBuffSet(int n)
{
    bool result = true;
    uint32_t *data_buff = (uint32_t *)this->temp_buff;
    for (int i = 0 ; i < 4 ; i++) {
        uint32_t data = this->state.device_id + n * 4 + i;
        uint32_t crc = crc32_le(WL_CFG_CRC_CONST, (uint8_t *)&data, sizeof(uint32_t));
        if (crc != data_buff[i]) {
            result = false;
        }
    }
    return result;
}
