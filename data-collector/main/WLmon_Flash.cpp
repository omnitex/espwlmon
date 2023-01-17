#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp32/rom/crc.h"
#include "wlmon.h"

static const char *TAG = "WLmon_FLash";
#ifndef WL_CFG_CRC_CONST
#define WL_CFG_CRC_CONST UINT32_MAX
#endif

WLmon_Flash::WLmon_Flash()
{

}

WLmon_Flash::~WLmon_Flash()
{
    free(this->temp_buff);
}

esp_err_t WLmon_Flash::config(wl_config_t *cfg, Flash_Access *flash_drv)
{
    ESP_LOGI(TAG, "%s start_addr=0x%08x, full_mem_size=0x%08x, page_size=0x%08x, sector_size=0x%08x, updaterate=0x%08x, wr_size=0x%08x, version=0x%08x, temp_buff_size=0x%08x", __func__,
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

    return ESP_OK;
}

esp_err_t WLmon_Flash::init()
{
    return ESP_OK;
}

#if 0
esp_err_t WL_Flash::recoverPos()
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
    return result;
}

void WL_Flash::fillOkBuff(int n)
{
    uint32_t *buff = (uint32_t *)this->temp_buff;

    for (int i = 0 ; i < 4 ; i++) {
        buff[i] = this->state.device_id + n * 4 + i;
        buff[i] = crc32_le(WL_CFG_CRC_CONST, (uint8_t *)&buff[i], sizeof(uint32_t));
    }
}

bool WL_Flash::OkBuffSet(int n)
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

size_t WLmon_Flash::chip_size()
{
    return ESP_OK;
}

size_t WLmon_Flash::sector_size()
{
    return ESP_OK;
}

esp_err_t WLmon_Flash::erase_sector(size_t sector)
{
    return ESP_OK;
}

esp_err_t WLmon_Flash::erase_range(size_t start_address, size_t size)
{
    return ESP_OK;
}

esp_err_t WLmon_Flash::write(size_t dest_addr, const void *src, size_t size)
{
    return ESP_OK;
}

esp_err_t WLmon_Flash::read(size_t src_addr, void *dest, size_t size)
{
    return ESP_OK;
}

esp_err_t WLmon_Flash::flush()
{
    return ESP_OK;
}
#endif