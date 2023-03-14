#ifndef _WL_ADVANCED_H_
#define _WL_ADVANCED_H_

#include "WL_Flash.h"

class WL_Advanced : public WL_Flash
{
public:
    WL_Advanced();
    ~WL_Advanced() override;

    esp_err_t init() override;

    esp_err_t erase_sector(size_t sector) override;

    esp_err_t flush() override;

protected:
    // buffer of 2B numbers for counting per sector erase counts
    // incremented every `updaterate` erases
    // 2^16 * updaterate can surely count erases past memory lifetime
    // required memory: 2B for every sector
    // e.g. at 1MB partition, 4K sectors => 250 sectors => 500B buffer
    uint16_t *erase_count_buffer;
    size_t erase_count_buffer_size;

    size_t erase_count_records_size;
    size_t addr_erase_counts1;
    size_t addr_erase_counts2;

    virtual esp_err_t updateEraseCounts();
    virtual esp_err_t readEraseCounts();

    esp_err_t config(wl_config_t *cfg, Flash_Access *flash_drv);
    esp_err_t updateWL(size_t sector);
    //esp_err_t recoverPos();
    esp_err_t initSections() override;
    void fillOkBuff(int n) override;
    bool OkBuffSet(int n) override;

};

typedef struct WL_Sector_Erase_Record_s {
    uint32_t device_id;
    uint32_t pos;
    uint32_t sector;
    uint32_t crc;
} wl_sector_erase_record_t;

typedef struct WL_Sector_Erase_Pair_s {
    uint16_t sector;
    uint16_t erase_count;
} wl_sector_erase_pair_t;

typedef struct WL_Erase_Count_s {
    union {
        wl_sector_erase_pair_t pairs[3];
    };
    uint32_t crc;
} wl_erase_count_t;

#endif // _WL_ADVANCED_H_