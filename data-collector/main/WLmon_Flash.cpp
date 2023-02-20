#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp32/rom/crc.h"
#include "spi_flash_mmap.h"
#include "wlmon.h"

#define WL_RESULT_CHECK(result) \
    if (result != ESP_OK) { \
        ESP_LOGE(TAG,"%s(%d): result = 0x%08x", __FUNCTION__, __LINE__, result); \
        return (result); \
    }

static const char *TAG = "wlmon";

WLmon_Flash::WLmon_Flash() {}

WLmon_Flash::~WLmon_Flash() {}

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
    if (result != ESP_OK) {
        // non OK code returned by flash_drv->read(), don't know which one, override with own code
        return ESP_ERR_FLASH_OP_FAIL;
    }

    return ESP_OK;
}

void WLmon_Flash::print_wl_config_json()
{
    printf("{");
    printf("\"start_addr\":\"0x%x\",", this->cfg.start_addr);
    printf("\"full_mem_size\":\"0x%x\",", this->cfg.full_mem_size);
    printf("\"page_size\":\"0x%x\",", this->cfg.page_size);
    printf("\"sector_size\":\"0x%x\",", this->cfg.sector_size);
    printf("\"updaterate\":\"0x%x\",", this->cfg.updaterate);
    printf("\"wr_size\":\"0x%x\",", this->cfg.wr_size);
    printf("\"version\":\"0x%x\",", this->cfg.version);
    printf("\"temp_buff_size\":\"0x%x\",", this->cfg.temp_buff_size);
    printf("\"crc\":\"0x%x\"", this->cfg.crc);
    printf("}");
    fflush(stdout);
}

void WLmon_Flash::print_wl_state_json()
{
    printf("{");
    printf("\"pos\":\"0x%x\",", this->state.pos);
    printf("\"max_pos\":\"0x%x\",", this->state.max_pos);
    printf("\"move_count\":\"0x%x\",", this->state.move_count);
    printf("\"access_count\":\"0x%x\",", this->state.access_count);
    printf("\"max_count\":\"0x%x\",", this->state.max_count);
    printf("\"block_size\":\"0x%x\",", this->state.block_size);
    printf("\"version\":\"0x%x\",", this->state.version);
    printf("\"max_count\":\"0x%x\",", this->state.max_count);
    printf("\"device_id\":\"0x%x\",", this->state.device_id);
    printf("\"crc\":\"0x%x\"", this->state.crc);
    printf("}");
    fflush(stdout);
}

void WLmon_Flash::print_wl_status_json()
{
    printf("{");
    printf("\"config\":");

    print_wl_config_json();

    printf(",\"state\":");

    print_wl_state_json();

    // TODO dummy addr in static analysis does not make much sense as it is written in updateWL()?
    //printf(",\"dummy_addr\":\"0x%x\"", this->dummy_addr);

    printf("}\n");
    fflush(stdout);
}
