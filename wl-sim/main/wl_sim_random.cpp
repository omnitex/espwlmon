#include <cassert>
#include <cmath>
#include <random>
#include "esp_log.h"
#include "wl_sim_random.h"
#include "dirty_zipfian_int_distribution.h"

static const char *TAG = "wl-sim-random";

size_t uniform(size_t max_addr)
{
    return rand() % max_addr;

}

size_t constant(size_t max_addr)
{
    return max_addr / 2;
}

size_t zipf(size_t max_addr)
{
    uint32_t max_sector = max_addr / SECTOR_SIZE;

    static std::default_random_engine generator(time(0));
    static dirtyzipf::dirty_zipfian_int_distribution<int> distribution(0, max_sector, 0.99);

    size_t ret = distribution(generator) * SECTOR_SIZE;

    ESP_LOGV(TAG, "%s(%lu)->%lu", __func__, max_addr, ret);
    return ret;
}

size_t block_constant(size_t erase_block)
{
    ESP_LOGV(TAG, "%s(%lu)->%lu", __func__, erase_block, erase_block);
    return erase_block;
}

size_t block_zipf(size_t erase_block)
{
    static std::default_random_engine generator(time(0));
    // start from 1 as block size of 0 is not desired
    static dirtyzipf::dirty_zipfian_int_distribution<int> distribution(1, erase_block, 0.99);

    size_t ret = distribution(generator);
    ESP_LOGV(TAG, "%s(%lu)->%lu", __func__, erase_block, ret);
    return ret;
}
