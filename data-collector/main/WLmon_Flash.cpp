#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "wlmon.h"

static const char *TAG = "WLmon_FLash";

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

    return ESP_OK;
}

esp_err_t WLmon_Flash::init()
{
    return ESP_OK;
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